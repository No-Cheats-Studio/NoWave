# NoWave â€“ To-Do (from PoC to product)

Goal: NoWave should become a professionally designed, cleanly operable product.
The order corresponds to priority. The current state is described in [roadmap.md](roadmap.md).

Legend: `[ ]` open Â· `[~]` in progress Â· `[x]` done Â· `[!]` blocked

---

## 1. Rooms, Presets & Textures

Goal: Rooms and surfaces look and sound realistic and high quality.

- [x] Align material values with common octave absorption coefficients (Low/Mid/High clearly defined)
- [x] Make preset names, descriptions, and material names consistent and professionally worded
- [x] Revise procedural surface textures (scale, noise, color palette, material appearance)
- [x] Define a unified color palette for UI and 3D view
- [x] Add further room presets: living room, underground garage, classroom (a church can follow)
- [x] Custom rooms: material assignment beyond the default material: imported OBJ/FBX materials are auto-mapped by name keyword (`MaterialLibrary`), else Default

## 2. Sound Visualization as a "Cloud"

Goal: Not individual rays, but a cloud that shows **where** the sound is **how loud**
and **from which direction** it arrives at the listener.

- [x] Take energy per segment from the GPU calculation (`RaySegment`: broadband, Low/Mid/High, reflection index)
- [x] Cloud rendering as an energy-density voxel field (segment power x path length per voxel, 0 dB = direct sound at 1 m): density and brightness correspond to the sound energy at the location
- [x] Color scale for the level in dB with legend (`LevelLegendComponent`, -60..0 dB, shared `levelRamp` in `Palette.h`)
- [x] Direction display: arrows per voxel show the direction the sound travels (power-weighted), plus a heatmap sphere (24x12 bins) around each receiver
- [x] Filter by arrival time (Sound View "Arrival": All / Direct / Early <= 3 reflections / Late) (direct sound, early reflections, reverb tail)
- [x] Rendering stays smooth at maximum Ray Density (voxel grid of max 56 cells along the longest axis, rebuilt only on change; not yet measured on hardware)
- [x] "Show Rays" remains available as a debug view (independent "Show rays" toggle, can be combined with the cloud)

## 3. Operation, Menus & Accessibility

Goal: Every function is understandable without a manual and easy to reach.

- [x] All controls with unit in the label and a short tooltip explanation
- [x] Name dropdown entries clearly (e.g. "Vocal" â†’ "Vocal (single source)")
- [x] Group the control panel clearly: source, receiver, room, output, raytracing
- [x] Keyboard operation and logical tab order
- [x] Minimum size for click targets and WCAG AA contrast
- [x] Note that higher Ray Density and Max Bounces cost more computation time
- [ ] Calculation status display (running / done / GPU not available)
- [x] Ensure tooltip component in the editor (prerequisite for tooltips)

## 4. Physical Calculation (GPU only)

Goal: Every sound is calculated fully physically on the GPU. There is no CPU replacement.

- [x] Remove CPU direct-sound fallback; the IR is always generated on the GPU
- [x] Clear status instead of silent fallback (without GPU or scene: no IR, signal stays dry, status retrievable)
- [~] Verify full physics in the compute shader: runtime, 1/r attenuation, absorption per band and reflection, scattering, source and receiver directivity, receiver hits
- [x] Export energy and reflection index per segment for visualization
- [ ] Automated check of C++â†”GLSL layouts (sizes and offsets)
- [ ] Validation against reference values (reverberation time per Sabine/Eyring)

## 5. Product Maturity & Quality

- [ ] Automated tests (see roadmap.md, Phase 0)
- [ ] Audio thread fully allocation-free (`ReverbEngine`, `wetBuffer.setSize`)
- [ ] Couple tail length to the actual sample rate (`getTailLengthSeconds`)
- [ ] Practically verify the Linux build
- [ ] CI build on every push for Windows and Linux
- [ ] Versioning: keep `CMakeLists.txt` and release tag in sync
- [ ] Code signing for Windows binaries

## 6. Open Decisions

- **UI language:** The interface is in English, the documentation was German. Should the UI be localized?
- **Behavior without GPU:** Proposal: the plugin keeps running dry and shows the status.
- **Target audience:** Musical result or physically verifiable values as the benchmark?

---

## Change Log

| Date | Change |
|---|---|
| 2026-11-25 | Initial version of the product to-dos |
| 2026-11-25 | Section 2: sound cloud, dB legend, direction heatmap, arrival filter; `show_ray_paths` replaced by `sound_view`, `sound_view_band`, `sound_view_arrival` |