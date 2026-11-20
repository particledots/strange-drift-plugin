# Strange Drift

An original AU / VST3 MIDI effect for Logic Pro: three slow chaos circuits (ripple, tide and glacier), each wandering
around two "wings" and switching between them at unpredictable moments, sent out as smooth MIDI CC for drones and slow
modulation. Inspired by the feel of the Nonlinearcircuits Triple Sloths (v2) Eurorack module. All code is our own; it is
not a port or copy of anything, and it is not affiliated with Nonlinearcircuits.

Status: **version 0.1.0 (milestone 1), built and checked; waiting for Bryn's test in Logic on both Macs.**

- **Three circuits.** At Speed 1x one trip around both wings takes about **20 s** (ripple), **75 s** (tide) and
  **35 min** (glacier), as on the module. The timing is never regular: some trips are short, some linger.
  Each circuit has an **X** output (which wing it is in), a **Y** output (how far out it is swinging) and **Z**
  (Y upside down).
- **Speed** moves all three together: 1/8x to 512x, with 1x (the module's times) at the centre; double-click returns
  to 1x. At 512x glacier makes a trip every 4 seconds, so every output can be checked quickly.
- **Lean** (ripple and tide): how long the circuit lingers in one wing. Centre = even; turned right it spends more time
  on the + side (amber), turned left on the − side (blue).
- **Nudge** (− / +) and **Nudge from** (ripple and tide): pushes the circuit with another circuit's X or Y, or with the
  incoming mod wheel (CC 1). As on the module, a strong nudge can bring stretches of regular movement or sudden jumps.
  A circuit is never nudged by itself: its own outputs pushing back on it can make it settle and stop, so each menu
  lists only the other two circuits.
- **Glacier** has no controls: it wanders on its own.
- **Blend**: tide + glacier − ripple (taken from the Z outputs), halved. **Blend +** is the part above zero,
  **Blend −** the part below.
- **Hold** freezes all three. **New start** jumps to a fresh random place.
- **Where the chaos is gets saved with the project**, so a project reopens where it left off. A new copy of the plugin
  starts somewhere random. A duplicated track starts in the same place as the original and the two move in step until
  you press New start on one of them.
- **Lights | Orbits** (bottom right): *Lights* (the default) shows each output as a glowing light (amber above the
  centre, blue below, brighter further out) with a level bar beside it; *Orbits* shows each circuit's butterfly
  (X across, Y up) with the current point as a light. The choice is saved with the project and changes nothing that
  is sent.
- **Resizing**: drag the corner at the bottom right; 80% to 200% (624 x 376 to 1560 x 940), proportions locked, size
  saved with the project.
- **MIDI out channel** (bottom left, default 15). Your own notes and every other message pass through untouched.

## How the outputs reach a knob in Logic

Logic has no CV. Seven outputs go out as 14-bit CC pairs (16,384 steps instead of 128) on channel 15:

| Output | ripple X | ripple Y | tide X | tide Y | glacier X | glacier Y | blend |
|---|---|---|---|---|---|---|---|
| coarse CC | 106 | 107 | 108 | 109 | 110 | 111 | 112 |
| fine CC | 113 | 114 | 115 | 116 | 117 | 118 | 119 |

CC 106-119 have no standard job in MIDI and are not used by Ogdoad (channel 16: CC 12-31, 44-63, 102-105). Z and
Blend + / − are worked out by the mapper, so they cost no MIDI.

The **Strange Drift Mapper** (a Scripter script, in the download as `Strange Drift Mapper (paste into Scripter).txt`,
source in `tools/mapper/StrangeDrift-Mapper.js`) sits in the MIDI FX slot below Strange Drift. It has 8 lanes; each
lane has a Source (Off, Ripple X/Y/Z, Tide X/Y/Z, Glacier X/Y/Z, Blend +, Blend −, Blend +/−), a Target (Logic's
*Learn plug-in parameter*), and Min % / Max %. It moves the learnt knob on any plugin below it on the same channel strip
and stops Strange Drift's CC from reaching the instrument; everything else passes through, Ogdoad's CC included.
Set up once per Mac (saved as a Scripter preset), then chosen from Scripter's preset menu on each track. `INSTALL.txt`
in the download has the steps.

**With Ogdoad on the same track:** MIDI FX slots in the order Ogdoad, Strange Drift, Ogdoad Mapper, Strange Drift Mapper.

Smoothness and CPU: each output is sent only when its value changes, at most once every 6 ms, coarse part only when it
changes, placed at its exact sample. At 1x glacier never moves more than 1 step of 16,384 per message. Strange Drift
sends about 950 MIDI messages a second at 1x and about 2,200 at 512x. The engine uses under 0.1% of one core.

## The circuits

Each circuit is the textbook Lorenz system (1963), the classic two-winged "butterfly", chosen because it can never run
away: from any starting point, with any of the pushes used here, it stays inside the same bounded region. Lean and Nudge
add a push that is soft-limited well below the point where a circuit would settle to a standstill (proved in the engine
tests across every Lean / Nudge combination). The circuits move in fixed 1 ms steps, so they take exactly the same path
at every sample rate and block size. Nonlinearcircuits' own schematic was not studied (no licence for it was found);
only its public description of the module was used.

## Folders

| Folder | What it is |
|---|---|
| `engine/` | The three circuits (`Chaos.h`), blend, the clocks and the saved position (`Engine`), the CC numbers (`Outputs.h`) and the CC stream (`CcStream`, from Ogdoad). Plain C++, no plugin code. |
| `plugin/` | The plugin wrapper: what Logic sees, the list of controls, and the panel (`PluginEditor`). `plugin/preview/` draws the window to pictures and checks the plugin without Logic. |
| `tools/mapper/` | The Scripter mapper script. |
| `tools/install/` | `Install-Unofficial-Plugin.command`, the same installer as the other particledots plugins (Audio Unit only). |
| `.github/workflows/` | The automatic build. |

## Getting the plugin

1. Open the **Actions** tab of this repo and click the latest **Build Strange Drift** run.
2. At the bottom, under **Artifacts**, download **StrangeDrift-macOS** and unzip it (twice: the download is a zip containing `StrangeDrift-macOS.zip`).
3. Inside the `StrangeDrift-macOS` folder are `StrangeDrift.component`, `StrangeDrift.vst3`, `INSTALL.txt`, `auval.log`,
   `pluginval-AU.log`, `pluginval-VST3.log`, `Strange Drift Mapper (paste into Scripter).txt` and `Install-Unofficial-Plugin.command`.
4. In Terminal, once, clear the download block: `xattr -dr com.apple.quarantine ~/Downloads/StrangeDrift-macOS/*`
   (adjust the path if you unzipped elsewhere).
5. **Easy install (Logic, Audio Unit):** double-click `Install-Unofficial-Plugin.command`. It asks for your Mac
   password once, installs the AU for all users and restarts the AU registrar.
   Or by hand: copy `StrangeDrift.component` to `~/Library/Audio/Plug-Ins/Components/` and `StrangeDrift.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
6. In Logic it is a MIDI effect: MIDI FX slot of a Software Instrument track, **Audio Units > particledots > Strange Drift**.

## What the build checks

- **Engine self-test** (Linux, before the Mac build): the trip time in circuit units over 28,000 trips; ten simulated
  hours at 44.1, 48 and 96 kHz with random block sizes (always inside −1..+1, never a restart, keeps moving in every
  window, both wings visited, trip times inside the module's ranges, blend always equal to its definition); Speed
  scaling exactly from 1/8x to 512x; all 125 Lean / Nudge combinations (both circuits, every source, the mod wheel held
  up and moved, the two Nudge knobs the same way and opposite ways) keeping moving, staying in range and visiting both
  wings; Lean leaning the right way; exactly the same chaos at every sample rate and the same MIDI at every block size;
  save and reload continuing exactly, broken saved states refused; New start; Hold; the mod wheel; the CC rules (6 ms
  cap, coarse before fine, small steps); CPU.
- **Mapper test**: the real mapper script run in a stand-in for Scripter, fed by the engine's real MIDI stream:
  every kind of source (X, Y, Z, Blend +, −, +/−), Min/Max, upside-down lanes, 14-bit detail, notes passing, Strange
  Drift's CC stopped, Ogdoad's CC passing, the channel setting, and the Strange Drift Mapper chained with a copy of the
  Ogdoad Mapper in both orders.
- **Mac build**: universal binary (Apple Silicon + Intel), ad-hoc signing, Apple's `auval` (as a MIDI effect,
  `aumi Sdr1 Pdot`), pluginval at strictness 5 on the AU and the VST3, the package contents.
- **Window picture and plugin checks**: the real window drawn to pictures (artifact **StrangeDrift-window-pictures**:
  default, running, held, 512x, Orbits view, small / medium / large size), every control moving only its own parameter,
  the menus, save and reload (settings, chaos position, Lights | Orbits, window size), projects without or with a broken
  position, two new copies starting in different places, New start, the size limits and locked proportions, every
  control inside the window at 80%, 150% and 200%, MIDI passing through (Ogdoad's CC included), CC on the chosen channel,
  the mod wheel, bypass, Hold and block sizes.

## Fixed names

Plugin code `Sdr1`, manufacturer `Pdot`, bundle id `com.particledots.strangedrift`, target `StrangeDrift`, engine library
`strangedrift_engine`. Parameter ids: `speed`, `hold`, `lean1`, `nudge1`, `nudge1From`, `lean2`, `nudge2`, `nudge2From`,
`outChannel` (1 = ripple, 2 = tide). Saved with the project besides the parameters: `chaosState`, `orbitsView`,
`editorWidth`. The CC numbers above, the Nudge from menu orders and the mapper's Source menu order are fixed too.
These never change, so saved projects keep finding the plugin and its settings.
