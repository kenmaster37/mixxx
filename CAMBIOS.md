# Changes in this fork compared to the original Mixxx

- **Repository:** <https://github.com/kenmaster37/mixxx>
- **Base:** Mixxx 2.7.0-alpha, commit `0fffd3d0d0`
  (*"Merge pull request #17253 from xARSENICx/fix/latenightqml-animations"*).
- **Documented:** 13-10-2026.
- **Session-by-session detail:** the author's internal log, `PENDIENTE.md` (not cleaned up and not
  part of this repository).

## What this fork is

A personal Mixxx build meant for DJing with a **Numark Mixtrack Pro II**, with two QML skins of its
own in VirtualDJ/Serato style, AI-driven AutoDJ, an OSC server, a panel that suggests the next track
by musical key and an **audio device assistant** that detects the mixer when it is plugged in or
unplugged hot.

It does not aim to be accepted upstream: these are project changes (skins of its own, a personal
workflow). They are kept separate so that the merge onto `main` can be redone whenever wanted.

## Summary by area

| Area | Change | Where |
| --- | --- | --- |
| **Skins of its own** | `PulseDeck` and `NeonDeck`: full QML skins (VDJ/Serato style layout, colour per deck, Rajdhani type, Font Awesome icons) | `res/skins/PulseDeck/`, `res/skins/NeonDeck/` |
| **Fonts** | Rajdhani (SIL OFL) and Font Awesome Free 6.7.2 (SIL OFL / CC BY 4.0 / MIT), with their licences | `res/fonts/` |
| **QML skin loading** | The QML skins load without `--developer`; `NeonDeck` is the default skin | `src/main.cpp`, `src/skin/skinloader.{h,cpp}` |
| **Native dark style** | Default style sheet for the native widgets (library, menus, dialogs) | `src/skin/defaultstylesheet.{h,cpp}`, `src/mixxxmainwindow.cpp` |
| **"Track Advisor" panel** | Suggests the next harmonically compatible track, under the samplers | `src/qml/qmllibraryproxy.{h,cpp}`, `src/skin/skincontrols.{h,cpp}`, `res/skins/*/Advisor/TrackAdvisor.qml` |
| **Harmonic AutoDJ** | Selection by key/BPM with progressive degradation; tests | `src/library/autodj/harmonicmixing.{h,cpp}`, `autodjcratesdao.{h,cpp}`, `autodjfeature.{h,cpp}` |
| **AI AutoDJ** | OpenAI/DeepSeek-compatible planner and an instruction bar in AutoDJ; key read asynchronously (QtKeychain) | `src/ai/aimixplanner.{h,cpp}`, `src/preferences/dialog/dlgprefai.*`, `dlgautodj.{h,cpp}` |
| **OSC server** | OSC server with a preferences page | `src/controllers/osc/oscserver.{h,cpp}`, `src/preferences/dialog/dlgprefosc.*` |
| **Audio assistant (VDJ style)** | Dialog when a controller is connected/disconnected, hot output change and remembered choice | `src/soundio/audiodeviceassistant.{h,cpp}`, `src/util/systemdevices.{h,cpp}` |
| **Audio: no mixer** | If the configured device is not connected, the system default device plays instead; on Windows the app does not close when the sample rate is not supported | `src/soundio/soundmanagerconfig.{h,cpp}`, `soundmanager.{h,cpp}`, `sounddevice*.{h,cpp}`, `portaudioenumerator.{h,cpp}` |
| **Full screen (Linux)** | The full screen action affects the window that shows the QML skin | `src/qml/qmlapplication.{h,cpp}`, `src/qml/qmlapplicationproxy.h`, `src/mixxxmainwindow.cpp` |
| **Keyboard shortcuts** | The *fallback* to `en_US.kbd.cfg` looked up the path wrong: nobody had shortcuts. And once they were active, typing in the AutoDJ AI box fired the deck commands | `src/controllers/keyboard/keyboardeventfilter.{h,cpp}`, `src/qml/qmllegacylibraryitem.cpp` |
| **Mixtrack Pro II controller** | Scripts and mappings without non-existent controls and without the platter timer's `abort()` | `res/controllers/Numark Mixtrack Pro.midi.xml`, `Numark-Mixtrack-{2,Pro}-scripts.js` |
| **Deck interface** | Hotcue pads that covered the deck settings; dead buttons; unreadable sandwich button with the menu open | `res/skins/*/Deck/*`, `res/skins/*/Toolbar/Toolbar.qml`, `res/qml/Deck/BeatJump.qml` |
| **Packaging** | `.msi` and `.deb` packaging, and the FFmpeg `-extra` alternative for the `.deb` | `packaging/CPackConfig.cmake`, `packaging/CPackDebFfmpegExtraAlternatives.cmake` |

## 1. QML skins of its own: `PulseDeck` and `NeonDeck`

Both start from `LateNightQML` and are rewritten as flat, dark skins, with the look of a modern
console: colour per deck (A cyan / B pink) with matching waveforms, coloured rings on the platters,
a pad grid to the right of the browser, FX on the edges and type of their own.

- `res/skins/PulseDeck/` and `res/skins/NeonDeck/`: full skin (compact/mixi/full deck, mixer,
  effects, sampler, mic/aux, library, toolbar and Advisor panel).
- `res/skins/NeonDeck/Controls/IconLabel.qml`: draws a Font Awesome glyph as if it were text
  (`LateNightTheme.iconGlyph(name)`), which allows the icons to be coloured according to state.
- `src/main.cpp`: the condition that required `--developer` to load QML skins was removed.
- `src/skin/skinloader.{h,cpp}`: `skinFromDirectory()` discovers the QML skins like any other,
  `getDefaultSkinName()` returns `NeonDeck`.
- `src/skin/defaultstylesheet.{h,cpp}` + `src/mixxxmainwindow.cpp`: style sheet of its own for the
  native widgets (the tree does not use the `LateNight` look).
- `res/skins/LateNightQML/**`: minor shared tweaks (beat buttons, `SkinControlBootstrap`,
  toolbar and application menu shortcuts).

⚠️ **Assets trap:** `lateNightAsset()` builds the paths as
`../../LateNight/<ColorScheme.name>/...`, and in `res/skins/LateNight/` only `classic` and
`palemoon` exist. If a scheme declares another `name`, **no SVG is loaded**. In NeonDeck
`neondeck → palemoon` is mapped in its `LateNightTheme.qml`.

## 2. "Track Advisor" panel

Collapsible panel (**off** by default) that lists the library tracks that mix harmonically with the
one playing, under the sampler grid; the library scrolls instead of being covered. Clicking loads
the track into the deck that is not the reference.

- `QmlLibraryProxy::findCompatibleTracks()` (new, `src/qml/qmllibraryproxy.{h,cpp}`) reuses
  `mixxx::HarmonicMixing` and returns a list of maps ready for QML.
- Control `[Skin] show_track_advisor` (`src/skin/skincontrols.{h,cpp}`), **ADVISOR** button in the
  toolbar and *View → Show Track Advisor* (**Ctrl+7**) in `src/widget/wmainmenubar.{h,cpp}`.
- QML: `res/skins/{NeonDeck,PulseDeck}/Advisor/TrackAdvisor.qml` and its place in `MainWindow.qml`.

## 3. Harmonic AutoDJ and AI AutoDJ

- `src/library/autodj/harmonicmixing.{h,cpp}` (new): circle of fifths over the `ChromaticKey` enum,
  with the BPM tolerance `[Auto DJ] HarmonicBpmTolerance`; it also accepts the half and double
  tempo windows. If no candidate qualifies, it degrades **first** to key compatibility and only
  then to random (before, harmony was lost with libraries concentrated in a narrow BPM range).
