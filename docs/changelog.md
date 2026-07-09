# Changelog

## [0.9.6] — 2026-07-08

### Fixed

- Lyrics (Whisper) and tab (basic-pitch) progress stuck at 50%: drain subprocess output during the run (prevents pipe deadlock) and report real / time-based progress to 100%
- Cancel during AI lyrics / tab / separation could hang or kill the whole app: cancel now stops only the child process, ends the progress UI, and leaves JamStudio open

### Changed

- Stem separation defaults to **htdemucs_6s** (6 stems) so **Guitar** and **Piano** are dedicated tracks instead of a muddy "Other" bucket
- Separation quality: Demucs `--shifts 2` for cleaner stems (slightly slower)
- Mixer orders stems guitar-first for practice (Guitar, Bass, Drums, Vocals, Piano, Other)
- AI tab generation prefers the Guitar stem when available
- Friendly stem labels: Guitar, Bass, Drums, Vocals, Piano
- **UI redesign (practice DAW):** lyrics top (karaoke highlight), tabs middle, main waveform + stem lanes bottom
- **Floating mixer window** with vertical faders, M/S buttons, and per-stem colours
- View toggles (Lyrics / Tabs / Stems / Mixer) + View menu — panels available as soon as a song loads
- Cooler pro-DAW dark theme, brighter playhead, section headers
- Mixer channels **scale to fit the mixer window** (no fixed strip size / horizontal scroll)
- Primary lyrics path is **online synced LRC** (lrclib.net) via song metadata, with multi-match picker

### Added

- `StemType::guitar` and `StemType::piano` with filename detection from Demucs output
- `MixerWindow`, `MixerChannelStrip`, `StemLane` UI modules
- `OnlineLyricsClient` + `OnlineLyricsDialog` + `SongMetadata` extraction
- Toolbar / menu: **Online Lyrics** (Find Synced Lyrics Online)

## [0.9.5] — 2026-07-08

### Added

- Tab / Sheet toggle for notation (toolbar, menu, and header bar)
- MusicXML lyrics auto-populate the synced lyrics panel
- Sample demo files in `samples/` for quick notation and lyrics testing

## [0.9.4] — 2026-07-08

### Fixed

- Demucs setup docs and error hint for missing `torchcodec` dependency (required by recent torchaudio)

## [0.9.3] — 2026-07-08

### Added

- Transcription correction dialog after AI lyrics and AI tab generation
- Edit lyric lines (timestamps preserved) before applying to the project
- Edit tab title, tempo, and per-measure fret/string notes before saving

## [0.9.2] — 2026-07-08

### Added

- Multi-take recording: each take is named (e.g. "Take 1 — 14:32:05") and kept in the mixer
- Overdub support — previous takes stay audible while recording new ones
- Automatic pruning of oldest takes beyond 8 per session/project

## [0.9.1] — 2026-07-08

### Added

- Per-stem mini waveforms with playback cursor in the mixer strip
- Master output volume slider in the transport bar
- Tempo detection from audio (auto on song load, manual via Transport → Detect Tempo)
- `masterVolume` persisted in project files (backward compatible)

## [0.9.0] — 2026-07-08

### Added

- Help → AI Tools Setup dialog with install guidance for Demucs, Whisper, and basic-pitch
- Embedded AI lyrics and scores in `.jamstudio` when no external file is linked
- Cancel button on separation and AI job progress bars

### Fixed

- AI tool availability now verified with `--help` probe (fixes false-positive `python3` detection)
- Menu bar crash on window close (separate `AppMenuBar` component)

## [0.8.0] — 2026-07-08

### Added

- Audacity-style UI with system light/dark theme
- Tabbed toolbar and 3D LED indicator buttons
- Scrolling lyrics viewport

## [0.7.0] — 2026-07-08

### Added

- AI lyrics from vocals (Whisper subprocess)
- Word-level karaoke highlighting
- AI guitar tab generation (basic-pitch subprocess)

## [0.6.0] — 2026-07-08

### Added

- LRC lyrics import with karaoke-style line highlighting during playback
- Lyrics panel works alongside tabs, sheet music, or on its own with any song
- Project files now save and restore linked LRC lyrics paths

## [0.5.0] — 2026-07-08

### Added

- Synced lyrics from MusicXML with syllable-level highlight during playback
- Lyrics row displayed below tabs and standard notation
- Recent projects menu for quick session restore

## [0.4.0] — 2026-07-08

### Added

- Project save/load (`.jamstudio` JSON format)
- Persists stems, mixer state, metronome, transport position, and score path

## [0.3.0] — 2026-07-08

### Added

- Compressed `.mxl` MusicXML import via ZIP extraction
- MusicXML tuplets, dotted notes, ties, chords, grace notes, backup/forward voices
- Tempo map with mid-score tempo changes for synced notation
- Tab data from `<technical>` and `<play>` elements with smarter fret estimation fallback
- Recording export to OGG (always) and MP3 (when `lame` is installed)
- Recordings automatically loaded as a new stem track in the mixer

### Fixed

- Notation sync now accounts for tempo changes throughout a score
- Tab positions prefer explicit string/fret data over pitch estimation

## [0.2.0] — 2026-07-08

### Added

- Waveform display with click-to-seek and playback cursor
- Separation progress bar with live Demucs percentage parsing
- MusicXML import with standard notation and guitar tab rendering
- Playback-synced notation scroll and measure highlighting
- Audio recording over backing stems (saved to Documents/JamStudio/Recordings)

## [0.1.0] — 2026-07-08

### Added

- CMake build system with JUCE 8
- Phase 1 application shell with main window
- Audio file import (WAV, MP3, FLAC, OGG, AIFF)
- Multi-stem mixer with per-stem mute, solo, and volume
- Shared transport (play, pause, stop, seek)
- Metronome with adjustable BPM
- Demucs stem separation integration (external subprocess)
- Product roadmap, milestones, and architecture decisions