# DM5 Editor → Logic Pro Audio Unit: build plan

Hand this file to Claude Code in this folder. Work through the phases in order and
don't start a phase until the previous one's "done when" check passes.

## Goal

A macOS Audio Unit that opens inside Logic Pro and does everything the Electron
DM5 Editor does, and also saves the kit library in the Logic project.

- **Plugin type:** audio effect (stereo pass-through). It goes on the same channel
  strip as Logic's External Instrument plugin that plays the DM5. It ignores the
  audio and acts as the DM5 editor panel.
- **MIDI:** the plugin opens the DM5's CoreMIDI ports itself (JUCE `MidiInput` /
  `MidiOutput`). It does NOT route MIDI through Logic, because Logic can't be relied
  on to pass SysEx from a plugin to hardware. CoreMIDI lets Logic and the plugin share
  the same interface ports.
- **UI:** reuse the existing React UI from `DM5Editor.html` in a JUCE 8
  `WebBrowserComponent`, so there is one UI source shared by the app and the plugin.

## What the current app does (from the code)

- Web MIDI with `sysex: true` (`navigator.requestMIDIAccess`).
- SysEx header `F0 00 00 0E 13 7F <op> … F7` (device ID 0x13).
  - Kit dump: op `0x20 + kit` (kits 0–20), 343 bytes, checksum over bytes 7–340 at byte 341.
  - Kit request: op `0x40 | (0x20 + kit)`.
  - Trigger setup: dump op `0x05`, request op `0x45`.
- Timing: 300 ms between kit requests, 400 ms between kit sends.
- NRPN live preview: CC99=0, CC98=lsb, CC6=value. **Hard-coded to `0xB9` (channel 10)**,
  while Program Change uses the selectable `midiCh`. Fix this so NRPN also uses
  `midiCh` (see Phase 2).
- Program Change on kit select. Pad preview: note on/off on channel 10.
- Fragmented SysEx is reassembled in `handleMIDI` / `sysexBufRef`.
- Export/import JSON via `window.electronAPI` (falls back to browser download/upload).
- React 18, ReactDOM and Babel standalone are loaded **from unpkg at runtime**, and
  fonts come from Google Fonts. A plugin must work offline (see Phase 2).

## Key design rules

1. **The processor owns everything that has to outlive the window.** In Logic the
   plugin window (editor + web view) is destroyed when it closes. MIDI ports, the kit
   library, the trigger setup and all send queues live in the `AudioProcessor`. The
   web view is only a view.
2. **Store kits as raw SysEx.** The processor keeps up to 21 kit dumps (343 bytes each)
   plus one trigger-setup dump. JS keeps doing all the encoding and decoding with the
   existing `decodeKit` / `buildKitSysEx`. C++ never needs to understand the kit
   format, but can resend raw dumps without the UI open.
3. **Timing lives in C++.** Paced sends (300/400 ms) run on a C++ timer or background
   thread, not JS `setTimeout`, because web view timers can be throttled when the
   window is hidden or closed.
4. **Never touch the audio thread.** `processBlock` just passes audio through. No MIDI
   I/O, locking or allocation there.
5. **Open MIDI ports lazily.** Logic runs `auval` and scans plugins at startup, so
   don't open ports in the constructor. Open them on first editor open, or when
   restoring a state that names saved ports.

## Toolchain

- Xcode (App Store) plus command-line tools: `xcode-select --install`
- CMake: `brew install cmake`
- JUCE 8.x as a git submodule at `plugin/JUCE`, pinned to a release tag.
- **Licence note:** JUCE is free for personal use (JUCE Personal) or under AGPLv3. That's
  fine for your own use. Check JUCE's licence terms before distributing the plugin
  publicly from this MIT repo.

## Folder layout

```
DM5Editor/
  DM5Editor.html          ← stays the single UI source
  main.js, preload.js     ← Electron app (unchanged apart from the transport refactor)
  plugin/
    CMakeLists.txt
    JUCE/                 ← submodule
    Source/
      PluginProcessor.h/.cpp
      PluginEditor.h/.cpp
      MidiEngine.h/.cpp   ← ports, SysEx reassembly, paced send queue
    WebUI/                ← build output: DM5Editor.html + vendored JS/fonts
    scripts/build-webui.sh
```

Plugin IDs: manufacturer code `Kwln`, plugin code `Dm5e`, name "DM5 Editor",
manufacturer "DM5 Editor" (originally a personal name), formats `AU` (add `Standalone` too; it's free and handy
for testing).

---

## Phase 0: toolchain and an empty plugin

1. Add the JUCE submodule; create `plugin/CMakeLists.txt` with `juce_add_plugin`:
   `FORMATS AU Standalone`, `IS_SYNTH FALSE`, `NEEDS_MIDI_INPUT FALSE`,
   `COPY_PLUGIN_AFTER_BUILD TRUE`, link `juce_audio_utils` and `juce_gui_extra`, and set
   `NEEDS_WEB_BROWSER TRUE` / `JUCE_WEB_BROWSER=1`.
2. Processor: stereo in/out, `processBlock` does nothing (audio passes through).
3. Build: `cmake -B build -G Xcode && cmake --build build --config Release`.

**Done when:** `auval -v aufx Dm5e Kwln` passes and "DM5 Editor" appears in Logic under
Audio FX on an audio or instrument track. (If Logic doesn't see it: Plug-in Manager →
Reset & Rescan, or `killall -9 AudioComponentRegistrar`.)