- `src/library/dao/autodjcratesdao.{h,cpp}`: `getRandomTrackIdHarmonic()`.
- `src/library/autodj/autodjfeature.{h,cpp}` and `dlgautodj.{h,cpp}`: natural language instruction
  bar and planner created lazily.
- `src/ai/aimixplanner.{h,cpp}` (new): OpenAI/DeepSeek-compatible client. **The API key is not in
  the repository**: it is stored in the preferences and read with QtKeychain asynchronously
  (`ReadPasswordJob`), just like the `DlgPrefAi` page does.
- `src/preferences/dialog/dlgprefai.{h,cpp,.ui}` (new) and `dlgprefautodj.*` (*Harmonic Mixing*
  group), registered in `dlgpreferences.cpp`.

## 4. OSC server

`src/controllers/osc/oscserver.{h,cpp}` (new) is created and started from `CoreServices`
(`m_pOscServer`), with its preferences page `dlgprefosc.{h,cpp,.ui}`. It is used to control Mixxx
from outside (for example, to measure CPU and to automate tests).

## 5. Audio device assistant (VirtualDJ style)

**What it does:** when a controller is connected or disconnected while Mixxx is open, a dialog
appears that **names the recognised device** and lets you choose, and the audio output switches
hot without a restart. If the device that was in use disappears, it warns and moves to the
computer audio. The choice can be remembered (`[Sound] controller_audio_device` = `mixer` | `computer`).
The same action is in the menu (*View → Audio Setup Assistant*, `Ctrl+8`) and, when used, it
**always asks** even if there is a remembered choice; if there is no mixer at all, it says so.

- `src/soundio/audiodeviceassistant.{h,cpp}` (new): watcher every 2 s, dialogs, choice of the
  API and of the channels (`Main` on channels 1-2 and, with 4 or more channels, `Headphones` on 3-4).
- `src/util/systemdevices.{h,cpp}` (new): **cheap** list of system devices (MIDI with
  `winmm` on Windows, `/proc/asound/cards` on Linux). It does not touch the audio engine nor the
  controller backend, which is why it can be queried every 2 s.
- When a change is detected the devices are really re-enumerated (`SoundManager::reinitializeDevices()`:
  closes the streams, `Pa_Terminate`/`Pa_Initialize` and matches the configuration again), which
  **cuts the audio for an instant** unavoidably: PortAudio only enumerates at startup. It was
  attempted to avoid it with retries and with a forced WASAPI preference and **both were reverted**
  (more dropouts and lag; see `PENDIENTE.md` §6.15).
- The controller is recognised **by name** (the MIDI name compared with the one of the outputs,
  normalised to lowercase letters and digits: `MixTrack Pro II` matches
  `Altavoces (MixTrack Pro II)`), so it works with any brand.
- The assistant **respects the configured API**: if the device shows up in the API of the
  configuration, it uses it; if not, it walks the natural order of `getHostAPIList()`. It does not
  force WASAPI nor degrade to MME.
- Wiring: `src/qml/qmlapplication.{h,cpp}` (owner of the assistant in the QML builds),
  `src/qml/qmlapplicationproxy.h` (`Mixxx.Application.checkAudioDevices()`),
  `src/mixxxmainwindow.{h,cpp}` (builds without QML), `src/widget/wmainmenubar.{h,cpp}` and the menu
  entries of the three skins (`Toolbar/Toolbar.qml`, `Toolbar/ApplicationMenuShortcuts.qml`,
  `res/qml/ApplicationMenuActions.qml`, `res/qml/MainMenuBar.qml`).
- `CMakeLists.txt`: new sources and linking of `winmm` (the executable does not inherit it from the
  static library).

**Known limitation:** if Mixxx starts **with the mixer disconnected** and it is plugged in later, the
audio changes but the buttons do not respond until Mixxx is restarted: `PortMidiEnumerator` only
enumerates devices when it initialises. The attempt to fix it (a `refreshHostDevices()` with
`Pm_Terminate` + `Pm_Initialize`) **crashed** because of stale objects from the incremental build and
was reverted; to try again a `cmake --build . --clean-first` is needed after touching the base class
and syncing with `dlgprefcontrollers` (see `PENDIENTE.md` §6.15).

## 6. Audio: what happens when there is no mixer

- `SoundManagerConfig::validateDevices()`: if no output device is left connected, it adds the output
  of the **system default device** so that Mixxx keeps sounding instead of going silent, marking the
  addition as temporary so that it is **not saved** in `soundconfig.xml` and so that the user's
  choice is kept and recovered on reconnection.
- `SoundManagerConfig::addOutput()`/`addInput()` remove that temporary mark when a device is added,
  so that what the user (or the assistant) chooses **is** saved. Without this, the choice was lost.
- **Sample rate negotiation:** `SoundDevice::isSampleRateSupported()` (in PortAudio with
  `Pa_IsFormatSupported`) and `SoundManagerConfig::checkSampleRate()` try the configured rate, then
  the device's one and then the API list. On Windows, asking for a rate the endpoint does not accept
  made Mixxx show an error and close.
- `SoundManager::reinitializeDevices()`, `getDefaultOutputDevice()`, `getDevice()`,
  `getSubstitutedDevices()`, and `SoundDeviceEnumerator::getDefaultOutputDevice(api)`.

## 7. Full screen on Linux

The full screen action pointed at the hidden `QWidget` window of `MixxxMainWindow`, while the QML
skins are drawn in the window created by `QQmlApplicationEngine`, so it did nothing. Now
`QmlApplication` keeps its window and `QmlApplicationProxy::setWindowFullScreen()` applies it;
`MixxxMainWindow::slotViewFullScreen()` uses it first.

## 8. Keyboard shortcuts

`src/controllers/keyboard/keyboardeventfilter.cpp`: the *fallback* to `en_US.kbd.cfg` passed
`resourcePath` (which ends in `res/`) instead of `resourcePath + "keyboard/"`, so it looked for
`res/en_US.kbd.cfg` and ended with `starting without shortcuts`: **the user had no shortcut at all**.
One-line fix; verified with Ctrl+P.

### 8.1 Typing in the AutoDJ panel fired the shortcuts

Once the shortcuts were active a side effect appeared: in the AI instruction box (AutoDJ panel)
**nothing could be typed**, and every letter moved the crossfader, started playback or fired
hotcues. The 26 single-letter mappings of `en_US.kbd.cfg` (`h`/`g` crossfader, `d` play, `f` cue,
`z`-`v`/`m` hotcues, `q`/`w`/`e`/`u`/`i`/`o` loops, `a`/`s`/`j`/`k` beatjump, `p` load track,
`t`/`y` PFL, `b`/`n` parameter, `r` edit, `l` play) clashed with typing.

Cause: the library widget tree is **painted** inside a QML item (`mixxx::qml::QmlLegacyLibraryItem`)
and lives **offscreen** (`WA_DontShowOnScreen`), with clicks and keys forwarded by hand. The
`KeyboardEventFilter` is installed on the whole application (`app->installEventFilter(...)`), so it
sees the keys before anyone else, and its guard to avoid stepping on typing was
`isEditableTextInput(QGuiApplication::focusObject())`, which **cannot see** a widget that is not in
the window hierarchy: it returns the QML window. With the box focused, the filter still treated the
letters as shortcuts and also **swallowed the key**.

Fix, following the *callback* pattern the project already uses:

- `KeyboardEventFilter::isEditableTextInput()` becomes a public static method.
- `KeyboardEventFilter::registerEmbeddedTextInputFocusQuery(std::function<bool()>)`: whoever paints
  offscreen widgets registers a query, and the filter's guard becomes
  `isEditableTextInput(focusObject()) || (s_embeddedTextInputFocusQuery && s_embeddedTextInputFocusQuery())`.
