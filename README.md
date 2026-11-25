# NoWave

C++20 VST3 & Standalone Audio Plugin built with JUCE and CMake.

## Voraussetzungen

- CMake >= 3.25
- Git
- Visual Studio (MSVC mit C++ Desktop-Entwicklung) auf Windows
- Vulkan SDK mit `glslc` auf Windows (`VK_SDK_PATH`); Linux-Paketabhängigkeiten siehe unten

## Build (Windows)

Alternativ einfach `build.bat` starten; das Skript konfiguriert das Release-Preset und
baut VST3 sowie Standalone.

```powershell
# Konfigurieren (lädt JUCE automatisch beim ersten Mal und kompiliert den Compute-Shader)
cmake --preset windows-release

# Bauen (VST3 + Standalone)
cmake --build --preset windows-release
```

Die fertigen Artefakte liegen danach unter:
- `build/release/NoWave_artefacts/Release/VST3/NoWave.vst3`
- `build/release/NoWave_artefacts/Release/Standalone/NoWave.exe`

## Build (Linux)

Der GitHub-Release-Workflow baut zusätzlich ein Linux-VST3 und eine Standalone-App.
Lokal kann mit `cmake --preset linux-release` konfiguriert und mit
`cmake --build --preset linux-release --parallel 2` gebaut werden. Benötigt werden
Vulkan SDK beziehungsweise `libvulkan-dev` und `glslc` sowie die JUCE-Entwicklungspakete
für ALSA, JACK, X11, FreeType, Fontconfig und OpenGL.

Die Vulkan-Raytracing-Engine berechnet die Raumimpulsantwort in einem Hintergrund-Worker.
Ist zur Laufzeit kein Vulkan-Compute-Gerät verfügbar, wird eine direkte Schallverbindung
als Fallback verwendet.

## Akustische Konfiguration

Im unteren Bedienfeld lassen sich Output und Input auswählen. **Vocal** verwendet eine
gerichtete Einzelquelle, **Speaker Box** zwei auf ±30° ausgerichtete Quellen und **PA**
zwei getrennte Stereo-Lautsprecher. Als Input stehen ein Mikrofon mit einstellbarer
Richtcharakteristik, zwei leicht angewinkelte Ohren oder zwei unabhängig konfigurierbare
PA-Mikrofone zur Verfügung. Die Yaw-/Pitch-Ausrichtung, Positionen und Richtcharakteristiken
gehen in die Raytracing-Berechnung ein. Für Stereo wird eine vollständige 2×2-IR-Matrix
zwischen den beiden Quellen und Empfängern berechnet; ein einzelner Empfänger wird auf
beide Ausgänge geroutet. Die Standardausrichtung richtet Quelle und Hörer zueinander aus.
Richtcharakteristik-Menüs erscheinen nur für den jeweils passenden Mikrofoneingang.

Die Regler **Input**, **Dry/Wet** und **Output** steuern den Eingangspegel vor der
Raumberechnung, das Verhältnis von trockenem zu bearbeitetem Signal und den finalen
Ausgangspegel. Input und Output haben jeweils ein frei eintippbares dB-Feld; intern werden
Gain-Werte auf ±120 dB begrenzt. Es gibt keinen Limiter, daher kann der Ausgang übersteuern.

## Räume und Materialien

Die drei integrierten Räume sind **Treated Studio**, **Concert Hall** und **Glass Atrium**.
Ihre Oberflächen verwenden sechs frequenzabhängige akustische Materialprofile:

- **Polished Stone** und **Glass** reflektieren fast alle Frequenzen.
- **Rough Brick** streut Schall stark und absorbiert nur wenig.
- **Wood/Parquet** absorbiert etwas mehr, besonders im Bass.
- **Heavy Velour Curtain** absorbiert vor allem Mitten und Höhen.
- **Mineral Wool (100 mm)** absorbiert Mitten und Höhen sehr stark.

Die Materialien besitzen prozedurale Oberflächentexturen und beeinflussen getrennt die
Tiefen, Mitten und Höhen der berechneten Impulsantwort.

Eigene Räume können als `.obj` oder `.fbx` geladen werden. Die zugehörigen Input- und
Output-Einstellungen (Gerätetyp, Richtcharakteristik, Pegel, Position, Ausrichtung und EQ)
werden pro Raum im Plugin-Zustand gespeichert.

## 3D-Navigation

Bei gedrückter linker Maustaste bewegt **W/A/S/D** die Kamera horizontal; **Leertaste**
bewegt sie nach oben und **Strg** nach unten.

## Standalone starten

`test.bat` startet die vorhandene Standalone-Version. Fehlt die EXE, konfiguriert und
baut das Skript sie automatisch. Mit `test.bat --build` wird vor dem Start immer neu gebaut.

## GitHub Release erstellen

Unter **Actions → Build and publish release → Run workflow** den Release-Tag (zum Beispiel
`v0.2.0`) und die Release Notes als Markdown eingeben. Der Workflow baut VST3 und Standalone
für Windows und Linux und veröffentlicht beide Plattformpakete als GitHub Release.

