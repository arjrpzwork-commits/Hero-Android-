#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <fcntl.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using boost::property_tree::ptree;

static std::string make_id() {
    static std::uint64_t n = 0;
    return "hero-project-" + std::to_string(static_cast<unsigned long long>(::getpid())) + "-" + std::to_string(++n);
}
static std::string json_string(const ptree& p, const std::string& key, const std::string& fallback = "") {
    return p.get<std::string>(key, fallback);
}
static double number(const ptree& p, const std::string& key, double fallback = 0.0) {
    return p.get<double>(key, fallback);
}
static bool boolean(const ptree& p, const std::string& key, bool fallback = false) {
    return p.get<bool>(key, fallback);
}
static std::string quote_filter(double x) {
    std::ostringstream s; s.setf(std::ios::fixed); s.precision(9); s << x; return s.str();
}
static std::vector<unsigned char> decode64(const std::string& in) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::array<int, 256> table{}; table.fill(-1);
    for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(chars[static_cast<std::size_t>(i)])] = i;
    std::vector<unsigned char> out; int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        if (table[c] < 0) throw std::runtime_error("Invalid base64 source chunk");
        val = (val << 6) + table[c]; bits += 6;
        if (bits >= 0) { out.push_back(static_cast<unsigned char>((val >> bits) & 0xff)); bits -= 8; }
    }
    return out;
}
struct Source { std::string id, name, kind; std::uint64_t expected = 0; std::uint64_t written = 0; std::uint64_t nextSequence = 0; fs::path path; bool finished = false; };
struct Project { std::string id; ptree snapshot, settings; fs::path dir; std::map<std::string, Source> sources; };

class Engine {
    fs::path root_ = fs::temp_directory_path() / "hero-native-engine";
    std::map<std::string, Project> projects_;
    std::string ffmpeg_ = "ffmpeg";
    std::string ffprobe_ = "ffprobe";

