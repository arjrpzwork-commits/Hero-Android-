# Native host adapter contract

The Android/desktop host adapter is a separate, platform-specific component. It must expose Promise-returning `window.HeroNative` methods matching the C++ JSON-lines protocol:

- `beginProject({project,settings}) -> {projectId,status}`
- `beginSource({projectId,sourceId,name,size,kind})`
- `appendSourceChunk({projectId,sourceId,sequence,base64})`
- `finishSource({projectId,sourceId})`
- `renderProject({projectId,settings}) -> {status,outputUri,outputPath,bytes,requestedSettings,verification,message}`

The host must stage chunks in order, verify exact byte counts, and use app-managed paths. It must not interpolate user-controlled values into shell commands. The renderer response must carry the original requested settings and ffprobe verification metadata through unchanged. The UI independently checks the echoed settings and metadata before accepting completion.

A platform without a compatible native renderer must not fake this API or claim native completion. It must report native rendering unavailable. The current Android `MainActivity` does not yet implement this adapter.
