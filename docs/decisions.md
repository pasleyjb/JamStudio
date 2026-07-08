# Architecture Decisions

## ADR-001: JUCE for audio and initial UI

**Date:** 2026-07-08

**Context:** JamStudio needs cross-platform audio I/O, file decoding, mixing, and a
desktop UI. The project already declares JUCE as a dependency.

**Decision:** Use JUCE for the Phase 1 application shell, audio engine, and UI.
Defer Qt 6 integration to Phase 2 when the notation renderer requires it.

**Rationale:** JUCE provides audio, GUI, and CMake integration in one framework.
Starting with a single framework reduces build complexity. Qt can be added later
for advanced notation layout without replacing the audio layer.

---

## ADR-002: Demucs for stem separation

**Date:** 2026-07-08

**Context:** Users want to split a mixed song into vocals, drums, bass, and other
instruments. This requires AI-based source separation.

**Decision:** Integrate Demucs as an external subprocess for Phase 1. The application
invokes `demucs` (or `python -m demucs`) on the imported file and loads the output
stems into the mixer.

**Rationale:** Demucs is open source, well-maintained, and produces good 4-stem
separation. Running it as a subprocess avoids embedding Python/ONNX in the C++ binary
for now. The separation interface is abstracted so the backend can be swapped later.

**Alternatives considered:**
- Embedded ONNX model — higher integration cost, larger binary
- Cloud API — requires network, ongoing cost, privacy concerns
- Spleeter — older, generally lower quality than Demucs

---

## ADR-003: Stem-first playback model

**Date:** 2026-07-08

**Context:** Users need to solo or combine instrument parts during playback.

**Decision:** Model playback as independent stem tracks summed at the master output.
Each stem has mute, solo, and volume. The transport clock is shared across all stems.

**Rationale:** Standard DAW pattern. Keeps notation sync straightforward in Phase 2
because the timeline is independent of which stems are audible.

---

## ADR-004: Structured notation for sync (Phase 2)

**Date:** 2026-07-08

**Context:** Tabs and sheet music must stay in sync with playback.

**Decision:** Use structured score data (MusicXML or internal format with beat/measure
timestamps) rather than static PDF or image tabs.

**Rationale:** Sync requires mapping playback position to measures and beats.
Static images cannot be scrolled or highlighted programmatically.

---

## ADR-005: C++20 with modular architecture

**Date:** 2026-07-04 (from engineering rules)

**Decision:** Modern C++20, RAII, smart pointers, loose coupling between modules
(Application, Audio Engine, Notation Engine, AI Engine, etc.).

**Rationale:** Long-term maintainability and cross-platform reliability.