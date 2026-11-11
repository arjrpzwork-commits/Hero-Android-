#include <jni.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <cstdlib>
#if defined(__ANDROID__)
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#endif

namespace fs = std::filesystem;
extern "C" const char* hero_engine_call_json(const char*, const char*, const char*, const char*);

namespace {
#if defined(__ANDROID__)
constexpr const char* kRuntimeInfo =
    R"json({"status":"loaded","component":"hero-android-jni-renderer","mediaRendererAvailable":true,"sourceStagingAvailable":true,"mediaProbeAvailable":true,"stage":"direct-jni-engine-call"})json";
#else
constexpr const char* kRuntimeInfo =
    R"json({"status":"loaded","component":"hero-android-jni-renderer-host","mediaRendererAvailable":true,"sourceStagingAvailable":true,"mediaProbeAvailable":false,"stage":"direct-jni-engine-call"})json";
#endif
struct SourceState { std::uint64_t expected=0, written=0, nextSequence=0; fs::path path; bool finished=false; };
std::mutex gMutex;
std::map<std::string, SourceState> gSources;

std::string fromJava(JNIEnv* env, jstring value) {
    if (!value) return {};
    const char* chars=env->GetStringUTFChars(value,nullptr);
    if (!chars) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value,chars);
    return result;
}
bool safeToken(const std::string& s) {
    if (s.empty() || s.size()>128) return false;
    for (unsigned char c:s) if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false;
    return true;
}
std::string key(const std::string& root,const std::string& project,const std::string& source) { return root+"|"+project+"|"+source; }
jstring result(JNIEnv* env,const std::string& value) { return env->NewStringUTF(value.c_str()); }
jstring failure(JNIEnv* env,const char* message) { return result(env,std::string("{\"status\":\"error\",\"message\":\"")+message+"\"}"); }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_webapp_herostudios_HeroNativeRuntime_nativeRuntimeInfo(JNIEnv* env,jclass) { return env->NewStringUTF(kRuntimeInfo); }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_webapp_herostudios_HeroNativeRuntime_nativeRenderAvailable(JNIEnv*,jclass) { return JNI_TRUE; }
extern "C" JNIEXPORT jstring JNICALL
Java_com_webapp_herostudios_HeroNativeRuntime_nativeCall(JNIEnv* env,jclass,jstring methodValue,jstring paramsValue,jstring cacheValue,jstring libraryValue) {
    const auto method=fromJava(env,methodValue), params=fromJava(env,paramsValue), cache=fromJava(env,cacheValue), library=fromJava(env,libraryValue);
    if(method.empty()||cache.empty()||library.empty()) return failure(env,"Invalid direct JNI render request");
    const char* response=hero_engine_call_json(method.c_str(),params.c_str(),cache.c_str(),library.c_str());
    return env->NewStringUTF(response ? response : "{\"error\":{\"code\":-32000,\"message\":\"Native renderer returned no response\"}}");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_webapp_herostudios_HeroNativeRuntime_nativeBeginProject(JNIEnv* env,jclass,jstring rootValue,jstring projectValue) {
    const auto root=fromJava(env,rootValue), project=fromJava(env,projectValue);
    if (!safeToken(project) || root.empty()) return failure(env,"Invalid project identifier or storage root");
    try { fs::create_directories(fs::path(root)/"hero-native"/project); return result(env,"{\"status\":\"ok\",\"projectId\":\""+project+"\"}"); }
    catch (...) { return failure(env,"Unable to create native project directory"); }
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_webapp_herostudios_HeroNativeRuntime_nativeBeginSource(JNIEnv* env,jclass,jstring rootValue,jstring projectValue,jstring sourceValue,jstring nameValue,jlong expected) {
    const auto root=fromJava(env,rootValue), project=fromJava(env,projectValue), source=fromJava(env,sourceValue), name=fromJava(env,nameValue);
    if (root.empty()||!safeToken(project)||!safeToken(source)||name.empty()||name.size()>255||expected<=0) return failure(env,"Invalid source staging request");
    std::lock_guard<std::mutex> lock(gMutex);
    try {
        const auto dir=fs::path(root)/"hero-native"/project; fs::create_directories(dir);
        SourceState state; state.expected=static_cast<std::uint64_t>(expected); state.path=dir/(source+".media");
        std::ofstream file(state.path,std::ios::binary|std::ios::trunc); if(!file) return failure(env,"Unable to create staged media file");
        gSources[key(root,project,source)]=state;
        return result(env,"{\"status\":\"ok\"}");
    } catch (...) { return failure(env,"Unable to initialize source staging"); }
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_webapp_herostudios_HeroNativeRuntime_nativeAppendSourceChunk(JNIEnv* env,jclass,jstring rootValue,jstring projectValue,jstring sourceValue,jlong sequence,jbyteArray data) {
    const auto root=fromJava(env,rootValue), project=fromJava(env,projectValue), source=fromJava(env,sourceValue);
    if (!safeToken(project)||!safeToken(source)||sequence<0||!data) return failure(env,"Invalid source chunk request");
    const jsize length=env->GetArrayLength(data); if(length<=0) return failure(env,"Source chunk must contain 1 byte to 1 MiB");
    std::vector<jbyte> bytes(static_cast<std::size_t>(length)); env->GetByteArrayRegion(data,0,length,bytes.data());
    if(env->ExceptionCheck()) { env->ExceptionClear(); return failure(env,"Unable to read source chunk"); }
    std::lock_guard<std::mutex> lock(gMutex); auto it=gSources.find(key(root,project,source));
    if(it==gSources.end()) return failure(env,"Source staging session not found");
    auto& state=it->second;
    if(state.finished||static_cast<std::uint64_t>(sequence)!=state.nextSequence) return failure(env,"Source chunk sequence is invalid or source is finalized");
    if(state.written+static_cast<std::uint64_t>(length)>state.expected) return failure(env,"Source chunks exceed declared source size");
    std::ofstream file(state.path,std::ios::binary|std::ios::app); file.write(reinterpret_cast<const char*>(bytes.data()),length);
    if(!file) return failure(env,"Failed writing source chunk");
    state.written+=static_cast<std::uint64_t>(length); ++state.nextSequence;
    return result(env,"{\"status\":\"ok\",\"written\":"+std::to_string(state.written)+"}");
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_webapp_herostudios_HeroNativeRuntime_nativeFinishSource(JNIEnv* env,jclass,jstring rootValue,jstring projectValue,jstring sourceValue) {
    const auto root=fromJava(env,rootValue), project=fromJava(env,projectValue), source=fromJava(env,sourceValue);
    if(!safeToken(project)||!safeToken(source)) return failure(env,"Invalid source identifier");
    std::lock_guard<std::mutex> lock(gMutex); auto it=gSources.find(key(root,project,source));
    if(it==gSources.end()) return failure(env,"Source staging session not found");
    auto& state=it->second;
    if(state.written!=state.expected) return failure(env,"Source upload is incomplete; refusing to finalize");
    std::error_code ec; const auto actual=fs::file_size(state.path,ec);
    if(ec||actual!=state.expected) return failure(env,"Staged source size verification failed");
    state.finished=true;
    return result(env,"{\"status\":\"ok\",\"verifiedBytes\":"+std::to_string(state.written)+"}");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_webapp_herostudios_HeroNativeRuntime_nativeProbeSource(JNIEnv* env,jclass,jstring rootValue,jstring projectValue,jstring sourceValue) {
#if defined(__ANDROID__)
    const auto root=fromJava(env,rootValue), project=fromJava(env,projectValue), source=fromJava(env,sourceValue);
    if(!safeToken(project)||!safeToken(source)) return failure(env,"Invalid source identifier");
    fs::path path;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it=gSources.find(key(root,project,source));
        if(it==gSources.end()||!it->second.finished) return failure(env,"Source is not finalized");
        path=it->second.path;
    }
    AMediaExtractor* extractor=AMediaExtractor_new();
    if(!extractor) return failure(env,"Android media extractor allocation failed");
    const media_status_t status=AMediaExtractor_setDataSource(extractor,path.string().c_str());
    if(status!=AMEDIA_OK) { AMediaExtractor_delete(extractor); return failure(env,"Android could not inspect staged media"); }
    std::string json="{\"status\":\"ok\",\"trackCount\":"+std::to_string(AMediaExtractor_getTrackCount(extractor))+",\"tracks\":[";
    const size_t count=AMediaExtractor_getTrackCount(extractor);
    for(size_t i=0;i<count;++i) {
        AMediaFormat* format=AMediaExtractor_getTrackFormat(extractor,i);
        if(!format) continue;
        const char* mime=nullptr; int32_t width=0,height=0,rate=0,channels=0; int64_t duration=0;
        AMediaFormat_getString(format,AMEDIAFORMAT_KEY_MIME,&mime);
        AMediaFormat_getInt32(format,AMEDIAFORMAT_KEY_WIDTH,&width);
        AMediaFormat_getInt32(format,AMEDIAFORMAT_KEY_HEIGHT,&height);
        AMediaFormat_getInt32(format,AMEDIAFORMAT_KEY_SAMPLE_RATE,&rate);
        AMediaFormat_getInt32(format,AMEDIAFORMAT_KEY_CHANNEL_COUNT,&channels);
        AMediaFormat_getInt64(format,AMEDIAFORMAT_KEY_DURATION,&duration);
        if(json.back()!='[') json+=",";
        json+="{\"index\":"+std::to_string(i)+",\"mime\":\""+(mime?mime:"")+"\",\"durationUs\":"+std::to_string(duration)+",\"width\":"+std::to_string(width)+",\"height\":"+std::to_string(height)+",\"sampleRate\":"+std::to_string(rate)+",\"channels\":"+std::to_string(channels)+"}";
        AMediaFormat_delete(format);
    }
    json+="]}";
    AMediaExtractor_delete(extractor);
    return result(env,json);
#else
    (void)rootValue; (void)projectValue; (void)sourceValue;
    return failure(env,"Android Media NDK probe is unavailable on this host");
#endif
}
