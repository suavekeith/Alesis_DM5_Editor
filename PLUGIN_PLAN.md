# DM5 Editor → Logic Pro Audio Unit: build plan

Hand this file to Claude Code in this folder. Phases 0 to 4 are built; start with
"Handoff: where things stand" below, then do the remaining work in the order given there.
The original phase descriptions further down are kept for reference.

## Handoff: where things stand

### Status

| Phase | State |
|---|---|
| 0 Toolchain, empty plugin | Done, tested in Logic |
| 1 MIDI path ("hello DM5") | Done, tested in Logic with the DM5 |
| 2 Offline web UI + transport layer | Done, Electron app tested with the DM5 |
| 3 Web UI inside the plugin | Done, tested in Logic (Send Kit and window size were fixed afterwards and confirmed) |
| 4 Save/recall with the project | Done, tested in Logic with the DM5 |
| MIDI FX switch (not in the original plan) | Done, tested in Logic |
| 5 Polish | Done, tested in Logic (shared-output warning works, so Logic hosts all instances in one process) |
| Electron: saved state + auto MIDI init | Done, tested by the user |
| Final test checklist | Done, all items confirmed by the user (2026-11-19) |

### How this project is worked on

- The user tests in Logic with the real DM5 and reports back. You cannot see Logic, the
  plugin window or the DM5, and `screencapture` is not permitted on this machine. Never
  report something as working in Logic unless the user has said so. Say what you verified
  (build, `auval`, browser test) and what still needs his test.
- One commit per phase or fix, made after the build and `auval` pass. Build before you
  commit; a broken build was committed once by committing in the same command as the build.
- The user is not a C++ developer. Explain results in plain terms, and give them exact steps
  to test, including what the activity log should say.
- Don't touch the untracked `octachron/` folder or `*.octachron` file in the repo root.
  They belong to the user and are unrelated.

### Build and verify

Xcode.app is not installed (Command Line Tools only), so the Xcode generator does not
work. Use Ninja. CMake and Ninja are in `/opt/homebrew/bin`; Node is in `~/.local/bin`.

```bash
cd plugin
/opt/homebrew/bin/cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
/opt/homebrew/bin/cmake --build build --config Release
auval -v aumi Dm5e Kwln
```

- The build runs `plugin/scripts/build-webui.sh` itself whenever `DM5Editor.html` changes,
  embeds the result in the plugin, and installs the component to
  `~/Library/Audio/Plug-Ins/Components/`.
- After changing the AU type or IDs: `killall -9 AudioComponentRegistrar`, and the user must
  rescan or restart Logic.
- Electron app: `npm start` (builds the web UI first, then launches).
- To test UI logic without Logic: serve `plugin/WebUI/` (a `webui` config exists in
  `.claude/launch.json`) and load `index.html`. For the plugin code path, load the page in
  an iframe with a fake `window.__JUCE__.backend` injected before the page scripts; it needs
  `addEventListener` and `emitEvent`, and must answer `__juce__invoke` events with a
  `__juce__complete` event carrying `{ promiseId, result }`. For the Electron path, replace
  `navigator.requestMIDIAccess` with a fake before clicking INIT MIDI.

### What was built differently from the original plan

- **Plugin type is MIDI FX** (`aumi`), not an audio effect. It goes in the MIDI FX slot
  above the External Instrument. It has no audio buses and passes the track's MIDI through
  untouched. Everything below that says "audio effect", "stereo pass-through", "Audio FX"
  or `aufx` is out of date.
- **`DM5Editor.html` is a build source, not a runnable page.** It references `vendor/`
  files that only exist in the build output and contains JSX that esbuild compiles.
  Both Electron and the plugin load `plugin/WebUI/index.html`. There is no committed
  `vendor/` folder; React and the fonts are npm dev dependencies copied in by the build.
- **Paced sends use job ids.** JS picks a random job id and passes it to `sendPaced`;
  C++ reports `sendProgress` events as `{ jobId, index, total, done }`. Sends started by
  C++ itself (the on-open send) use job id `-1`, which the UI currently ignores.
- **Send Kit also writes the DM5 edit buffer.** A slot dump (opcode `0x20 + kit`) is
  stored but does not change what the module plays. When the PC checkbox is on and the
  kit sent is the active kit, the same data is sent again with opcode `0x01`. The on-open
  send does the same for the active kit, then sends Program Change.