    static std::string safe_name(std::string s) {
        for (char& c : s) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')) c = '_';
        if (s.empty()) s = "source.bin";
        return s;
    }
    static int run_process(const std::vector<std::string>& args, const fs::path& logPath) {
        if (args.empty()) throw std::runtime_error("Empty process command");
        pid_t pid = ::fork();
        if (pid < 0) throw std::runtime_error("Unable to start FFmpeg process");
        if (pid == 0) {
            int fd = ::open(logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd >= 0) { ::dup2(fd, STDERR_FILENO); ::close(fd); }
            std::vector<char*> argv; argv.reserve(args.size() + 1);
            for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
            argv.push_back(nullptr); ::execvp(argv[0], argv.data()); _exit(127);
        }
        int status = 0;
        while (::waitpid(pid, &status, 0) < 0) { if (errno != EINTR) throw std::runtime_error("Failed waiting for FFmpeg"); }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::ifstream f(logPath); std::ostringstream msg; msg << f.rdbuf();
            std::string detail = msg.str(); if (detail.size() > 2500) detail = detail.substr(detail.size() - 2500);
            throw std::runtime_error("FFmpeg failed (exit " + std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1) + "): " + detail);
        }
        return 0;
    }
    static std::string video_encoder(const std::string& c) {
        if (c == "h264") return "libx264";
        if (c == "hevc") return "libx265";
        if (c == "av1") return "libaom-av1";
        if (c == "vp9") return "libvpx-vp9";
        if (c == "prores") return "prores_ks";
        if (c == "ffv1") return "ffv1";
        throw std::runtime_error("Unsupported video codec: " + c);
    }
    static std::string audio_encoder(const std::string& c, int bits) {
        if (c == "aac") return "aac";
        if (c == "opus") return "libopus";
        if (c == "mp3") return "libmp3lame";
        if (c == "flac") return "flac";
        if (c == "pcm") {
            if (bits == 16) return "pcm_s16le";
            if (bits == 24) return "pcm_s24le";
            if (bits == 32) return "pcm_s32le";
            throw std::runtime_error("PCM bit depth must be 16, 24, or 32");
        }
        throw std::runtime_error("Unsupported audio codec: " + c);
    }
    static std::string container_ext(const std::string& c) {
        if (c == "mp4" || c == "mov" || c == "mkv" || c == "webm" || c == "avi" || c == "wav" || c == "flac") return c;
        throw std::runtime_error("Unsupported output container: " + c);
    }
    static void validate(const ptree& s) {
        const auto container = json_string(s, "container", "mp4");
        const bool video = boolean(s, "video", true);
        const int w = s.get<int>("width", 1280), h = s.get<int>("height", 720);
        const int rate = s.get<int>("audio_sample_rate", 48000), bits = s.get<int>("audio_bit_depth", 24);
        const int fn = s.get<int>("fps_num", 30), fd = s.get<int>("fps_den", 1);
        const auto vb = s.get<std::uint64_t>("video_bitrate", 24000000);
        const auto ab = s.get<std::uint64_t>("audio_bitrate", 320000);
        const double gain = number(s, "master_gain", 1.0), duration = number(s, "duration", 0.0);
        // Validate format correctness only. Do not impose product-level export ceilings.
        // Practical limits come from the selected encoder/container and available server storage.
        if (w < 2 || h < 2 || (w % 2) || (h % 2)) throw std::runtime_error("Video dimensions must be positive even numbers");
        if (rate < 1) throw std::runtime_error("Audio sample rate must be a positive integer");
        if (bits != 16 && bits != 24 && bits != 32) throw std::runtime_error("PCM audio bit depth must be 16, 24, or 32");
        if (fn < 1 || fd < 1) throw std::runtime_error("Frame rate numerator and denominator must be positive");
        if (vb < 1 || ab < 1) throw std::runtime_error("Requested bitrates must be positive");
        if (!std::isfinite(gain) || gain < 0 || gain > 16) throw std::runtime_error("Master gain must be between 0 and 16");
        if (!std::isfinite(duration) || duration <= 0) throw std::runtime_error("Project duration must be a positive finite value");
        if (video && (container == "wav" || container == "flac")) throw std::runtime_error("Selected container is audio-only; choose a video container or disable video");
        if (!video && container != "wav" && container != "flac") throw std::runtime_error("Audio-only projects require WAV or FLAC container");
        if (video) (void)video_encoder(json_string(s, "video_codec", "h264"));
        const auto ac = json_string(s, "audio_codec", video ? "aac" : (container == "flac" ? "flac" : "pcm"));
        if (container == "wav" && ac != "pcm") throw std::runtime_error("WAV requires PCM audio codec");
        if (container == "flac" && ac != "flac") throw std::runtime_error("FLAC container requires FLAC audio codec");
        if (container == "webm" && ac != "opus") throw std::runtime_error("WebM requires Opus audio in this engine build");
        if (container == "webm" && json_string(s, "video_codec", "vp9") != "vp9" && json_string(s, "video_codec", "vp9") != "av1") throw std::runtime_error("WebM supports VP9 or AV1 in this engine build");
        (void)audio_encoder(ac, bits);
    }
    static std::string input_path(const Project& p, const std::string& id) {
        auto it = p.sources.find(id); if (it == p.sources.end() || !it->second.finished) throw std::runtime_error("Missing or unfinished source media: " + id);
        return it->second.path.string();
    }
    ptree probe(const fs::path& file, const fs::path& log) {
        int pipefd[2];
        if (::pipe(pipefd) != 0) throw std::runtime_error("Unable to create ffprobe output pipe");
        pid_t pid = ::fork();
        if (pid < 0) { ::close(pipefd[0]); ::close(pipefd[1]); throw std::runtime_error("Unable to start ffprobe"); }
        if (pid == 0) {
            ::dup2(pipefd[1], STDOUT_FILENO); ::close(pipefd[0]); ::close(pipefd[1]);
            int fd = ::open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd >= 0) { ::dup2(fd, STDERR_FILENO); ::close(fd); }
            const std::vector<std::string> args{ffprobe_, "-v", "error", "-show_streams", "-show_format", "-of", "json", file.string()};
            std::vector<char*> argv; for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str())); argv.push_back(nullptr);
            ::execvp(argv[0], argv.data()); _exit(127);
        }
        ::close(pipefd[1]); std::string output; std::array<char,8192> buffer{}; ssize_t n;
        while ((n = ::read(pipefd[0], buffer.data(), buffer.size())) > 0) output.append(buffer.data(), static_cast<std::size_t>(n));
        ::close(pipefd[0]); int status=0; while (::waitpid(pid,&status,0)<0) { if(errno!=EINTR) throw std::runtime_error("Failed waiting for ffprobe"); }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) throw std::runtime_error("ffprobe could not verify rendered output; refusing to report success");
        std::istringstream json(output); ptree result; boost::property_tree::read_json(json, result); return result;
    }
    static std::pair<long long,long long> rational(const std::string& value) {
        const auto slash = value.find('/'); if (slash == std::string::npos) return {0,1};
        try { return {std::stoll(value.substr(0,slash)), std::stoll(value.substr(slash+1))}; } catch (...) { return {0,1}; }
    }
    static std::string expected_audio_codec(const std::string& c, int bits) {
        if (c == "pcm") return bits == 16 ? "pcm_s16le" : bits == 24 ? "pcm_s24le" : "pcm_s32le";
        if (c == "opus") return "opus";
        if (c == "mp3") return "mp3";
        if (c == "flac") return "flac";
        return c;
    }
    static std::string expected_video_codec(const std::string& c) {
        if (c == "h264") return "h264";
        if (c == "hevc") return "hevc";
        if (c == "av1") return "av1";
        if (c == "vp9") return "vp9";
        if (c == "prores") return "prores";
        return "ffv1";
    }
    static void verify_output(const ptree& meta, const ptree& settings) {
        const bool video = boolean(settings,"video",true); bool gotVideo=false, gotAudio=false;
        const int bits=settings.get<int>("audio_bit_depth",24), rate=settings.get<int>("audio_sample_rate",48000);
        const std::string ac=json_string(settings,"audio_codec",video?"aac":"pcm"), vc=json_string(settings,"video_codec","h264");
        const int width=settings.get<int>("width",1280), height=settings.get<int>("height",720), fn=settings.get<int>("fps_num",30), fd=settings.get<int>("fps_den",1);
        for (const auto& item : meta.get_child("streams")) {
            const auto& st=item.second; const auto type=json_string(st,"codec_type");
            if (type=="video") {
                gotVideo=true;
                if (json_string(st,"codec_name")!=expected_video_codec(vc)) throw std::runtime_error("Output verification failed: video codec differs from the requested codec");
                if (st.get<int>("width",0)!=width || st.get<int>("height",0)!=height) throw std::runtime_error("Output verification failed: rendered dimensions differ from the requested dimensions");
                const auto fps=rational(json_string(st,"avg_frame_rate"));
                if (fps.second==0 || std::llabs(fps.first*fd-static_cast<long long>(fn)*fps.second)>std::max(1LL,static_cast<long long>(fd))) throw std::runtime_error("Output verification failed: encoded frame rate differs from the requested frame rate");
            } else if (type=="audio") {
                gotAudio=true;
                if (json_string(st,"codec_name")!=expected_audio_codec(ac,bits)) throw std::runtime_error("Output verification failed: audio codec differs from the requested codec");
                if (st.get<int>("sample_rate",0)!=rate) throw std::runtime_error("Output verification failed: audio sample rate differs from the requested sample rate");
                if (ac=="pcm") {
                    const int actual=st.get<int>("bits_per_sample",st.get<int>("bits_per_raw_sample",0));
                    if (actual!=bits) throw std::runtime_error("Output verification failed: PCM bit depth differs from the requested bit depth");
                }
            }
        }
        if (video && !gotVideo) throw std::runtime_error("Output verification failed: no video stream was produced");
        if (!gotAudio) throw std::runtime_error("Output verification failed: no audio stream was produced");
    }
