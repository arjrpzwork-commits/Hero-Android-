# Rebuild decisions
- Keep the original app ID `com.webapp.herostudios`; increase versionCode only.
- Preserve the 0.2 editor and Library archives unchanged; this rebuild is a new package.
- Project files contain metadata/source references, not embedded media. Original media remains user-owned and external.
- Use a strict fail-closed platform contract. Hosts cannot silently alter render settings or claim success without output verification.
- Keep C++ engine source independent of the browser UI. Android/desktop host adapters are separate deliverables and must be tested per platform.
- Do not bundle the local GPL-enabled FFmpeg executable or claim licensing clearance.
- Do not label the Linux executable as an Android binary.
