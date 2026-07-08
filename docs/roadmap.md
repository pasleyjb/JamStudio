# JamStudio Roadmap

## Product Vision

JamStudio is a music practice and learning workstation. Load any song, split it into
playable stems, read along with synced tabs or sheet music, practice with a metronome,
and record yourself — all in one application.

## Phase 1 — Playback Workstation (complete)

**Goal:** Import a song, separate it into stems, mix/solo stems during playback, and
practice with a metronome.

| Feature | Status |
|---|---|
| Application shell (JUCE + CMake) | Done |
| Import audio file (WAV, MP3, FLAC, OGG) | Done |
| Stem separation via Demucs (subprocess) | Done |
| Per-stem mute / solo / volume | Done |
| Transport controls (play, pause, stop, seek) | Done |
| Waveform display with seek | Done |
| Separation progress bar | Done |
| Metronome (tempo from song or manual) | Done |
| Project save / load (stems + mixer state) | Planned |

## Phase 2 — Synced Notation (initial)

**Goal:** Display sheet music and guitar tabs that follow playback in real time.

| Feature | Status |
|---|---|
| MusicXML import | Done |
| Tab and standard notation renderer | Done |
| Playback-synced scroll and highlight | Done |
| Per-instrument notation views | Planned |
| Manual notation editing | Planned |

## Phase 3 — Record (initial)

**Goal:** Record yourself playing along with the backing stems.

| Feature | Status |
|---|---|
| Audio input monitoring | Done |
| Record over backing stems | Done |
| Export recording (WAV) | Done |
| Overdub and multi-take support | Planned |

## Phase 4 — Smart Transcription

**Goal:** Automatically generate tabs and sheet music from separated stems.

| Feature | Status |
|---|---|
| Pitch and onset detection from stems | Planned |
| MIDI extraction per instrument | Planned |
| Auto-generate guitar tab | Planned |
| Auto-generate standard notation | Planned |
| Transcription correction UI | Planned |

## Future (beyond initial product)

- Live performance mode
- Plugin hosting (VST3, AU)
- Multiple cue mixes
- AI-assisted arrangement and composition
- Full multitrack DAW editing
- Cross-platform Qt-based notation UI