# Reverb Diffusion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Replace sparse rattling reverb tails with a damped, diffused wet response while preserving the direct attack.

**Architecture:** Keep four feedback combs; damp their feedback at6kHz. Apply four unity-magnitude Schroeder allpasses to the wet sum, gain0.65, delays5.1/7.7/10.3/12.7ms. Preallocate stereo cells in prepare and reuse voice epochs.

**Tech Stack:** C++17, MSVC, CMake/CTest, PowerShell, existing JUCE application.

**Spec:** `docs/superpowers/specs/2026-10-27-reverb-diffusion-design.md`

## Global Constraints

- Diretto a guadagno unitario, mix additivo, stesso limiter e nessun nuovo controllo o schema dati.
- Buffer allocati solo in prepare; nessuna allocazione o lock nel callback.
- Entrambi i canali elaborati senza sommarli in mono.
- Preserve current dirty changes, original audio files, delay and scrub. No push or publication.

## Review Focus

- Short percussion: dense wet response must not attenuate the direct transient.
- Sample-rate changes: diffusion and damping times remain correct at44.1/48/96/192kHz.
- Stereo and silent pad reuse: do not cancel anti-phase content or resurrect another pad's tail.
- Short decay: allpass storage must drain before a voice is reclaimed.
- Multiple voices: processing and preparation stay bounded without audio-thread allocation or large clears.

### Task 1: Damped and diffused reverb

**Files:** `Source/AudioCore.h`, `Tests/ReverbTests.cpp`, `Tests/AudioTests.cpp`, `CMakeLists.txt`, user documentation.

**Interfaces:** Keep VoiceSettings, JSON schema, `AudioCore::prepare(double)` and `render(...)` unchanged. Add private arrays for four allpass offsets/lengths, per-voice cursors and damped L/R feedback states.

- [x] Add real-engine regressions: density of samples >-80dB of local peak between200–500ms >0.5; high-frequency late/early power ratio <0.2 of low-frequency ratio; first sample0.5; zero mix silent tail; short decay lower late energy; stereo wet content nonzero; reused silent voice remains silent. Observe density/damping fail on old code.
- [x] Add damping to existing comb feedback. Implement four wet-only allpasses as `y=delayed-g*x; buffer=x+g*y`, with existing epoch validation. Extend prepare storage and tail budget, clear per-voice scalar states at trigger, suppress only feedback below1e-20.
- [x] Replace obsolete exact-four-reflection tests with dry/tail/decay behavior; add `reverb_quality` to CTest and use the same static runtime as other tests.
- [x] Run `./Build.ps1` and all CTest suites; compare current probe against old impulse/Bonk measurements. Keep a verified Release executable.
- [x] Complete one fresh-context independent review, document measured limits and results, leave changes local for user listening.
