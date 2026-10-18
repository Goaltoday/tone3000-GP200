# Tone3000 + GP200 — experimental integration 0.3

Upstream base: `ab5e5ae4388753184687d67816815aa481f2cea9`.

## Use

Browse and load NAM models using Tone3000 normally. Click **GP200** in the header, choose one of the NAMs loaded in the left or right chain, then choose MIDI input/output, an eligible destination AMP and a 1–12 character ASCII name. The tab refreshes the source list when opened; **Refresh loaded NAMs** refreshes it manually.

Only the active, fully loaded NAM model of each chain block is listed. This is not a library of every download ever made. A block removed from the chain disappears on refresh. Selection snapshots the original cached bytes: loading from a local file, catalog download or restored session uses the same validation. A model changed/loading since list refresh is rejected.

The GP200 reader must find exactly one compatible A2 Lite model at 48 kHz, 1,871 finite weights and the exact expected topology. Full-only and incompatible NAMs are rejected. A supported SlimmableContainer contributes its Lite member even if Tone3000 was auditioning Full.

Only NAM weights/name are transferred. Tone3000 gain, normalization, calibration, EQ, IR and chain settings do not transfer, and audition loudness need not match. GP200 Input/Output remain preset settings.

The pedal needs compatible **NH04 custom firmware** (Unified 1.7.2 or the experimental Lite branch). This app does not install firmware. Select a DIFFERENT AMP on the pedal first; bypass is insufficient. Keep preset/AMP unchanged throughout upload. Do not open the standalone loader or GP200 VST on the same MIDI ports concurrently. Start testing with Tone3000 Standalone.

State dump (5 frames) → live preset (7 frames) → active destination check → HQ → HN → verified ACK. The receiver independently rejects an active destination. No stock IR commit or automatic write retry. Status 11 blocks further loading on an uncertain recovery. Missing final ACK means persistence is uncertain. Switching the GP200 tab off keeps an in-flight transfer alive; closing the application/editor stops the host and cannot undo a write already sent.

## Build

Use the normal upstream recursive submodules and CMake build. The new manual workflow **Build Tone3000 GP200 - Windows** builds Standalone and optionally VST3, with GP200 protocol and optional native UI self-tests. Set repository secret or variable `T3K_PUBLISHABLE_KEY` to your own Tone3000 publishable key for catalog login (upstream requirement); no private API key is needed. Local files do not need catalog login. GitHub Actions has not been run in this session.

The source patch is based on the exact commit above. Apply with `git apply GP200.patch` in a clean checkout at that commit. Normal upstream dependencies, including JUCE 9.0.3, remain unchanged. GP200 includes inherited GPL terms; see the adjacent NOTICE/COPYING files before redistribution.

## Validation

Locally: state/live packet regression executable; memory/file NAM reader equivalence, invalid-model rejection and NH04 frame counts using modeled JUCE types; syntax checks of the panel, NAM codec, header/root, processor cache accessor and processor backend with real JUCE 9.0.3 headers. Frame regressions cover reordering, duplicates, inconsistent totals, invalid nibbles/header, out-of-bounds offsets and incomplete/unrelated messages. This does not establish a linked Windows build or physical MIDI/audio compatibility. No firmware changes or pedal writes were performed.

Remaining: linked Windows build, visual review at supported window sizes, 5+7 physical MIDI capture, positive/negative upload tests and persistence. Original REVIEW3 transaction timings and the current experimental firmware limits remain applicable.

Revision 0.2: strict ACK vendor prefix, explicit destination whitelist check, matched destination before ACK status handling, explicit input stop during reset, improved MIDI port conflict message, Windows workflow with optional VST3/UI tests and failure logs. See GP200_GITHUB_ACTIONS_ES.md.

Revision 0.3: initialize and verify recursive submodules explicitly; fall back to the NAM Core commit pinned by the upstream base if the fork checkout omits the gitlink.