- `QmlLegacyLibraryItem` registers it in its constructor (and clears it in the destructor) returning
  `isEditableTextInput(m_pFocusedWidget)`, which is the widget that `updateEmbeddedFocus()` assigns on
  every click. That way the state is read live and there is nothing to get out of sync.

With that, typing in the box works and the shortcuts stay active when focus goes back to the track
table (a click on the table focuses the viewport, which is not a text field). Verified by the user on
screen.

### 8.2 The space bar triggered "BIG LIBRARY" while typing

A loose end was left from the previous section: typing worked now, but **the space bar triggered
"BIG LIBRARY"**, because space *is* the *Maximize Library* shortcut (an upstream decision, it is in
`res/qml/ApplicationMenuActions.qml` and in `res/keyboard/en_US.kbd.cfg`).

The keyboard filter was not the only path to cover:

- the QML `Shortcut`s (`Toolbar/ApplicationMenuShortcuts.qml`) are resolved by Qt **before** the key
  is delivered to the item, so they do not go through `KeyboardEventFilter`;
- and the `Action`s of `ApplicationMenuActions.qml` are **QtQuick** ones, which **register the
  shortcut of the Action itself** on their own (the skin instantiates `MainMenuBar`, so they are in a
  menu). Blocking only the `Shortcut` was not enough — it was tried and it still fired —.

Solution, with a single source of truth:

- `KeyboardEventFilter::isTextInputFocused()`, which the filter itself uses and which is exposed to
  QML as the property `Mixxx.Application.textInputFocused` (with `NOTIFY` from `focusObjectChanged`
  and from a notice that `QmlLegacyLibraryItem` sends when the focus of its painted widgets changes).
- The block in `configuredMenuShortcut()`, the only place where the 32 menu shortcuts are defined:
  while typing it returns `""`, and that disarms both the Action's shortcut and the QML `Shortcut`.
  When the focus leaves the box, the binding is re-evaluated and the shortcuts come back.

## 9. Numark Mixtrack Pro II controller

- `res/controllers/Numark Mixtrack Pro.midi.xml`: the `[Flanger]` and `[ChannelN],flanger` groups do
  not exist in Mixxx 2.x; the FX knobs and LEDs now point at
  `[EffectRack1_EffectUnitN_Effect1]`. The script uses `fxSlotForDeck(deck)`.
- `res/controllers/Numark-Mixtrack-Pro-scripts.js`: the timer called `jogWheelStopScratch()` **without
  the deck**, and `scratchDisable(undefined)` ended in `groupForDeck(-1)`, which fails an assertion
  (`DEBUG_ASSERTIONS_FATAL`) and **closed Mixxx**. The deck is passed and a guard is added in both
  Mixtrack scripts.
- Usage note: the platter only scratches with the deck playing if its vinyl/scratch mode is on
  (note `0x48`/`0x50`, with LED); off it is *nudge*, like a CDJ.
- The tests `src/test/{neondeckskin,pulsedeck_skin,harmonicmixing,aimixplanner}_test.cpp` cover the
  skin loading and the selection logic.

## 10. Deck and menu interface

- The hotcue pad numbers covered the deck settings (stars, slip, quantize, curpos, eject, repeat,
  keylock): `deckSettingsTopOffset` in `res/skins/*/Deck/OverviewRow.qml` (and the matching
  background panel) moves the block when the pads are visible.
- Dead buttons: `beatloop_size_double/halve` and `beatjump_size_halve/double` do not exist (the real
  ones are `loop_double`/`loop_halve`), so the size *spin box* did not advance.
  See `res/qml/Deck/BeatJump.qml` and the `TransportLoopBeatjump*` of the three skins.
- The menu sandwich button went grey without its lines showing: the icon was the light SVG
  `btn__menu.svg` over the grey background of the open menu. It is now drawn with the icon font and
  coloured according to state (`PulseDeck` and `NeonDeck`; in `LateNightQML` it stays the same because
  that theme does not have the icon font).

## 11. Packaging

- `packaging/CPackConfig.cmake`: build and packaging of the `.msi` (MSVC + Ninja + WiX) and of the
  `.deb`. The global `wix.exe` is a .NET *tool* and needs `DOTNET_ROOT` pointing at the SDK.
- `packaging/CPackDebFfmpegExtraAlternatives.cmake` (new): allows the FFmpeg
  `-extra` libraries as alternatives in the `.deb` dependencies.

The local build scripts (`build_windows_release.cmd`, `CMakeSettings.json`) are in the project's
`.gitignore`, so **they are not included**: everyone builds their own command with the data of
section 12.

## 12. How to build and verify

```powershell
# Windows (MSVC + Ninja + vcpkg from buildenv), from a console with vcvars64
cmake --build C:\path\build\x64__legacy --config RelWithDebInfo
# and for the installer:
cpack -G WIX -C RelWithDebInfo
```

```bash
# Linux
cmake --build build --config RelWithDebInfo --parallel
cpack -G DEB
```

⚠️ **Never interrupt a build** and, after changing a *header* that is part of a class,
rebuild clean (`--clean-first`) if crashes that move around appear: an old `.obj` with the old class
size causes memory corruption (the real case is in `PENDIENTE.md` §2.1).

⚠️ **When packaging the `.msi`, call `vcvars64.bat` on a line of its own.** vcpkg copies the DLLs
walking the executable's import graph and needs `dumpbin` in the `PATH`. If everything is chained
on a single `cmd` line —

```bat
rem WRONG: cmd expands %PATH% before vcvars64 runs, and the set overwrites PATH
cmd /c "call vcvars64.bat >nul && set "PATH=<cmake>;<ninja>;%PATH%" && cpack -G WIX"
```

— the `set` writes the `PATH` **previous** to vcvars and wipes the MSVC tools directory (where
`dumpbin.exe` and `cl.exe` live). The build does not fail (CMake caches the absolute paths of
`cl.exe` inside `build.ninja`), but the vcpkg script does look for `dumpbin` in the `PATH` and only
warns with `resolve : Neither dumpbin, llvm-objdump nor objdump could be found`; CPack finishes all
the same and **produces an MSI with no DLL at all**, which installs fine and fails to open with
`0xC0000135` (a DLL is missing). Symptom to recognise it: the MSI weighs far less than usual
(~74 MB instead of ~115 MB). It is worth putting `where dumpbin || exit /b 1` before `cpack`.
Full detail in `PENDIENTE.md` §6.17.

## 13. Notes

- **Style, not copied art:** the visual goal is the *style* of Serato/VirtualDJ. All the assets are
  original or freely licensed (Rajdhani and Font Awesome Free, SIL OFL / CC BY 4.0 / MIT, with their
  licence files in `res/fonts/`). No graphic file or logo is copied from VirtualDJ/Serato.
- **API keys:** there is none in the repository. The one for the AI AutoDJ is stored in the user's
  preferences (system vault via QtKeychain).
- **`PENDIENTE.md`** is the work log: it contains local paths, log captures and the reason behind
  every decision. It serves as a backup, not as documentation for third parties.

---

## 12. Stem generation (Stems tab)

Mixxx already knew how to **play** stems, but not how to create them. Now it generates them too:

- **External worker** `res/stems/stems_worker.py` (+ `stem_mp4.py`): separates with
  `demucs-onnx` (ONNX Runtime, no PyTorch) and writes the Native Instruments container: 5 tracks
  (the mix and the four stems) and the JSON manifest in `moov/udta/stem`, with the `stco`/`co64`
  patched because the `moov` grows. Progress is published as lines (`STATUS`, `PROGRESS`, `RESULT`,
  `DONE`, `ERROR`) and the `probe` subcommand reports on the environment so it can warn before
  starting.