- **State ownership.** The processor holds the saved state as raw dumps. The UI reads it
  with `getState` when the window opens and pushes the whole state back with `setState`
  (debounced 200 ms) on any change. The processor also stores valid dumps arriving from
  the DM5 itself, so kits received with the window closed are kept. Ports are recorded by
  the `openPorts` native call only when its third argument (`remember`) is true; the UI
  passes false when it is just reconnecting on restore, so a missing interface doesn't
  erase the saved port.
- **Plugin opens its ports list automatically**; INIT MIDI is only needed in Electron.
  Clicking a pad plays it (added in a forked session).
- **Window:** opens at a size capped to the display, minimum 900x500, with a 16 px strip
  under the web view because the native web view would otherwise cover the resize corner.
- **ON OPEN option** is a dropdown in the toolbar (plugin only): DON'T SEND, SEND ALL
  KITS, SEND ACTIVE KIT.

### Where things are

- `DM5Editor.html`: the shared UI. The transport layer (`createWebMidiTransport`,
  `createJuceTransport`) is in the plain script block before the JSX block. State restore
  is `applyState`; the state push is the effect right after it.
- `plugin/Source/MidiEngine.*`: ports, paced send thread, delivery of incoming SysEx and
  progress to listeners on the message thread.
- `plugin/Source/PluginProcessor.*`: saved state, JSON save/load, on-open send.
- `plugin/Source/PluginEditor.*`: web view, embedded resources, native functions.
- DM5 MIDI spec: removed from the repo but still in history. Extract it with
  `git show 6f331f3:d4d5midm.pdf > spec.pdf` (text extraction works with `pypdf`).
  Check it before changing anything about SysEx opcodes, NRPNs or Program Change.

### Remaining work, in order

**1. ~~Get Phase 4 and the MIDI FX switch tested.~~** Done. Ask the user to run these in Logic and fix
whatever fails before starting Phase 5:

- Insert DM5 Editor in the MIDI FX slot; the DM5 still plays from the track.
- REQ ALL, edit a kit, APPLY, close and reopen the plugin window: library, ports, channel
  and checkboxes are all still there.
- ON OPEN = SEND ACTIVE KIT, save the project, change kit on the DM5 or power-cycle it,
  reopen the project without opening the plugin window: the DM5 returns to the saved kit.
- Same with SEND ALL KITS (about 9 seconds for 21 kits).
- Unplug the MIDI interface, open the project: Logic doesn't hang, and the log reports the
  missing port when the window is opened.

**2. Phase 5 polish.** Done and tested. What was done:

- Status light shows `SENDING n%` during any transfer, including the on-open send
  (job id `-1`, delivered to the UI through `transport.onBackgroundSend`), and
  `PORT NOT FOUND` when a saved port is missing.
- C++ log channel: `DM5AudioProcessor::log()` queues messages; the UI drains them with the
  `takeLog` native function on open and whenever a `logAvailable` event arrives. The
  on-open send logs start, finish, "output not found" and "no saved kits".
- Shared outputs: `MidiEngine` keeps a process-wide count of open output ids, and
  `openPorts` returns `outputShared`. This only sees instances in the same process. It
  The user confirmed the warning appears in Logic, so Logic hosts the instances in one process.
- Version: plugin from `JucePlugin_VersionString` (via `withInitialisationData`), app from
  `package.json` (substituted by `build-webui.sh`). Shown in the header subtitle. Keep the
  version in `plugin/CMakeLists.txt` and `package.json` in step.
- README has a "Logic plugin" section and the `npm start` build-step note.

Original Phase 5 task list:

- *Status line.* The header already shows port status. Add a "sending" state, including
  for the on-open send that C++ starts (job id `-1`), so opening the window during it
  shows a transfer in progress.
- *Log messages from C++.* There is no channel for the processor to write to the activity
  log. Add one (an event to the web view, plus a short backlog for messages raised while
  the window is closed), and use it for the on-open send ("sent 21 kits on project open",
  "saved output not found, nothing sent").
- *Multiple instances.* Allow them, but warn in the log when another instance already has
  the same MIDI output open. A process-wide registry of open output ids in `MidiEngine`
  is enough if Logic hosts the instances in one process; say so in the notes if that
  can't be confirmed.
