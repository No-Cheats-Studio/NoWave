# NoWave
[![Build and publish release](https://github.com/No-Cheats-Studio/NoWave/actions/workflows/release.yml/badge.svg)](https://github.com/No-Cheats-Studio/NoWave/actions/workflows/release.yml)
C++20 VST3 & Standalone Audio Plugin built with JUCE and CMake.

NoWave is a product of **no-cheats.de**. It is an audio effect for exploring and
shaping spatial acoustics: from room geometry, surface materials, sound sources, and
microphone positions, the plugin calculates a room impulse response and applies the
resulting room sound to the audio signal. This makes it possible to try out different rooms and
microphone or speaker arrangements directly in a DAW or with the standalone app, without
having to build a real room.

The calculation uses Vulkan raytracing on the GPU. Custom room geometries can be imported as
OBJ or FBX files; alternatively, three example rooms are available.

**Development:** The project was developed with the support of **ChatGPT Luna 6**.

## Requirements

- CMake >= 3.25
- Git
- Visual Studio (MSVC with C++ Desktop Development) on Windows
- Vulkan SDK with `glslc` on Windows (`VK_SDK_PATH`); for Linux package dependencies, see below

## Build (Windows)

Alternatively, simply run `build.bat`; the script configures the release preset and
builds VST3 and Standalone.

```powershell
# Configure (downloads JUCE automatically the first time and compiles the compute shader)
cmake --preset windows-release

# Build (VST3 + Standalone)
cmake --build --preset windows-release
```

The finished artifacts are then located at:
- `build/release/NoWave_artefacts/Release/VST3/NoWave.vst3`
- `build/release/NoWave_artefacts/Release/Standalone/NoWave.exe`

## Build (Linux)

The Linux build and the Linux packages are currently **not tested on Linux**.
The workflow is set up; the Linux build, the Debian installation package, and runtime
on a Linux system still need to be verified in practice.
Locally, you can configure with `cmake --preset linux-release` and build with
`cmake --build --preset linux-release --parallel 2`. You need the
Vulkan SDK or `libvulkan-dev` and `glslc`, as well as the JUCE development packages
for ALSA, JACK, X11, FreeType, Fontconfig, and OpenGL.

The Vulkan raytracing engine calculates the room impulse response in a background worker.
If no Vulkan compute device is available at runtime, a direct sound connection
is used as a fallback.
With **Show Rays**, the calculated ray paths can be displayed in 3D space.
**Ray Density** sets the number of emitted rays from 128 to 8192:
higher values improve statistical quality but require more computation time.
**Max Bounces** limits the number of reflections per ray (0–16). Rays start
distributed across the entire sphere and are weighted according to the soft, forward-facing
cardioid directivity of the speaker. As a result, there is no hard emission cone;
the energy fades out to the sides and to the rear. Visible connection segments only show
unobstructed sound paths to the receiver. Transparency and
line thickness reflect the relative path energy.

## Acoustic Configuration

In the lower control panel, you can select the output and input. **Vocal** uses a
directional single source, **Speaker Box** uses two sources aligned at ±30°, and **PA**
uses two separate stereo speakers. As input, there is a microphone with an adjustable
directivity pattern, two slightly angled ears, or two independently configurable
PA microphones. The yaw/pitch orientation, positions, and directivity patterns
feed into the raytracing calculation. For stereo, a complete 2×2 IR
matrix is calculated between the two sources and receivers; a single receiver is routed to
both outputs. The default orientation aligns the source and listener with each other.
Directivity menus only appear for the respective matching microphone input.

The **Input**, **Dry/Wet**, and **Output** controls adjust the input level before the
room calculation, the ratio of dry to processed signal, and the final
output level. Input and Output each have a freely editable dB field; internally,
gain values are limited to ±120 dB. There is no limiter, so the output can clip.

## Rooms and Materials

The three built-in rooms are **Treated Studio**, **Concert Hall**, and **Glass Atrium**.
Their surfaces use six frequency-dependent acoustic material profiles:

- **Polished Stone** and **Glass** reflect almost all frequencies.
- **Rough Brick** scatters sound strongly and absorbs only a little.
- **Wood/Parquet** absorbs somewhat more, especially in the bass.
- **Heavy Velour Curtain** absorbs mainly mids and highs.
- **Mineral Wool (100 mm)** absorbs mids and highs very strongly.

The materials have procedural surface textures and separately influence the
lows, mids, and highs of the calculated impulse response.

Custom rooms can be loaded as `.obj` or `.fbx`. The associated input and
output settings (device type, directivity, level, position, orientation, and EQ)
are stored per room in the plugin state.

## 3D Navigation

While holding down the left mouse button, **W/A/S/D** moves the camera horizontally; the **Spacebar**
moves it up and **Ctrl** moves it down.

## Starting the Standalone

`test.bat` starts the existing standalone version. If the EXE is missing, the script
configures and builds it automatically. With `test.bat --build`, it always rebuilds before starting.

## Creating a GitHub Release

Under **Actions → Build and publish release → Run workflow**, select the release tag (for example
`v0.2.0`), the release notes as Markdown, and optionally **Publish this release as a
pre-release**. The workflow builds VST3 and Standalone for Windows and Linux and
additionally publishes a Windows installer (`.exe`) and a Linux Debian package
(`.deb`) as release artifacts. The ZIP and TAR.GZ packages remain available as well.
