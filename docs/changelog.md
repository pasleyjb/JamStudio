# Changelog

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