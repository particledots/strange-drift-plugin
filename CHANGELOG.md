# Changelog

## 0.1.0 (milestone 1; built and checked, waiting for Bryn's test in Logic)

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
