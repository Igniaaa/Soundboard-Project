# Effects Dry Level Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Preserve the initial dry level at maximum effect mix and control accumulated level with the stereo limiter.

**Architecture:** Keep feedback input stabilisation and the existing effect chain. Remove whole-signal wet/dry normalisation. Give the final stereo peak limiter a 20 ms hold before its existing 100 ms recovery.

**Tech Stack:** C++17, CMake/CTest, MSVC, PowerShell, JUCE application.

**Spec:** `docs/superpowers/specs/2026-10-27-effects-dry-level-design.md`

## Global Constraints

- Limiter stereo a 0,98; mantenimento circa 20 ms e recupero circa 100 ms.
- Nessun lookahead, buffer aggiuntivo o nuova latenza.
- Conservare stabilizzazione del feedback e correzioni delle code a fine file.
- Nessun nuovo controllo, parametro salvato, cambio di schema o intervento sullo scrub.
- No publication or push; preserve pre-existing documentation edits.

## Review Focus

- Combined effects: first attack remains 0.8 at mix 100%, including negative/right-channel polarity.
- Full-scale resonant notes: limiter bounds output without cycle-by-cycle gain modulation after settling.
- Low/moderate inputs: wet contribution is audible and the chain remains linear below limiting.
- 44.1/48/96/192 kHz: limiter hold/recovery remain time based and stereo linked.
- Limiter recovery: after overload, a later quiet signal returns to its own level.

### Task 1: Dry level and limiter regression

**Files:** `Source/AudioCore.h`, `Tests/AudioQualityTests.cpp`, `Tests/AudioTests.cpp`, `LEGGIMI.md`, `DOCUMENTAZIONE-CODICE.md`, `VERIFICHE-AUDIO-REVIEW.md`.

**Interfaces:** Retain `AudioCore::prepare(double)` and `AudioCore::render(float*, float*, int, const float*, const float*, float)` unchanged. No changes to VoiceSettings or persistence.

- [x] Write and run the initial-attack regression against the current headers. Expected: input 0.8 becomes 0.4 for one effect and 0.2 for delay plus reverb; test fails.
- [x] Remove only the divisions by `1 + mix` in delay, ping-pong and reverb; keep the normalised feedback input. Re-run the attack test; expected initial samples 0.8.
- [x] Align impulse expectations with additive wet output. Replace the former above-threshold linearity requirement with below-threshold linearity plus a separate overloaded peak/THD test.
- [x] Write a bass sine regression using real microphone-input mixing, at 44.1/48/96/192 kHz; expected finite output <= 0.98001, right = -0.7 * left, steady fundamental residual < 0.001, and recovered quiet level after 1 second. Watch the existing limiter fail the residual test.
- [x] Add `limiterHoldFrames` state to AudioCore, clear in prepare, and hold gain for 20 ms following each stronger peak, then keep the existing 100 ms recovery. Add no latency or allocation.
- [x] Run `./Build.ps1`; expected four CTest suites pass and Release builds. If the user app locks an executable, keep it running and copy the verified build under a separate explicit name.
- [x] Update the user documentation and this task's ledger; perform one fresh-context code review of this change. Keep the fix reviewable locally without pushing.
