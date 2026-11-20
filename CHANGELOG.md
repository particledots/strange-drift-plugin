# Changelog

## 0.1.1 (milestone 1 finished: tested in Logic on the M4 Mac mini and the Intel Mac, 2026-11-19)

- Tested by Bryn: the M4 Mac mini passed every step on 0.1.1; the Intel Mac passed the install, lights, resizing,
  mapper, Ogdoad and save/reopen steps on 0.1.0 and the CPU and smoothness re-check on 0.1.1 (meter about 5-25% window
  closed, 10-30% open; no stepping heard).

- Fewer MIDI messages, same smoothness: an output is sent once it has moved at least 0.2% (32 of 16,384 steps; still at
  most every 6 ms), or after 100 ms for smaller moves. About 300 messages a second at 1x instead of about 950. In the 0.1.0
  test on the Intel Mac the Scripter mapper's work showed clearly on Logic's CPU meter (5% with the mapper bypassed,
  up to about 30-50% with it on).
- The window draws again only the parts that moved, instead of the whole panel 30 times a second; nothing at all while held.
- Nothing else changes: every parameter id, CC number, the mapper (still 0.1.0) and the saved position format are as in 0.1.0.

## 0.1.0 (first build; Intel Mac test showed the CPU use fixed in 0.1.1)

- First build: AU + VST3 MIDI effect, universal (Apple Silicon + Intel), macOS 11 or newer, ad-hoc signed.
- Three slow chaos circuits (ripple about 20 s, tide about 75 s, glacier about 35 min per trip at 1x), each with X, Y
  and Z; blend (tide + glacier − ripple, from Z) with + and −.
- Seven 14-bit CC outputs on channel 15 (changeable): coarse CC 106-112, fine CC 113-119. Z and Blend + / − are worked
  out by the mapper.
- Speed (1/8x to 512x, 1x at the centre), Hold, New start; Lean, Nudge and Nudge from on ripple and tide (Off, mod wheel,
  or the other two circuits' X or Y); glacier has no controls.
- The chaos position is saved with the project; a new copy starts somewhere random.
- The window as approved in the draft, with Lights (default) and Orbits views, resizable from 80% to 200%.
- The Strange Drift Mapper Scripter script (8 lanes: source, Learn target, min, max), working alongside the Ogdoad Mapper.
- Package: the plugin, `Install-Unofficial-Plugin.command` (unchanged, Audio Unit only), `INSTALL.txt`, the mapper, auval and pluginval logs.