- **Service** `src/stems/stemgenerator.{h,cpp}`: a queue of one job at a time, postponed while music
  is playing, launched with QProcess, with the CUDA DLLs from the Python wheels added to the PATH and
  output next to the track (by default) or in `<config>/stems/<hash>/`. The queue is shown with
  `src/stems/stemqueuemodel.{h,cpp}`.
- **Interface**: **Stems** tab in Preferences (`res/qml/Settings/Stems.qml`) with the hardware notice
  and the queue with progress, and the *Stems > Generate stems / Regenerate stems* entries in the
  library context menu (WTrackMenu).

Notice shown by the tab: without a GPU the separation is done on the CPU, uses much more and takes
about five times as long. Measured on the development machine (243 s track, `htdemucs` fp32):
**22.4 s with an RTX 3060 (10.9x real time)** against **102.4 s on CPU (2.4x)**, with the sum of
stems at -30.9 dB from the original in both cases.
---

# 14. PulseDeck deck taken from the experimental skin (15-10)

- **Skin startup bug** (`Toolbar.qml` in PulseDeck, NeonDeck and LateNightQML): a
  `function onEnabledChanged()` inside a `Connections` clashes with the `enabled` property that
  `Connections` already declares, and the QML compiler rejects the whole file ("Duplicate method
  name"), so **the skin did not load**. Replaced by a `Binding` that mirrors
  `stemGenerator.enabled` on the stems button.
- **Overlapping key controls**: the slot of `KeyControlsPlaceholder` was stretchy
  (76-111 px) and the `RowLayout` squeezed its children when the row did not fit. Now the slot is
  fixed (76 px) in the three `FullDeck.qml` and in the three `TransportLoopBeatjumpCompact.qml`, and the
  vinyl controls (158 px fixed width) have a row of their own in PulseDeck or hide
  while they do not fit in the rest of the skins and in the compact rows.
- **PulseDeck deck** (`res/skins/PulseDeck/Deck/FullDeck.qml`): it is the composition of
  `LateNightQML` (large platter with the title rows beside it, transport below, pitch on the
  edge) keeping PulseDeck's ornaments (accent strip in the deck colour, deck colour
  in BPM and key, icons, A/B/C/D badge) and the mirroring of the B/D decks, added to the copied
  deck as `mirrorControls`. `fullDeckHeight` goes from 230 to 206 px, which is what it measures.
- **Sync button**: it says **SYNC** instead of drawing a glyph (`RatePlaceholder.qml` and
  `RateCompact.qml`), like VirtualDJ.
- **Stems worker**: `StemGenerator::resolvedWorkerScript()` now also looks next to the
  executable, in `<app>/../share/mixxx/stems/` and in the configuration directory: Mixxx only
  knows the resource path if `--resource-path` is passed to it, and without it it resolved
  `stems_worker.py` relative to the current directory (hence the "No se encuentra .../stems_worker.py"
  when checking the environment).