## Phase 1: "hello DM5" (de-risk the MIDI path before any UI work)

Use a throwaway native JUCE UI: two combo boxes (MIDI in/out), a "Request kit 0"
button, and a text log.

1. `MidiEngine` lists devices with `MidiInput::getAvailableDevices()` /
   `MidiOutput::getAvailableDevices()`, and opens the chosen pair by identifier.
2. Request kit 0 (`F0 00 00 0E 13 7F 60 F7`). Reassemble incoming SysEx. JUCE normally
   delivers complete SysEx messages, but keep a guard. Log length, op byte and checksum
   result (port `calcChecksum` to C++ for this test only).
3. Marshal MIDI-thread callbacks to the message thread with `MessageManager::callAsync`.

**Done when**, running inside Logic itself (not just Standalone):
- kit 0 arrives as 343 bytes with a valid checksum
- Logic can still play the DM5 through the External Instrument while the plugin holds the ports
- closing and reopening the plugin window doesn't drop the ports or crash

This is the biggest risk. Logic may host AUs out-of-process on Apple Silicon. If
CoreMIDI access fails there, stop and report back before going further.

## Phase 2: make the web UI plugin-ready (do this in the shared HTML)

1. **Vendor dependencies.** Download React 18.3.1 and ReactDOM production UMD builds plus
   the Orbitron, Share Tech Mono and Rajdhani font files into a local `vendor/` folder.
   Better still, precompile the JSX with esbuild so Babel standalone isn't needed at
   runtime. `scripts/build-webui.sh` assembles everything into `plugin/WebUI/`.
   The Electron app can use the same build.
2. **Add a transport layer.** Replace direct Web MIDI calls with a small `transport`
   object chosen at startup:
   - `listPorts()`, `openPorts(inId, outId)`, `send(bytes)`, `onMessage(cb)`
   - `sendPaced(listOfMessages, gapMs, onProgress)` for REQ ALL / SEND ALL
   - `saveFile` / `openFile` (Electron: `electronAPI`; plugin: native dialogs)
   - `getState()` / `setState()` (plugin only; no-ops in Electron)

   Implementations: `WebMidiTransport` (Electron, today's behaviour) and
   `JuceTransport` (plugin), picked by checking `window.__JUCE__`.
3. **Fix NRPN channel.** Use `0xB0 | midiCh` instead of hard-coded `0xB9`.

**Done when:** the Electron app behaves exactly as before, fully offline (turn Wi-Fi off
and test). Commit this before starting Phase 3.

## Phase 3: web UI inside the plugin

1. Editor creates a `WebBrowserComponent` with
   `Options().withNativeIntegrationEnabled().withResourceProvider(...)`. The provider
   serves files from `WebUI/`, embedded via `juce_add_binary_data` so the plugin is
   self-contained.
2. Native functions (`withNativeFunction`) map to the transport:
   `listPorts`, `openPorts`, `send`, `sendPaced`, `saveFile`, `openFile`, `getState`,
   `setState`. Byte arrays go across as arrays of ints.
3. Incoming MIDI → `emitEventIfBrowserIsVisible("midi", bytes)`. JS listens via
   `window.__JUCE__.backend.addEventListener("midi", …)`. Progress for paced sends goes
   the same way (`"sendProgress"`).
4. Editor size: 1280×960 to match the app, resizable with a minimum of 900×700.

**Done when:** every feature in the README works from inside Logic: INIT MIDI, REQ ALL,
kit select with Program Change, pad edit with NRPN preview, APPLY, SEND KIT, SEND ALL,
the TRIGGERS tab, the drum map, copy/paste, export/import JSON, and the activity log.

## Phase 4: save and recall with the Logic project

1. `getStateInformation`: write a `ValueTree` or JSON containing the 21 raw kit dumps
   (base64; null for kits not loaded), the trigger-setup dump, the MIDI in/out device
   identifiers **and names** (identifiers can change between sessions, so fall back to
   name matching), `midiCh`, the active kit index, and UI prefs (theme, tweaks).
2. `setStateInformation`: restore it, and push it into the web view if the editor is open.
3. Option **"Send kits to DM5 when project opens"** (default OFF): after state restore,
   open the ports and send the stored dumps through the paced queue (400 ms gaps). Then
   send Program Change for the saved active kit. This must work without the plugin
   window ever being opened.
4. Option "Send active kit only" for a faster recall (one dump + PC).

**Done when:** you edit a kit, save the project, power-cycle the DM5 (or load a
different kit on it), reopen the project, and the module comes back to the saved
setup without the plugin window being opened.

## Phase 5: polish

- Status line in the UI: ports open / not found / sending.
- If saved ports are missing on load, log it and don't crash or hang Logic.
- Multiple instances: allow it, but warn in the log if another instance holds the same output.
- Version string in the UI; update `README.md` with a "Logic plugin" section.

## Test checklist (run inside Logic each phase)

- [ ] Plugin loads; `auval` passes
- [ ] Audio passes through unchanged (bypass A/B)
- [ ] Logic playback of the DM5 is unaffected while the plugin is open and while it sends
- [ ] REQ ALL receives all 21 kits with valid checksums
- [ ] SEND ALL completes; the DM5 shows the new data
- [ ] Close/reopen window mid-transfer: transfer completes, no crash
- [ ] Save/reopen project restores the library; optional send-on-load works with window closed
- [ ] Works with Wi-Fi off
- [ ] Electron app still works after the shared-UI changes