- *Version string* in the UI, from `JucePlugin_VersionString` in the plugin and
  `package.json` in Electron.
- *README:* add a "Logic plugin" section (build steps, MIDI FX slot, ON OPEN option,
  JUCE licence note), and note that `npm start` now runs a build step.

**3. Bring two plugin features to the Electron app.** Done and tested.
State is saved to `state.json` in Electron's `userData` folder via the `load-state` /
`save-state` IPC handlers; `WebMidiTransport` tracks the user's port choice and resolves
saved ports by id then name. The restore effect in `DM5Editor.html` now runs for both
transports. Original task:

- *Remember state between launches.* The Electron app starts empty every time. Save the
  same state the plugin saves (kit library, trigger setup, ports, MIDI channel, active
  kit, checkboxes) and restore it on launch. Implement `getState` / `setState` in
  `WebMidiTransport` (they are no-ops today), backed by a JSON file in
  `app.getPath('userData')` through new IPC handlers in `main.js` and `preload.js`.
  Then let the existing `applyState` call and state-push effect in `DM5Editor.html` run
  for both transports instead of only when `transport.kind === 'juce'`. Web MIDI port ids
  can change between launches, so store port names too and match by name when the id is
  gone, as the plugin does. The ON OPEN dropdown stays plugin-only.
- *Automatic MIDI init.* `main.js` already grants MIDI and SysEx permission without a
  prompt, so call `initMIDI` on launch in Electron as the plugin does. Keep the INIT MIDI
  button as a manual rescan.

Done when the user confirms: quit and relaunch the app, and the kits, ports and settings
are back without clicking INIT MIDI; with the MIDI interface unplugged, the app still
launches and the log names the missing port.

**4. ~~Run the full test checklist~~** Done; every item confirmed.

**Done when** every checklist item has been confirmed by the user, and the README describes
the plugin.

### Open questions for the user (don't decide these alone)

- Pad preview note on/off is fixed to channel 10 (`0x99` / `0x89`); only NRPN and Program
  Change follow the channel selector. The plan specified this, but it may not be what he
  wants.
- The on-open send covers kits only, not the trigger setup.
- The forked session's summary said selecting a *sound* previews it, but the code plays a
  note only when a *pad* is clicked. Changing the sound dropdown does not play anything.
- The theme and pad-size "tweaks" panel can't be opened in the app or the plugin (it only
  responds to messages from a design tool), so those settings are saved but not changeable.

### Known oddities

- The DM5 replies to every SysEx it receives with `F0 00 00 0E 13 7F F7`. It isn't in the
  spec and appears harmless; it shows in the log as an unknown 7-byte SysEx.
- The plugin takes keyboard focus so kit names can be typed. If the user reports Logic key
  commands not working while the plugin window is focused, that is why
  (`EDITOR_WANTS_KEYBOARD_FOCUS` in `plugin/CMakeLists.txt`).
- Changing a port selection while a transfer is running reopens only the port that
  changed, but messages sent while the output is closed are dropped silently and the
  progress bar continues.
- The plugin is built for arm64 only, which is fine for this Mac.

## Goal

A macOS Audio Unit that opens inside Logic Pro and does everything the Electron
DM5 Editor does, and also saves the kit library in the Logic project.

- **Plugin type:** MIDI FX (changed from the audio effect originally planned; see the
  handoff section). It goes in the MIDI FX slot above Logic's External Instrument plugin
  that plays the DM5, passes MIDI through, and acts as the DM5 editor panel.
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

## Test checklist (run inside Logic each phase; all confirmed 2026-11-19)

- [x] Plugin loads; `auval` passes
- [x] The track's MIDI passes through unchanged (the DM5 plays with the plugin inserted)
- [x] Logic playback of the DM5 is unaffected while the plugin is open and while it sends
- [x] REQ ALL receives all 21 kits with valid checksums
- [x] SEND ALL completes; the DM5 shows the new data
- [x] Close/reopen window mid-transfer: transfer completes, no crash
- [x] Save/reopen project restores the library; optional send-on-load works with window closed
- [x] Works with Wi-Fi off
- [x] Electron app still works after the shared-UI changes
- [x] Electron app restores kits, ports and settings after quit and relaunch
