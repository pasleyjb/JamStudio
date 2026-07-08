# JamStudio Milestones

## M1 — Application Shell

- [x] Project documentation and architecture
- [x] CMake build system with JUCE
- [x] Cross-platform application entry point
- [x] Main window with transport bar

## M2 — Audio Playback

- [x] Import single audio file
- [x] Play / pause / stop / seek
- [x] Waveform display
- [x] Volume control (per-stem)

## M3 — Stem Separation

- [x] Demucs subprocess integration
- [x] Progress UI during separation
- [x] Load separated stems into mixer
- [x] Fallback: manual stem import (open song as single track)

## M4 — Stem Mixer

- [x] Per-stem mute, solo, volume
- [x] Play any combination of stems
- [x] Master output level
- [x] Stem labels (vocals, drums, bass, other)

## M5 — Metronome

- [x] Click track on beat grid
- [x] Tempo from song detection or manual BPM
- [x] Enable / disable during playback
- [x] Accent on downbeats

## M6 — Project Persistence

- [x] Save project (stems paths, mixer state, tempo)
- [x] Load project
- [x] Recent projects list

## M7 — Notation (Phase 2)

- [x] MusicXML import
- [x] Tab and sheet music display
- [x] Synced scroll during playback
- [x] Synced lyrics from MusicXML (syllable highlight)
- [x] LRC / external lyrics import
- [x] AI vocal transcription to lyrics
- [x] Transcription correction UI (AI lyrics and tab)
- [ ] Manual notation editing

## M8 — Record (Phase 3)

- [x] Audio input capture
- [x] Record over backing
- [x] Export recording (WAV, OGG, MP3 via LAME)
- [x] Load recording as stem track
- [x] Multi-take list with named/timestamped takes
- [x] Overdub without replacing previous takes