- **`wsl_rebuild_deb.sh`** at the repo root: builds and packages the `.deb` inside WSL
  reusing `/home/mixxx/mixxx/build` and copies the package to `dist\`.
- **Track Advisor panel icons**: the experimental skin (`LateNightQML`) did not have the
  Font Awesome glyph map in its theme, so the ✨ icon of the header and the ⟳ reload button
  **were not drawn** (their `iconName`s did not resolve). The map was added (64 glyphs) and the
  `iconFontFamily` family to its theme. Besides, the reload button now has a fixed minimum width in the
  three skins, so that a narrow header does not squeeze it until it disappears.
- **Stems column of the library**: it stayed at the end because the placement was marked as done
  even if it had not been able to move. Now the mark is a **version** (`stems_column_placed = 2`):
  the profiles that went through the previous rule are placed **one more time** and from then on
  wherever the user drags it is respected.
- **STEMS panel of the toolbar**: pressing the button with the panel open **did not close it** (it
  reopened it). Now it uses the same helper as the settings drop-downs, with the mark
  `wasPopupOpenOnPress` on the button so as not to reopen it with the click that has just closed it.
- **Python environment for the stems**: `res/stems/requirements.txt` and
  `res/stems/setup_venv.py` are added, which create the environment in `<config>/stems/venv` (the
  folder Mixxx queries by default) and install ONNX Runtime and the separator. Before there was no way
  to install it from the repo.

---

# 15. Stem engine of its own, installable from Preferences

Generating stems required having Python and the libraries set up by hand. Now the **Stems** page
offers to **download a complete engine** that carries its own interpreter, ONNX Runtime, the
separator, `ffmpeg` and the model inside, so that nothing needs to be installed on the machine.

- **The live page is C++, not QML**: `src/preferences/dialog/dlgprefstems.cpp` (Qt Widgets),
  registered in `dlgpreferences.cpp`. The file `res/qml/Settings/Stems.qml` **is not shown**
  (the QML is embedded in the executable and that page is the C++ one). That is why "matching the
  skin" means using the same widgets as the rest of the page (`QGroupBox`, `QFormLayout`,
  `QListWidget`, `QProgressBar`, `QPushButton`), not `Theme`/QML.
- **Installer** `src/stems/stemengineinstaller.{h,cpp}` and `src/stems/ziparchive.{h,cpp}`: reads the
  manifest `res/stems/engine.json` (CPU/CUDA variants and models with their sizes), checks the
  network **before** starting host by host, downloads with retries **from scratch** (never resumes a
  cut file), verifies the `sha256`, unpacks the ZIPs with zlib in 50 ms slices so as not to freeze the
  interface, writes the `_pth` of the embedded interpreter (`../app`, `../site-packages`), copies the
  worker and the `huggingface_hub` substitute, and leaves `installed.json` with version, variant and
  model.
- **Suggested variant**: an **asynchronous** GPU probe (never blocks startup) leaves the CUDA
  variant set when there is an NVIDIA card, unless the user chooses another one or an engine is
  already installed.
- **The generator prefers the engine**: `StemGenerator::resolvedPython()`, `resolvedFfmpeg()` and
  `resolvedWorkerScript()` look first inside the engine folder, and the job's `PATH` includes the
  NVIDIA `bin`s that come in the wheels.
- **Resource paths**: `m_pConfig->getResourcePath()` is used (the already resolved path, which in a
  development tree comes from `CMakeCache.txt`), not `CmdlineArgs::getResourcePath()`, which only has
  a value if `--resource-path` is passed. With the previous code, in a development tree the page
  found neither the manifest nor the worker.
- **When the installation finishes** the page probes the environment again, so that the
  *Environment* box and the generator status reflect the freshly installed engine without having to
  close and open Preferences.
- **Isolation**: the package is called, installed and registered separately (`Mixxx Stem Engine` with
  its own GUID; `.deb` `mixxx-stem-engine` in `/opt/mixxx-stem-engine`), so that trying it **does not
  replace** an installed Mixxx. Scripts of this phase: `build-msi.cmd`, `package-msi.cmd`,
  `wsl-build-deb.sh` and `compile-check.cmd`.
- **Build trap that cost a while**: `dlgprefstems.cpp` includes its `moc_dlgprefstems.cpp` by
  hand and Ninja does not always record that dependency. If members are added to `DlgPrefStems`, the
  object of `dlgpreferences.cpp` is left with the **old size of the class**: `new DlgPrefStems`
  reserves too little and the constructor writes outside, with an access fault in `Qt6Core.dll` when
  opening Preferences. The affected `.obj`s (the ones that include the header) have to be deleted and
  rebuilt.
- **Verified**: installing the CPU variant from Mixxx (237.9 MB, 16 files) the engine folder
  ends up at 576.9 MB and its embedded interpreter imports `onnxruntime`, `numpy`, `soundfile`,
  `soxr` and `imageio_ffmpeg` (the 87 MB `ffmpeg` travels inside the wheel) with no Python at all on
  the machine.
- **Pending**: on Linux the page warns and does not offer to install, because the manifest is
  `win_amd64`; a manifest with the python-build-standalone `.tar.gz` files is needed, another
  `ffmpeg` and `chmod +x` when unpacking.

## 16. The failure when writing the container (WinError 2)

Symptom reported: the separation advances, reaches *encoding and writing the container* and ends
with `writing the container failed: [WinError 2] El sistema no puede encontrar el archivo
especificado`. No `.stem.mp4` was generated.

- **Cause**: `stem_mp4.read_metadata()` read the tags with **`ffprobe`**, a program that the
  engine **does not** ship (only the encoder travels in `imageio_ffmpeg`). It looked for an `ffprobe`
  next to `ffmpeg`, then in the `PATH` and, if it did not find it, it kept the bare name `ffprobe`
  and launched it: that is a `[WinError 2]`, and it happened **after** the separation, in the last
  step of the job. On the development machine it was not seen because there is a system `ffprobe`
  (winget) in the `PATH`; on a laptop with no ffmpeg installed, always.
- **Reproduced and verified**: with ffmpeg out of the `PATH` and the ffmpeg path broken on purpose,
  the installed worker gives exactly the user's error (exit code 6); the same command with the fixed
  code ends in `DONE` and leaves a 1.9 MB `.stem.mp4` whose own verifier accepts
  (5 tracks, manifest and correct offset tables).
- **Fix, in three layers**: `read_metadata()` asks `ffmpeg` itself for the tags
  (`-f ffmetadata`), which is the only program the engine guarantees, and never drops a job
  because of the tags; the worker resolves the encoder with `resolve_ffmpeg()` (if what it gets
  is not a launchable program, it asks `imageio_ffmpeg`, which knows where its own is) and checks it
  **before** separating, so as not to lose minutes of work because of a path; and
  `StemGenerator::resolvedFfmpeg()` no longer propagates a non-existent path.
- **The first suspect was not the culprit**: `StemEngineInstaller::ffmpegFor()` returned the
  first file of `binaries` that was not `.txt`, and there `README.md` comes before
  `ffmpeg-win-x86_64-v7.1.exe`. It was fixed all the same (it looks for the `ffmpeg` prefix), but an
  `.md` does not give error 2 but **193** (`no es una aplicacion Win32 valida`), so it was not the
  reported failure.
- **The worker now travels with the application**: `resolvedWorkerScript()` prefers the copy in
  `share/mixxx/stems/` over the one that was left inside the engine. Before, a fix in the worker only
  reached the user by downloading 240 MB of engine again; now installing the new Mixxx is enough. For
  the worker to work from either of the two folders, the application passes it
  `STEM_ENGINE_MODELS` and the `huggingface_hub` substitute can resolve the `models` folder from the
  variable, from its own location or from the interpreter's one.
- **Test package**: the MSI is packaged with `CPACK_PACKAGE_VERSION=2.7.1` (the project is
  2.7.0) because with the same upgrade GUID and the same version Windows refuses to install
  on top with error 1638.
- **The test `.deb` can now be launched and uninstalled**: as it installs under `/opt`, there
  was nothing in the `PATH` nor in the menu, and it looked like it "was not installed" (`mixxx` was
  not a command) and it could not be removed either (`apt remove mixxx` is not the package name).
  The isolated package carries `packaging/isolated-deb/{postinst,postrm}` (passed with
  `CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA` from `wsl-build-deb.sh`): the `postinst` creates
  `/usr/local/bin/mixxx-stem-engine` and a menu entry of its own —`Mixxx (motor de stems, pruebas)`,
  with absolute `Exec` and `Icon`—, and the `postrm` removes it. The official package uses none of
  this.
- **Detail of the `postinst` `sed`**: the patterns are not anchored to the end of the line
  (`^Name=Mixxx.*`) because the repository's `.desktop` comes out with a carriage return on Windows
  and an anchored `$` matches nothing.

## 17. Blurry library and side panel at fractional scaling

Symptom reported: on a 1080p laptop with the screen scaled (the other machine, at 100 %, looks
fine) **the type of the library and of the side panel** (font tree, tracks, AutoDJ) looks blurry,
while the QML skin and the waveforms look sharp.

- **Cause**: in the QML skins the library is a **Qt Widgets** tree that is not drawn on
  screen, but **painted** inside a QML item (`QmlLegacyLibraryItem`, with
  `WA_DontShowOnScreen`). `renderOffscreen()` created the `QPixmap` at **one pixel per logical pixel**
  (`QPixmap(size)`, without `devicePixelRatio`) and `paint()` drew it as it was. With the window at
  150 % the scene graph scales that map 1.5 times: the library looks like a magnified image, and only
  it, because the rest of the window is drawn by the QML engine at native resolution. At 100 % there
  is no magnification and that is why the other machine looks fine.
- **Fix**: the map is created at the size of the **device** (`ceil(side * devicePixelRatio)`) with
  its `devicePixelRatio` set, so that the painter draws the widgets at the screen resolution and
  `paint()` places it pixel by pixel of the device, without magnifying. The layout of the
  widgets stays in logical pixels (clicks still line up because `QQuickItem` delivers the
  positions in those same units). `itemChange(ItemDevicePixelRatioHasChanged)` is added to
  repaint when the window is moved to a screen with another scale.
- **Verified**: with `QT_SCALE_FACTOR=1.5`, same profile, snapshots of the same crop of the tree
  enlarged 4 times: before, the strokes come out doubled and with colour fringes (an enlarged map),
  afterwards clean. With scaling at 100 % the result is identical to the one before the change.

## 18. The engine on Linux

Until here the Stems page warned on Linux and did not offer to install, because the manifest was
`win_amd64`. Now there is a list of files per platform.

- **New manifest** `res/stems/engine-linux.json`, generated with `tools/make_manifest.py`
  (the same tool from the lab, with three changes):
  - it accepts **several `--platform` tags**, because pip does not expand a `manylinux` family into
    the others and these packages do not match in any: numpy publishes `manylinux_2_28`,
    `imageio-ffmpeg` is still on `manylinux2014` and `soxr` on `manylinux_2_27`;
  - `--runtime-strip-prefix python`: the python-build-standalone interpreter comes inside a
    single `python/` directory, and the engine wants it in `runtime/bin/python3`;
  - `--no-nvidia`: the Linux CUDA variant resolves to `onnxruntime-gpu` 1.26 with cu12 wheels
    while the CPU one goes to 1.31, and as both install the same `onnxruntime` package they
    would overwrite each other's files. It is left out on purpose, instead of offering a half engine.
  - Content: `cpython 3.11.17` interpreter (46.7 MB), 14 wheels and the same models. Installation
    of 438.1 MB with the fp32 model and 287.3 MB with the fp16.
- **The installer** (`stemengineinstaller.{h,cpp}`):
  - picks the manifest by platform (`engine.json` on Windows, `engine-linux.json` on the rest,
    falling back to the first if it is not there);
  - does not keep a variant the list does not have: without this, the automatic CUDA suggestion
    left the selection pointing at a variant that does not exist on Linux;
  - passes the interpreter a `PYTHONPATH` with the engine's `site-packages` folder. On Windows that is
    done by the `._pth` the installation writes; on Linux there is no such file;
  - gives **execute permission** to the `ffmpeg` of `imageio_ffmpeg`, which travels inside a ZIP and
    therefore without the execute bit;
  - **cancel** is no longer confused with failing: the interrupted extraction does not call `finish()`
    and the installation is not marked as done.
- **Tar reader** `src/stems/tararchive.{h,cpp}`: gzip over zlib and tar with the extensions the
  packaging tools use (ustar, GNU long name and long link, PAX headers),
  **symbolic and hard links**, mode of each file and trimming of a prefix. Nothing is written
  outside the destination, just like in the ZIP reader. The extraction reports progress in compressed
  bytes and can be interrupted from the call, which is what cancellation uses.
- **Tests** `src/test/tararchive_test.cpp`, inside the project's suite and without support
  files (the test builds the tar and compresses it itself): prefix trimming, execute
  permission, relative link, long name, rejection of paths that leave the destination, extraction
  without prefix and cancellation. **All four pass on Windows and on Linux** (`mixxx-test
  --gtest_filter=TarArchiveTest.*`), which is the only way to check links and permissions.
  Two notices that cost a round: `QFile::readAll()` without `open()` returns empty, and in Qt 6
  `symLinkTarget()` returns the resolved **absolute** path, so the test reads the link with
  `readlink()` to check that it was stored relative.
- **Real validation** `tools/validate_linux_engine.py`: downloads the interpreter, the wheels and the
  model from the manifest itself, places the worker and the `huggingface_hub` substitute the way the
  installer does, grants permission to `ffmpeg`, and separates a 12 s test track to write
  a `.stem.mp4`. It is what answers the question the manifest cannot: whether the engine starts
  on Linux. **Verified in WSL (Ubuntu 24.04)**: interpreter 3.11.17, 14 wheels, `ffmpeg-linux-x86_64-v7.0.2`
  from the wheel itself, 12 s separation in 19.9 s of CPU and a 1.9 MB `.stem.mp4` that its
  verifier accepts (5 tracks, manifest and offset tables).
- **Verified by the user**: stem generation works **on Linux** installing the engine from
  Preferences and **on Windows**, that is, the whole engine path on Linux (manifest,
  download, `tar.gz` with links, permissions and execution) is also validated from the application,
  not only with the validation script.
- **What still has no explanation**: a few black icons in the Mic/Aux section on Linux. It has been
  ruled out that the SVGs are missing (they are in the package), that there are case discrepancies in
  the paths (128 references checked, 0 discrepancies) and that some declared package does not exist
  (71 out of 71 correct). The data from the affected machine is missing.



## 19. The Windows package that did not start (stale objects, again)

The 2.7.3 MSI **died on startup on Windows**: the splash screen showed and it disappeared. On
Linux the same code was fine, which was the first clue (on Linux GCC keeps dependency files;
with MSVC and Ninja not always).

- **Diagnosis**: the Windows event log gave the failing module —
  `Qt6Core.dll`, `0xc0000005` — which with such a low offset is a corrupted pointer, not a
  Qt failure. The test that separated it: the Mixxx installed in `C:\Program Files` (a previous
  build, same music, same scan) **started fine**, so the failure was in this build.
- **Cause**: `stemgenerator.cpp.obj` was **older** than `stemengineinstaller.h`. That header had
  grown (`m_unpackCancelled`, `manifestHasVariant`, `stripPrefix`) and that file is exactly the one
  that builds the installer, so it reserved the old class size and the constructor wrote
  outside the reserved memory. The corruption showed up later, when scanning the library, and that is
  why the symptom was "it stays on the logo".
- **Why Ninja did not see it**: the sources were being edited **while** a build was running, so
  the timestamps ended up in an order that made Ninja believe the object was up to date. It is the same
  trap that section 15 documents for `dlgprefstems.cpp`, with another cause.
- **Fix**: `find_stale_objs.ps1` (looks for objects older than the changed headers that mention
  them), delete the 47 that came out and rebuild. **Verified by starting the extracted MSI**: it
  survives the scan (100 s with the main window open and 1 GB of memory) where 2.7.3
  died. The check was repeated with the final package (2.7.5) and the detector is run **before**
  packaging, as the rule below demands.
- **Note about the names**: CPack names the file with the *project* version, so the MSI always
  comes out as `mixxx-2.7-alpha-...-amd64.msi`. `package-msi.cmd` renames it when copying it
  (`mixxx-2.7.5-stem-engine-amd64.msi`) so that there is a single copy in the delivery folder and
  it is known which one it is. The `.deb` keeps its name and its version (`2.7.0-alpha`), so to
  install on top of a previous one `apt install --reinstall ./...deb` is needed.
- **Rule for the next batches**: run `find_stale_objs.ps1` **before packaging**, and do not
  edit sources with a build running.


## 20. The engine on Linux with a GPU (CUDA variant)

The Linux manifest was born with a single variant (`cpu`). In this batch the GPU one has been added
and it has been verified **for real** on an RTX 3060.

- **What the variant carries**: the same common files and the same models, but with
  `onnxruntime-gpu 1.26` instead of the normal `onnxruntime`, plus the CUDA wheels it needs
  (`nvidia-cublas-cu12`, `nvidia-cudnn-cu12` and company). 1,894.5 MB in total against the 263.5 MB
  of the CPU one: that is why the GPU one is offered separately and not by default.
- **Two traps of the manifest generator** (`tools/make_manifest.py`):
  - `onnxruntime` and `onnxruntime-gpu` **cannot be shared** between variants: each one needs
    its own. That is what `VARIANT_SPECIFIC_WHEELS` is for.
  - pip extras carry commas, so the `--nvidia-requirements` option asks for them separated
    by `;` and the tool is the one that splits them.
- **What was missing in the code**: the NVIDIA wheels are not in `site-packages` as such, but
  in `site-packages/nvidia/*/lib`, and the Linux loader does not look there. `StemGenerator` now adds
  those folders to `LD_LIBRARY_PATH` (only outside Windows) when setting up the worker's environment,
  together with `PYTHONPATH` and `STEM_ENGINE_MODELS`.
- **Verified in WSL (Ubuntu 24.04) with the RTX 3060** (`validate_linux_engine.py --variant=nvidia`):
  the probe lists `[TensorrtExecutionProvider, CUDAExecutionProvider, CPUExecutionProvider]` with
  `has_cuda: true`, and the separation of the same 12 s of audio drops from **19.0 s (CPU) to 12.8 s
  (GPU)** leaving a `.stem.mp4` that the verifier accepts.
- **Trap when validating**: the manifest talks about variants (`cpu`, `nvidia`), but the worker talks
  about providers (`cpu`, `cuda`, `auto`). Passing `--provider nvidia` fails with `invalid choice`;
  the validation script translates the variant before calling it.
- **What the packages carry**: the `.deb` was rebuilt after this work, so its manifest
  carries both variants and its binary the `LD_LIBRARY_PATH` change (verified: the packaged binary is
  the compiled one, 594 MB with symbols -> 25 MB `stripped`, and the manifest inside
  has the same hash as the one in `res/stems/`). The MSI was repackaged as **2.7.5** so that Windows
  carries exactly the same code, although the library change only acts outside Windows
  (`#ifndef Q_OS_WIN`): on Windows those folders go in `PATH`, which is what it already did.


## 21. The dock icon (Plank) and the launcher that was no good

On Linux the application starts and is visible, but in Plank (the dock) it came out with the generic
icon, and the launcher did not appear where it should. Both causes were measured, not assumed.

- **Cause 1: the generated `.desktop` file was not valid**. The original (`res/linux/org.mixxx.Mixxx.desktop`)
  came with Windows line endings and the `postinst` `sed` only rewrote the lines it
  changed (`Name`, `Exec`, `Icon`, `StartupWMClass`), so `[Desktop Entry]`, `Type=Application` and
  `Categories` kept the carriage return. `desktop-file-validate` rejects it with *"file contains
  at least one line ending with a carriage return, while lines should only be separated by a line feed
  character"* (53 CR bytes in total), and an invalid launcher is ignored: no menu entry and no
  association with the window, hence the generic icon. **Fix**: `tr -d '\r'` over the whole
  file before rewriting it, which also stops depending on where the original comes from.
- **Cause 2: `StartupWMClass` was not the real one**. The original entry declares
  `org.mixxx.mixxx`, which is the Wayland `app_id` (`QGuiApplication::setDesktopFileName`), but on
  X11 Mixxx reports something else. Measured with a virtual X server and `xprop`:
  `WM_CLASS(STRING) = "mixxx", "Mixxx"`. **Fix**: `StartupWMClass=mixxx`, which in the
  case-insensitive comparisons covers both parts (`mixxx` is the instance and `Mixxx` the
  class).
- **And the icon, by name**: it pointed at an absolute path of an SVG outside the theme. Now the PNGs
  the package already carries are copied to the icon theme as `mixxx-stem-engine` (`32x32` to `512x512`)
  and the entry uses `Icon=mixxx-stem-engine`: a name of its own, so it resolves like any
  application and **does not touch** the `mixxx` icons of the official package. The `postrm` deletes
  them, and `purge` was checked again: neither the launcher, nor the entry, nor the icons are left.
- **How all this was checked** (in WSL, without a screen): install the `.deb`, `desktop-file-validate`
  (now without errors, 0 carriage returns), list the icons in
  `/usr/share/icons/hicolor/*/apps/`, and measure `WM_CLASS` starting Mixxx under `Xvfb` and reading the
  window with `xprop`. The check scripts are kept in the session folder
  (`check_icon_desktop.sh`, `check_icon_theme.sh`).
- **What to do after updating**: reinstall the `.deb` and restart Plank (or log out and
  back in), because the dock reads the launchers on startup.
- **A reading that was a false alarm**: on the waveform a box with `CUE` is seen and, below it, a pink
  `1`. It does not say `CUT`: it is the `cue_point` mark of the skin (`LateNightQML` and company draw
  it with the text `CUE`) together with the hotcue 1 badge, and it appears on **every** waveform of the
  track (the top one, the main one and the two platters'), which is what makes it look like it repeats.


## 22. MIC/AUX and effect slots empty on Linux (components bound to their context)

On Linux, the MIC/AUX section came out as empty boxes (the `MIC 1..4` / `AUX 1..4` did not appear) and
the effect slots did not draw their parameters either. On Windows it looked fine. The application
log said it without ambiguity:

```
PulseDeck/MicAux/MicUnit.qml: Cannot instantiate bound component outside its creation context
PulseDeck/MicAux/AuxUnit.qml: ...
PulseDeck/Effects/EffectSlot.qml: ...
```

- **Cause**: those files carry `pragma ComponentBehavior: Bound` (the modern QML mode) and at the same
  time hand their `Component {}` blocks to a `Loader.sourceComponent`. Qt forbids exactly that
  combination: a component *bound* to its context cannot be instantiated outside it, and the `Loader`
  tries to. The item is not created, the `Loader` stays empty and the unit is drawn as a box.
  On Windows it was not seen because the Qt version of the dependency package does not apply that
  restriction.
- **Fix**: remove the `pragma` from those files (`MicUnit.qml`, `AuxUnit.qml` and `EffectSlot.qml`
  in the three QML skins: `PulseDeck`, `LateNightQML` and `NeonDeck`). It is a one-line change per
  file: it goes back to the usual behaviour, which is what the code expects, and it touches nothing
  else.
- **Verified on Linux, with MIC/AUX and effects enabled** (`[Skin] show_microphones=1`,
  `show_effectrack=1`): before the fix, three notices per unit; afterwards, **zero** notices
  `outside its creation context`, and a capture of the window shows `MIC 1..4` and `AUX 1..4` with their
  buttons, just like on Windows.
- **How to reproduce it without a screen**: `Xvfb`, start the binary with `QT_QPA_PLATFORM=xcb`, and for
  the capture close the first-run assistant with `xdotool key Escape` (otherwise what gets
  photographed is the "choose your music folder" dialog, which covers the main window).
- **Pending in the log**: the notices from `qml/ApplicationMenu*.qml` (`TypeError: Cannot read
  property 'length' of undefined`, `Unable to assign [undefined]`), which point at the application
  menus. They are separate noise and do not prevent starting; they are left for the next batch
  together with the detail of the flashing `CUE` mark.


## 23. The CUE mark that flashed over the waveform (with no cue set)

Over the waveform an orange `CUE` box appeared (with its flag) and, beside it, a pink badge with
a `1`. It showed even when the deck had no cue set, and it **flashed**.

- **What it is**: two different marks of the skin. The `CUE` box is
  `Mixxx.WaveformMark { control: "cue_point" }` and the `1` badge is hotcue 1 (on these tracks,
  imported from Serato). At real size, `CUE` + `1` reads as "CUT 1".
- **The flashing, measured on the user's video**: extracting the 8.66 s at 10 frames per
  second (90 in total) and cropping the mark area, the box appears **in only 4
  frames** (0.8 s, 7.2 s and 8.3 s). So it is not an impression: the mark is drawn and erased.
  It matches the fact that it is drawn in the same place as the play marker, which is repainted on top
  every frame.
- **Why it shows with no cue set**: in Mixxx 2.7 the `cue_point` control always has a position (0 if
  none has been set), so the mark always exists.
- **Fix**: the `cue_point` mark is removed from the waveforms of the fork's two skins (`PulseDeck` and
  `NeonDeck`), with a comment explaining the reason. `LateNightQML` is left as it is, because it is
  the reference one.
- **How a video was diagnosed without being able to watch it**: `ffprobe` for the duration, `ffmpeg`
  with `fps=10,crop=...,tile=10x9` to have the 90 frames of the box in a single image and see in
  which ones it appears. It is the quick way to detect something intermittent.


## 24. The images were not installed, and menus proof against a strange singleton

Two small things that came out of the Linux log and of comparing trees.

- **`res/images` was not installed in any package**. The project only installs
  `res/images/icons/` (for the system menu icons) and assumes the rest of the images are
  provided by Qt's resources. But the skins and the QML components are loaded **from disk**, and
  their relative references (`../images/keywheel/keywheel.svg`) point at `share/mixxx/images`, which
  did not exist: hence the `Cannot open: .../images/keywheel/keywheel.svg` and the hole in the key
  circle dialog. **Fix**: an `install(DIRECTORY res/images ...)` rule next to the one for
  the skins, which is 136 files and 2.1 MB. It was checked that the `.deb` carried **zero** files of
  `images/` before the change. Useful surprise: it may cover similar holes in other windows.
- **`Mixxx.Application` in the menus**. On the user's machine, with its Qt version,
  `Mixxx.Application` does not resolve to the Mixxx proxy but to a `QQuickApplication`, so
  `menuShortcut` and `textInputFocused` arrive undefined and **all** the `Action`s of the menus are
  left without a shortcut. On the compilation Linux (Qt 6.4.2) it does not reproduce: the `.deb`
  carries the correct QML and starts without a single notice, so it is an environment thing.
  **Defensive fix**: in `res/qml/ApplicationMenuActions.qml` the proxy is read once into a property
  and every shortcut goes through `configuredMenuShortcut()`, which returns an **empty** shortcut
  when the proxy is not there — and also when a text input has the focus. The first version armed
  the default shortcut instead, and it broke the AI box (the default of *Maximize Library* is the
  space bar), so it was reverted. The Qt version of the affected machine remains to be confirmed
  (`qmake6 -query QT_VERSION`) to know whether anything else has to be done.


## 25. The real icon was the cache, the flicker was the render loop, and the duplicated stem was mine

Three things closed with verification, now on the user's machine.

- **The icon (menu, dock and launcher) was theme resolution, not the files**. The files
  were properly installed and the `.desktop` validated, but the menu and Plank resolved the icon **by
  name** and did not find it: `icon-theme.cache` did not index it. First attempt, regenerate the cache
  with `gtk-update-icon-cache -f -t` from the `postinst`; in the lab it worked, but on the user's
  machine the tool **is not installed** (Mint), the `postinst` skips it and the icon
  kept coming out as the generic wheel (`grep -c mixxx-stem-engine` on the cache gave **0**).
  **Final fix**: the `.desktop` does not use a theme name, it points at the **file** (absolute
  path to the 256x256 PNG inside the prefix), so it depends neither on the cache nor on the tool
  existing. The icons are still copied to the theme and the cache is regenerated **if** the tool
  is there, as an extra for other launchers; the `postrm` leaves everything clean.
- **And the underlying thing was missing: Mixxx did not set a window icon**. Neither `setWindowIcon`
  nor an `.ico` on Linux, so the dock and the task bar, which show the icon **of the window** and not
  the one of the `.desktop`, always fell back to the generic one. Now `main.cpp` loads the installed
  PNGs (from the resource path if it comes in the command line and, if not, from `<prefix>/share/mixxx`
  computed from the executable, because at that point the path is not resolved yet) and calls
  `setWindowIcon`. Verified by measuring the `_NET_WM_ICON` property with `xprop` on the Mixxx window
  on a virtual X server: **absent before, present now** on both windows (splash and main).
- **The waveform flicker was the render loop, not the graphics**. The log filled up with
  `VisualPlayPosition::calcOffsetAtNextVSync "[Channel1]" outdated position request (offset <
  minOffset)`: the drawing asks for the position of the next frame and that position has already gone
  by, so Mixxx clamps it and the waveform stays still for a frame and jumps to the next one. With
  `QSG_RENDER_LOOP=basic` the lines dropped from hundreds to 58, and with the vsync mode already
  forced to timer (which is what `startVSync` does with QML skins) they went down to 2. **Fix**:
  `main.cpp` sets `QSG_RENDER_LOOP=basic` before creating the application, only on Linux and only if
  the user has not set it themselves. Important detail for the future: in QML skins the
  `[Waveform] VSync` key of `mixxx.cfg` **is ignored** (`startVSync` forces `ST_TIMER`), so that
  was not what worked.
- **The duplicated stem was caused by the generator itself**. On finishing, `StemGenerator` called
  `rescanLibrary()`, and the scan added the `.stem.mp4` as one more track: the same song
  appeared twice. It was checked with the user that **the original track already plays the stems**
  (drum, vocals, etc.) and that its column says `STEMS`.
  **Final decision: it is left as it was**, with the automatic rescan. The two rows are seen on
  purpose: they are the notice that the conversion happened, and the user prefers that to the mark
  appearing without more. Hiding the `.stem.mp4` files when their original is next to them (and still
  marking the original) is left **for a next version**; Mixxx's scanner for now does not pair both
  things. The `rescanLibrary()` method also remains available on a button of the Stems page of
  Preferences.

## 26. The effect names left their box, and two things worth taking from upstream

Reported after the first session with the 16 October package: *"when the effects are on and you
pick one, for example filters, the names overlap, they do not fit in the box"*.

- **Cause**: in a collapsed effect unit the slot is about 80 px wide; the effect name is a
  `ComboBox` whose `contentItem` carried `leftPadding: 5` and `rightPadding: 18`. With ~24 px of
  slot left for the name, the available width for the text came out **negative**, and a `Text` with
  negative width does not elide: Qt drew the whole string, which landed on top of the neighbouring
  slot's power button and knob. Seen as `Grap…c EQ` crossing two controls.
- **Fix**: the padding is now adaptive (5/18 when there is room, 2/3 when there is not), the text
  has `clip: true` so it can never leave its box, the drop-down arrow is hidden when the box is
  narrow, and a tooltip shows the full name. In the collapsed slot the name gets **a line of its
  own** above the on/off button and the knob, which is the only layout that leaves it readable: with
  the knob in place the name would have about 10 px.
- **The knob stays**: it is the control that sets how much effect is heard, so it is not an option
  to hide it. That is why the row grew from 30 to 44 px instead of the name being shrunk further.
  The row is 44 px with the name at 10 px; at 48 px the font was 11 px, which cost 4 px for nothing.
- **Two changes from upstream PR #17292** (`lateNight-qml-mic-aux-parity`), ported by hand:
  - `powerWindowLatched` in `res/qml/ControlProxyButtonBehavior.qml`: releasing the primary button
    while the secondary was still held wrote a 0 into the control, so talkover and main mix jumped
    to 0. It now leaves the value alone.
  - Peak hold in `Controls/ImageVuMeter.qml`: the bar keeps a mark on the peak for 500 ms and then
    lets it fall.
  The rest of the PR (a pixel-for-pixel rewrite of the MIC/AUX geometry to match the classic skin)
  was **not** taken: it targets a skin grid this fork does not use, it touches the two files where
  our own pragma fix lives, and it would have to be redone by hand for the fork's skins.
- **Verified** in WSL under Xvfb with the real skin and the real effect list, forcing nothing but a
  test string: the name is drawn inside its box, the knob is present in both states, the expanded
  unit is unchanged, and the log carries no new QML warnings.
- **Packages of 17-10**: `.msi` 2.8.2 (120.824.844 B) and `.deb` (19.966.844 B), both opened and
  checked that they carry the three changed files. Published on the release
  `v2.7.0-alpha-2026-10-17`; the 16-10 one is marked as previous.

## 27. The controller was left without a mapping

Reported right after installing the 17 October package: *"testing with the controller, it is not
taking the buttons; before it worked"*.

- **What the log said**: `Searching for controller mapping
  "C:/Program Files/Mixxx/controllers/Numark Mixtrack 2.midi.xml"` followed by `Could not find ...
  in any mapping path`. The mapping was not being looked up at all: the configuration remembered an
  **absolute path** to the mapping of the Mixxx that used to be installed in `C:\Program Files\Mixxx`.
  This fork installs to `C:\Program Files\Mixxx Stem Engine`, so the path was dead and the device was
  skipped in silence — the controller opened its MIDI ports but no button had a function.
- **Why it is not automatic**: `ControllerManager::slotSetUpDevices()` opens only what the
  configuration says. It checks `[Controller] <device>`, then loads exactly the file named in
  `[ControllerPreset] <device>`, and if there is none it `continue`s. The **hardware** is detected;
  the **mapping** is a decision remembered per device.
- **Fix, in three parts**:
  1. `findMappingForDevice()` in `controllermanager.cpp`: before giving up, it walks the mapping
     paths looking for a file whose `<controller id>` is the device name (compared without case,
     spaces or punctuation). If it finds one, it uses it and writes it into the configuration.
  2. The same lookup runs when the remembered path no longer exists, which is the case that breaks
     when the installation moves.
  3. A device that nobody has configured is disabled by default; when a mapping claims it, it is
     enabled on the first sighting.
- **New preset** `res/controllers/Numark Mixtrack Pro II.midi.xml`: the existing file declares
  `<controller id="MixTrack">` while the hardware reports `MixTrack Pro II`, so nothing could match it
  by name. The new file is the same mapping with the id the device actually reports.
- **Why the package is not called "Mixxx"**: installing into `C:\Program Files\Mixxx` would write on
  top of a real Mixxx — two installations sharing a folder with different upgrade GUIDs, and
  uninstalling one taking the other's files. The isolated package exists so that trying this fork
  does not replace the official one. The problem was the absolute path in the configuration, not the
  folder name, and that is what is fixed.