public:
    Engine() { fs::create_directories(root_); }
    ptree begin_project(const ptree& req) {
        Project p; p.id = make_id(); p.snapshot = req.get_child("project"); p.settings = req.get_child("settings"); validate(p.settings);
        p.dir = root_ / p.id; fs::create_directories(p.dir); const auto id = p.id; projects_.emplace(id, std::move(p));
        ptree r; r.put("projectId", id); r.put("status", "ok"); return r;
    }
    ptree begin_source(const ptree& req) {
        auto& p = project(req.get<std::string>("projectId")); Source s;
        s.id = req.get<std::string>("sourceId"); s.name = req.get<std::string>("name", "source.bin"); s.kind = req.get<std::string>("kind", "video");
        s.expected = req.get<std::uint64_t>("size"); s.path = p.dir / (safe_name(s.id) + "-" + safe_name(s.name));
        if (s.expected == 0) throw std::runtime_error("Source file size must be positive");
        std::ofstream(s.path, std::ios::binary | std::ios::trunc).close(); p.sources[s.id] = s;
        ptree r; r.put("status", "ok"); return r;
    }
    ptree append_source(const ptree& req) {
        auto& p = project(req.get<std::string>("projectId")); auto& s = source(p, req.get<std::string>("sourceId"));
        if (s.finished) throw std::runtime_error("Source already finalized");
        const auto seq = req.get<std::uint64_t>("sequence"); if (seq != s.nextSequence) throw std::runtime_error("Out-of-order or duplicate source chunk");
        const auto bytes = decode64(req.get<std::string>("base64"));
        if (s.written + bytes.size() > s.expected) throw std::runtime_error("Source chunks exceed declared file size");
        std::ofstream f(s.path, std::ios::binary | std::ios::app); f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!f) throw std::runtime_error("Failed writing source chunk");
        s.written += bytes.size(); ++s.nextSequence;
        ptree r; r.put("status", "ok"); r.put("written", s.written); return r;
    }
    ptree finish_source(const ptree& req) {
        auto& p = project(req.get<std::string>("projectId")); auto& s = source(p, req.get<std::string>("sourceId"));
        if (s.written != s.expected || fs::file_size(s.path) != s.expected) throw std::runtime_error("Source upload is incomplete; refusing to render");
        s.finished = true; ptree r; r.put("status", "ok"); r.put("verifiedBytes", s.written); return r;
    }
    ptree render(const ptree& req) {
        auto& p = project(req.get<std::string>("projectId")); const auto settings = req.get_child("settings", p.settings); validate(settings);
        const bool video = boolean(settings, "video", true); const int width = settings.get<int>("width"), height = settings.get<int>("height");
        const int rate = settings.get<int>("audio_sample_rate"), bits = settings.get<int>("audio_bit_depth");
        const int fn = settings.get<int>("fps_num"), fd = settings.get<int>("fps_den");
        const double duration = number(settings, "duration"), master = number(settings, "master_gain", 1.0);
        const std::string container = json_string(settings, "container", "mp4"), ac = json_string(settings, "audio_codec", "aac");
        const std::string vc = json_string(settings, "video_codec", "h264");
        std::vector<std::string> args{ffmpeg_, "-hide_banner", "-nostdin", "-y"};
        std::map<std::string, int> inputIndex; int index = 0;
        for (const auto& [id, s] : p.sources) { if (!s.finished) throw std::runtime_error("Unfinished source upload: " + id); args.push_back("-i"); args.push_back(s.path.string()); inputIndex[id] = index++; }
        std::ostringstream graph; bool haveGraph = false; std::string videoLabel;
        if (video) {
            graph << "color=c=black:s=" << width << "x" << height << ":r=" << fn << "/" << fd << ":d=" << quote_filter(duration) << "[vbase];";
            std::string current = "vbase";
            const auto& clips = p.snapshot.get_child("clips");
            std::vector<const ptree*> vclips;
            for (const auto& item : clips) if (json_string(item.second, "kind") == "video") vclips.push_back(&item.second);
            std::stable_sort(vclips.begin(), vclips.end(), [](const ptree* a, const ptree* b) { return number(*a,"track") < number(*b,"track"); });
            int n = 0;
            for (const auto* c : vclips) {
                const double start = number(*c,"start"), in = number(*c,"in"), out = number(*c,"out"), len = out - in;
                if (len <= 0 || start >= duration || start + len <= 0) continue;
                const int ix = inputIndex.at(json_string(*c,"source")); const std::string label = "vclip" + std::to_string(n), next = "vcomp" + std::to_string(n);
                graph << "[" << ix << ":v:0]trim=start=" << quote_filter(in) << ":duration=" << quote_filter(len)
                      << ",setpts=PTS-STARTPTS,scale=" << width << ":" << height << ":force_original_aspect_ratio=decrease,pad=" << width << ":" << height
                      << ":(ow-iw)/2:(oh-ih)/2,setsar=1,fps=" << fn << "/" << fd << ",format=yuv420p[" << label << "];"
                      << "[" << current << "][" << label << "]overlay=0:0:eof_action=pass:shortest=0:enable='between(t," << quote_filter(start) << "," << quote_filter(std::min(duration,start+len)) << ")'[" << next << "];";
                current = next; ++n;
            }
            videoLabel = current; graph << "[" << videoLabel << "]trim=duration=" << quote_filter(duration) << ",setpts=PTS-STARTPTS[vout];"; haveGraph = true;
        }
        std::vector<std::string> audioLabels; int ai = 0;
        const auto& clips = p.snapshot.get_child("clips");
        for (const auto& item : clips) {
            const auto& c = item.second; const std::string kind = json_string(c,"kind");
            if (boolean(c,"mute",false)) continue;
            const bool isVideo = kind == "video";
            if (isVideo && !boolean(settings,"include_original_audio",true)) continue;
            if (!isVideo && kind != "audio") continue;
            const double start = number(c,"start"), in = number(c,"in"), out = number(c,"out"), len = out-in;
            if (len <= 0 || start >= duration) continue;
            const int ix = inputIndex.at(json_string(c,"source")); const double gain = number(c,"gain",1.0)*master;
            const std::string label = "a" + std::to_string(ai++);
            graph << "[" << ix << ":a:0]atrim=start=" << quote_filter(in) << ":duration=" << quote_filter(len)
                  << ",asetpts=PTS-STARTPTS,aresample=" << rate << ",aformat=sample_fmts=fltp:channel_layouts=stereo,volume=" << quote_filter(gain);
            const double fi = number(c,"fadeIn",0), fo = number(c,"fadeOut",0);
            if (fi > 0) graph << ",afade=t=in:st=0:d=" << quote_filter(std::min(fi,len));
            if (fo > 0) graph << ",afade=t=out:st=" << quote_filter(std::max(0.0,len-fo)) << ":d=" << quote_filter(std::min(fo,len));
            graph << ",adelay=delays=" << static_cast<long long>(std::llround(start*1000.0)) << ":all=1,apad=whole_dur=" << quote_filter(duration) << ",atrim=duration=" << quote_filter(duration) << "[" << label << "];";
            audioLabels.push_back(label); haveGraph = true;
        }
        if (audioLabels.empty()) {
            graph << "anullsrc=r=" << rate << ":cl=stereo,atrim=duration=" << quote_filter(duration) << "[aout];";
        } else if (audioLabels.size() == 1) {
            graph << "[" << audioLabels.front() << "]anull[aout];";
        } else {
            for (const auto& l : audioLabels) graph << "[" << l << "]";
            graph << "amix=inputs=" << audioLabels.size() << ":duration=longest:dropout_transition=0:normalize=0,atrim=duration=" << quote_filter(duration) << "[aout];";
        }
        haveGraph = true;
        if (!haveGraph) throw std::runtime_error("No render graph could be constructed");
        args.insert(args.end(), {"-filter_complex", graph.str()});
        if (video) {
            args.insert(args.end(), {"-map", "[vout]", "-c:v", video_encoder(vc), "-r", std::to_string(fn) + "/" + std::to_string(fd)});
            if (vc == "prores") {
                args.insert(args.end(), {"-profile:v", "3", "-pix_fmt", "yuv422p10le"});
            } else {
                args.insert(args.end(), {"-pix_fmt", "yuv420p"});
            }
            if (vc != "prores" && vc != "ffv1") args.insert(args.end(), {"-b:v", std::to_string(settings.get<std::uint64_t>("video_bitrate"))});
            if (vc == "h264") args.insert(args.end(), {"-preset", "slow"});
            if (vc == "hevc") args.insert(args.end(), {"-preset", "slow", "-x265-params", "log-level=error"});
            if (vc == "av1") args.insert(args.end(), {"-cpu-used", "4"});
            if (vc == "vp9") args.insert(args.end(), {"-deadline", "good", "-cpu-used", "2"});
        }
        args.insert(args.end(), {"-map", "[aout]", "-c:a", audio_encoder(ac,bits), "-ar", std::to_string(rate)});
        if (ac == "aac" || ac == "libopus" || ac == "libmp3lame") args.insert(args.end(), {"-b:a", std::to_string(settings.get<std::uint64_t>("audio_bitrate"))});
        if (container == "mp4" || container == "mov") args.insert(args.end(), {"-movflags", "+faststart"});
        if (container == "webm") args.insert(args.end(), {"-f", "webm"});
        const fs::path output = p.dir / ("Hero-Export." + container);
        args.push_back(output.string());
        const fs::path log = p.dir / "ffmpeg.log";
        (void)run_process(args, log);
        const auto metadata = probe(output, p.dir / "ffprobe.log");
        verify_output(metadata, settings);
        ptree r; r.put("status", "ok");
        std::string message = "Native FFmpeg render completed; output metadata was probed.";
        if (video && (vc == "prores" || vc == "ffv1")) message += " The selected codec uses its profile/lossless mode; the Mbps target does not apply to this codec.";
        r.put("message", message); r.put("outputUri", output.string()); r.put("outputPath", output.string());
        r.put("bytes", fs::file_size(output)); r.add_child("verification", metadata); r.add_child("requestedSettings", settings); return r;
    }
    ptree handle(const ptree& req) {
        const auto method = req.get<std::string>("method"); const auto& params = req.get_child("params");
        if (method == "beginProject") return begin_project(params);
        if (method == "beginSource") return begin_source(params);
        if (method == "appendSourceChunk") return append_source(params);
        if (method == "finishSource") return finish_source(params);
        if (method == "renderProject") return render(params);
        throw std::runtime_error("Unknown engine method: " + method);
    }
private:
    Project& project(const std::string& id) { auto it=projects_.find(id); if(it==projects_.end()) throw std::runtime_error("Unknown project id"); return it->second; }
    Source& source(Project& p,const std::string& id) { auto it=p.sources.find(id); if(it==p.sources.end()) throw std::runtime_error("Unknown source id"); return it->second; }
};

int main() {
    Engine engine; std::string line;
    while (std::getline(std::cin, line)) {
        try {
            std::istringstream in(line); ptree req; boost::property_tree::read_json(in, req);
            const auto id = req.get<std::string>("id", ""); ptree response; response.put("id", id);
            try { response.add_child("result", engine.handle(req)); }
            catch (const std::exception& e) { ptree err; err.put("code", -32000); err.put("message", e.what()); response.add_child("error", err); }
            boost::property_tree::write_json(std::cout, response, false); std::cout.flush();
        } catch (const std::exception& e) {
            ptree response; response.put("id", ""); ptree err; err.put("code", -32700); err.put("message", e.what()); response.add_child("error", err);
            boost::property_tree::write_json(std::cout, response, false); std::cout.flush();
        }
    }
    return 0;
}
