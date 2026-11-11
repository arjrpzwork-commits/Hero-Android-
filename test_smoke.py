#!/usr/bin/env python3
import argparse
import base64
import json
import os
import subprocess
import tempfile


def call(proc, method, params):
    request = {"id": method, "method": method, "params": params}
    proc.stdin.write(json.dumps(request) + "\n")
    proc.stdin.flush()
    line = proc.stdout.readline()
    if not line:
        raise RuntimeError("Native engine closed its response stream")
    response = json.loads(line)
    if "error" in response:
        raise RuntimeError(response["error"].get("message", "Engine request failed"))
    return response["result"]


def stage_source(proc, project_id, source_id, path, kind):
    size = os.path.getsize(path)
    call(proc, "beginSource", {"projectId": project_id, "sourceId": source_id,
                                "name": os.path.basename(path), "size": size, "kind": kind})
    with open(path, "rb") as f:
        sequence = 0
        while True:
            chunk = f.read(256 * 1024)
            if not chunk:
                break
            call(proc, "appendSourceChunk", {"projectId": project_id, "sourceId": source_id,
                                              "sequence": sequence, "base64": base64.b64encode(chunk).decode("ascii")})
            sequence += 1
    result = call(proc, "finishSource", {"projectId": project_id, "sourceId": source_id})
    assert result["verifiedBytes"] == str(size)


def probe(path):
    return json.loads(subprocess.check_output([
        "ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", path
    ], text=True))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True)
    args = parser.parse_args()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    video_source = os.path.join(root, "tests", "fixtures", "test-source.mp4")
    audio_source = os.path.join(root, "tests", "fixtures", "test-tone.wav")
    for path in (video_source, audio_source):
        if not os.path.isfile(path):
            raise SystemExit("Missing test fixture: " + path)

    proc = subprocess.Popen([args.engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, bufsize=1)
    try:
        video_id = "video-fixture"
        video_project = {
            "format": "HeroBrowserSource", "version": 2,
            "trackCounts": {"video": 1, "audio": 1},
            "clips": [{"id": "v1", "source": video_id, "kind": "video", "track": 0,
                       "start": 0, "in": 0, "out": 2, "gain": 1, "mute": False,
                       "fadeIn": 0, "fadeOut": 0}],
            "sources": [{"id": video_id, "name": "test-source.mp4", "size": os.path.getsize(video_source),
                         "kind": "video", "duration": 2}], "settings": {}, "original": True
        }
        video_settings = {"container": "mp4", "video": True, "video_codec": "h264", "audio_codec": "aac",
                          "width": 640, "height": 360, "fps_num": 30, "fps_den": 1,
                          "video_bitrate": 3000000, "audio_bitrate": 192000, "audio_sample_rate": 48000,
                          "audio_bit_depth": 24, "master_gain": 1.0, "include_original_audio": True, "duration": 2.0}
        result = call(proc, "beginProject", {"project": video_project, "settings": video_settings})
        pid = result["projectId"]
        stage_source(proc, pid, video_id, video_source, "video")
        rendered = call(proc, "renderProject", {"projectId": pid, "settings": video_settings})
        streams = probe(rendered["outputPath"])["streams"]
        v = next(s for s in streams if s["codec_type"] == "video")
        a = next(s for s in streams if s["codec_type"] == "audio")
        assert (v["codec_name"], int(v["width"]), int(v["height"]), v["avg_frame_rate"]) == ("h264", 640, 360, "30/1")
        assert (a["codec_name"], int(a["sample_rate"])) == ("aac", 48000)
        print("PASS: native offline MP4 H.264/AAC render + metadata verification")

        audio_id = "audio-fixture"
        audio_project = {
            "format": "HeroBrowserSource", "version": 2,
            "trackCounts": {"video": 0, "audio": 1},
            "clips": [{"id": "a1", "source": audio_id, "kind": "audio", "track": 0,
                       "start": 0.1, "in": 0, "out": 1.25, "gain": 0.8, "mute": False,
                       "fadeIn": 0.05, "fadeOut": 0.05}],
            "sources": [{"id": audio_id, "name": "test-tone.wav", "size": os.path.getsize(audio_source),
                         "kind": "audio", "duration": 1.25}], "settings": {}, "original": False
        }
        audio_settings = {"container": "wav", "video": False, "video_codec": "h264", "audio_codec": "pcm",
                          "width": 640, "height": 360, "fps_num": 30, "fps_den": 1,
                          "video_bitrate": 3000000, "audio_bitrate": 192000, "audio_sample_rate": 96000,
                          "audio_bit_depth": 24, "master_gain": 1.0, "include_original_audio": False, "duration": 1.5}
        result = call(proc, "beginProject", {"project": audio_project, "settings": audio_settings})
        pid = result["projectId"]
        stage_source(proc, pid, audio_id, audio_source, "audio")
        rendered = call(proc, "renderProject", {"projectId": pid, "settings": audio_settings})
        stream = probe(rendered["outputPath"])["streams"][0]
        assert (stream["codec_name"], int(stream["sample_rate"]), int(stream["bits_per_sample"])) == ("pcm_s24le", 96000, 24)
        print("PASS: native 96 kHz / 24-bit PCM WAV render + metadata verification")
    finally:
        proc.stdin.close()
        proc.wait(timeout=30)
        if proc.returncode != 0:
            stderr = proc.stderr.read()
            raise RuntimeError("Native engine exited unexpectedly: " + stderr[-2000:])


if __name__ == "__main__":
    main()
