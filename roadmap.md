# NoWave – Roadmap & Bauplan

> **Lebende Datei.** Diese Datei dient als Bauplan des Projekts: Sie zeigt, wo was liegt, wie
> die Teile zusammenhängen und was als Nächstes ansteht. Vor jeder größeren Änderung zuerst
> hier nachlesen, nach jeder abgeschlossenen Aufgabe Status und Bauplan aktualisieren.
>
> Stand: 2026-11-25 · Projektstatus: **Proof of Concept**

---

## 1. Pflege-Regeln (für Mensch und Agent)

1. **Vor der Arbeit:** Abschnitte 2–4 lesen, um Zuständigkeiten und Abhängigkeiten zu kennen.
2. **Während der Arbeit:** Neue Dateien, Klassen oder Parameter sofort in Abschnitt 3 eintragen.
3. **Nach der Arbeit:** Erledigte Punkte in Abschnitt 6 abhaken (`- [x]`), neue offene Punkte
   ergänzen, Abschnitt 5 (bekannte Auffälligkeiten) bereinigen.
4. Die Datei beschreibt den **Ist-Zustand**. Wunschdenken gehört in Abschnitt 6, nicht in Abschnitt 2–4.
5. Kurz halten. Details gehören in den Code bzw. in Commit-Messages.

---

## 2. Projektüberblick

NoWave ist ein **C++20-Audio-Plugin** (VST3 und Standalone) auf Basis von **JUCE 8.0.6**.
Es berechnet eine Raumimpulsantwort (RIR) aus Raumgeometrie, Oberflächenmaterialien,
Schallquelle und Empfänger und faltet das Eingangssignal damit.

Kernidee: **GPU-Raytracing (Vulkan Compute)** erzeugt die IR, **Partitioned Convolution**
wendet sie in Echtzeit an. Ohne Vulkan-Gerät greift ein **Direktschall-Fallback**.

Das Projekt ist ein Proof of Concept. Es gibt **keine automatisierten Tests** und
**keine CI-Build-Prüfung** außerhalb des Release-Workflows.

### 2.1 Datenfluss (Laufzeit)

```text
                 ┌────────────── Message-Thread (UI) ──────────────┐
  OBJ/FBX/Preset │  StartScreen → Editor → Viewport3D / BottomBar  │
  ─────────────► │          │ setRoomMesh()        │ APVTS-Attachments
                 └──────────┼──────────────────────┼───────────────┘
                            ▼                      ▼
                 NoWaveAudioProcessor ── ParameterPointers (atomic<float>*)
                            │                      │
          getRoomMesh()     │                      │ liest Parameter
                            ▼                      ▼
                 RaytracingWorker-Thread (≈25 Hz Debounce)
                   ├─ Parameteränderung? → VulkanRaytracer::trace()
                   │       (BVH-Upload, Compute-Shader raytracer.comp)
                   │   oder synthesizeDirectPath() als Fallback
                   ├─ BVH/Taps → IRBuffer (3 Bänder × 2×2-Matrix)
                   └─ IRUpdateQueue (Triple-Buffer, lock-free)
                            │
                 Audio-Thread │ processBlock()
                            ▼
   Input-Gain → Source-EQ → CrossfadeConvolver (IR) → Listener-EQ
              → Dry/Wet → Output-Gain
```

---

## 3. Bauplan – wo liegt was

### 3.1 Verzeichnisübersicht

| Pfad | Verantwortung |
|---|---|
| [CMakeLists.txt](CMakeLists.txt) | Build-Definition: JUCE/tinyobjloader/glm/ufbx per FetchContent, Shader-Kompilierung |
| [CMakePresets.json](CMakePresets.json) | Presets `windows-debug`, `windows-release`, `linux-release` |
| [cmake/spv_to_header.cmake](cmake/spv_to_header.cmake) | Wandelt `raytracer.spv` in C-Header (`raytracer_spv.h`) um |
| [build.bat](build.bat) / [test.bat](test.bat) | Windows-Skripte: komplett bauen bzw. Standalone starten (`--build` erzwingt Neubau) |
| [packaging/](packaging/) | Inno-Setup-Installer (Windows) und Debian-Control/Desktop-Datei (Linux) |
| [.github/workflows/release.yml](.github/workflows/release.yml) | Manueller Release-Workflow (Windows-ZIP/Installer, Linux-Build/.deb) |
| [src/PluginProcessor.*](src/PluginProcessor.h) | Plugin-Einstieg (`createPluginFilter`), APVTS, State-Speicherung, Raumzustand pro Raum |
| [src/PluginEditor.*](src/PluginEditor.h) | Wurzel-UI; schaltet zwischen StartScreen und 3D-Ansicht um |
| [src/parameters/](src/parameters/) | Parameter-IDs, Ranges, Defaults (`ParameterLayout`) und `ParameterPointers` |
| [src/audio/](src/audio/) | Echtzeit-DSP: Engine, Faltung, EQ, IR-Puffer und Triple-Buffer-Queue |
| [src/raytracing/](src/raytracing/) | Worker-Thread, Vulkan-Wrapper, BVH-Bau, GPU-Strukturen, Shader |
| [src/scene/](src/scene/) | Raumdatenmodell, Material, Datei-Loader (OBJ/FBX), Presets |
| [src/ui/](src/ui/) | Start-Screen, 3D-Viewport (OpenGL), untere Bedienleiste |

### 3.2 Dateien nach Zuständigkeit

**Plugin-Kern**
- [PluginProcessor.cpp](src/PluginProcessor.cpp): `processBlock` ruft nur `ReverbEngine::processBlock`.
  `setRoomMesh` speichert Raum-Audio-Einstellungen pro Raum (Schlüssel `sourceType|identifier`)
  im APVTS-State (Kind `Room` unter `RoomAudioSettings`). `restoreRoomFromState` lädt den Raum beim Laden des States neu.
- [PluginEditor.cpp](src/PluginEditor.cpp): Lädt Dateien über `SceneFileLoader`, Presets über `RoomPresets`.
  Setzt `isRoomLoaded` (wird derzeit nur gesetzt, aber nirgends gelesen).

**Parameter** ([ParameterLayout.cpp](src/parameters/ParameterLayout.cpp))
- Positionen ±25 m, Winkel Yaw ±180° / Pitch ±89°, EQ ±18 dB, Mitten-Frequenz 200–5000 Hz.
- Input/Output ±120 dB, Dry/Wet 0–100 %, Ray Density 128–8192, Max Bounces 0–16.
- Auswahl-Parameter: `output_type` (Vocal/SpeakerBox/PA), `input_type` (Microphone/Ears/PA),
  Mikrofon-Richtcharakteristik (4 Muster), `show_ray_paths` (Strahlen-Debugansicht) sowie `sound_view` (Wolke aus/an), `sound_view_band` und `sound_view_arrival` (Filter).
- **Neuer Parameter?** Muss an drei Stellen gepflegt werden: `ParamIDs` in `ParameterLayout.h`,
  `ParameterPointers` (+ `bind`), und bei Raum-spezifischen Parametern die Liste
  `kRoomAudioParameterIds` in `PluginProcessor.cpp`.

**Audio** ([src/audio/](src/audio/))
- [ReverbEngine.cpp](src/audio/ReverbEngine.cpp): Orchestriert die Kette. EQ-Koeffizienten werden nur bei Änderung neu berechnet.
- [CrossfadeConvolver.h](src/audio/CrossfadeConvolver.h): Zwei `juce::dsp::Convolution`-Engines (A/B) mit 25 ms Crossfade.
  Je Band und Route eine Faltung (3 Bänder × 4 Routen).
- [IRUpdateQueue.h](src/audio/IRUpdateQueue.h): Lock-free Triple-Buffer (Slots 0=read, 1=middle, 2=write, Dirty-Bit 0x4).
- [IRBuffer.h](src/audio/IRBuffer.h): 12 Kanäle (`channelIndex(band, receiver, source)`), max. 192 000 Samples (4 s @ 48 kHz).
- [ChannelEQ.h](src/audio/ChannelEQ.h): Low-Shelf 200 Hz, Mid-Peak (frei wählbar), High-Shelf 8 kHz.

**Raytracing** ([src/raytracing/](src/raytracing/))
- [RaytracingWorker.cpp](src/raytracing/RaytracingWorker.cpp): Herzstück der Simulation.
  - `simulationParametersChanged()` vergleicht Parameter mit Schwellen (1 cm, 1°).
  - `makeRaytraceParameters()` baut Quellen/Empfänger-Positionen aus Yaw/Pitch und Offsets
    (SpeakerBox ±6 cm/±30°, PA ±15 cm, Ears ±9 cm/±10°).
  - `synthesizeDirectPath()`: Fallback mit 1/d-Dämpfung und Richtcharakteristik.
- [AcousticSimulation.h](src/raytracing/AcousticSimulation.h): Konstanten (Ray-Limits, Bounces), `RaytraceParameters` (192 Byte, `static_assert`), Richtcharakteristik-Funktionen.
- [VulkanContext.cpp](src/raytracing/VulkanContext.cpp): Headless-Instanz, Compute-Queue, Command-Pool.
- [VulkanRaytracer.cpp](src/raytracing/VulkanRaytracer.cpp): Pipeline, Buffer (Nodes, Tris, Materials, Taps, Counter, RayPaths, Parameter), `setScene`, `trace`.
- [shaders/raytracer.comp](src/raytracing/shaders/raytracer.comp): GLSL 450, Local Size 64. Eine Invocation je Ray und Quelle;
  Bounce-Schleife bis `limits.w`; Materialabsorption je Band, Streuung über `absorption.w`.
- [BVHBuilder.cpp](src/raytracing/BVHBuilder.cpp): Median-Split, ein Dreieck pro Blatt.
- [BVHNode.h](src/raytracing/BVHNode.h): **GPU-Layouts** (`GPUBVHNode` 48 B, `GPUTriangle` 80 B, `GPUMaterial` 16 B, `RayTap` 32 B, `RaySegment` 48 B).

> ⚠️ **Kopplung:** Die C++-Structs in `BVHNode.h` und `AcousticSimulation.h` müssen exakt zum
> `std430`-Layout der Shader-Blöcke in `raytracer.comp` passen. Änderungen immer in beiden Dateien,
> plus `static_assert`-Größen prüfen.

**Szene** ([src/scene/](src/scene/))
- [RoomMesh.h](src/scene/RoomMesh.h): Vertices, Triangles (mit `materialId`), Materialliste, Bounding Box.
- [AcousticMaterial.h](src/scene/AcousticMaterial.h): Absorption (Low/Mid/High), Streuung, Oberflächen-Textur-Enum (7 Werte).
- [SceneFileLoader.cpp](src/scene/SceneFileLoader.cpp): Dispatch nach Dateiendung (`.obj`, `.fbx`).
- [ObjSceneLoader.cpp](src/scene/ObjSceneLoader.cpp) (tinyobjloader) und [FbxSceneLoader.cpp](src/scene/FbxSceneLoader.cpp) (ufbx).
- [RoomPresets.cpp](src/scene/RoomPresets.cpp): Sechs prozedurale Räume (Studio, Konzertsaal, Glasatrium, Wohnzimmer, Tiefgarage, Klassenzimmer); Materialien aus `MaterialLibrary`
  (`Studio`, `ConcertHall`, `GlassAtrium`).

**UI** ([src/ui/](src/ui/))
- [StartScreenComponent.cpp](src/ui/StartScreenComponent.cpp): Datei-Drop, Dateidialog, Preset-Buttons, Statusmeldung.
- [Viewport3DComponent.cpp](src/ui/Viewport3DComponent.cpp): OpenGL-Renderer (Raum-, Marker-, Richtungs-, Strahlen- und Schallwolken-Geometrie; Wolke = Energiedichte-Voxelfeld mit Flugrichtungs-Pfeilen), Kamera mit WASD/Leertaste/Strg.
  Lädt Strahlen über `rayPathProvider` (Snapshot-Polling per Timer).
- [LevelLegendComponent.cpp](src/ui/LevelLegendComponent.cpp): dB-Farbskala der Schallwolke (Farbrampe in `Palette.h`).
- [BottomControlBar.cpp](src/ui/BottomControlBar.cpp): Regler für Quelle und Empfänger, Input/Output-Typen, Raytracing-Optionen.
  Bindet per `SliderAttachment` an das APVTS.

---

## 4. Build, Tooling und Umgebung

| Thema | Stand |
|---|---|
| Sprache / Standard | C++20, `CMAKE_CXX_EXTENSIONS OFF` |
| Mindest-CMake | 3.25 |
| Abhängigkeiten | JUCE 8.0.6, tinyobjloader v2.0.0rc13, glm 1.0.1, ufbx v0.23.0 (alle per FetchContent) |
| Shader | `glslc --target-env=vulkan1.1 -O` (benötigt Vulkan SDK, `VK_SDK_PATH`) |
| Windows-Build | `build.bat` bzw. `cmake --preset windows-release` + `cmake --build --preset windows-release` |
| Linux-Build | Konfiguriert, aber **nicht praktisch verifiziert** (siehe README) |
| Tests | **Keine** vorhanden |
| Lint/Format | `.clang-format` vorhanden, keine Pipeline dafür |
| Ausgabe | `build/release/NoWave_artefacts/Release/{VST3,Standalone}/` |
| Release | Manueller Workflow mit Tag, Release Notes und Pre-Release-Flag |
| Lizenz | AGPL-3.0 (Projekt); ufbx-Lizenz wird ins Release-ZIP übernommen |

Hinweis: Im Repo liegt ein `build/`-Verzeichnis mit einem veralteten Target `juce_vst3_helper`,
das in der aktuellen `CMakeLists.txt` nicht mehr existiert. Es ist in `.gitignore`, also unkritisch.

---

## 5. Bekannte Auffälligkeiten & technische Schulden

Gefunden beim Durchlesen (Stand: Lesen der Kernmodule; Dateien unter `ui/*.cpp`, `CrossfadeConvolver.cpp`,
`VulkanRaytracer.cpp` und `FbxSceneLoader.cpp` wurden nur oberflächlich geprüft).

- [ ] **Dokumentation vs. Code:** [ChannelEQ.h](src/audio/ChannelEQ.h) nennt `SmoothedValue` als Glättung der Koeffizienten. Das wird nicht verwendet.
- [ ] **Hardcodierte Samplerate:** [PluginProcessor.cpp](src/PluginProcessor.cpp) (`getTailLengthSeconds`) rechnet mit 48 kHz statt mit der tatsächlichen Samplerate.
- [ ] **Audio-Thread-Allokation möglich:** [ReverbEngine.cpp](src/audio/ReverbEngine.cpp) ruft `wetBuffer.setSize(...)` im `processBlock`. Bei Blöcken größer als in `prepare` angegeben wird neu alloziert.
- [ ] **Mono-Layout ungeklärt:** `isBusesLayoutSupported` erlaubt Mono, die Faltung ist aber für Stereo ausgelegt (`CrossfadeConvolver.h`). Verhalten prüfen.
- [ ] **Ungenutzter Zustand:** `isRoomLoaded` in [PluginEditor.h](src/PluginEditor.h) wird geschrieben, aber nie gelesen.
- [ ] **Direktschall-Fallback:** `RaytracingWorker` wählt bei Fehlern still den Direktschall. Die UI zeigt das nicht an.
- [ ] **Raum-Einstellungen im State:** Pro Raum werden alle Audio-Parameter gespeichert. Der Schlüssel `sourceType|identifier` nutzt bei Dateien den **absoluten Pfad**. Verschobene Dateien verlieren ihre Einstellungen.
- [ ] **Keine Tests:** Besonders kritisch für BVH-Bau, Triple-Buffer-Logik und Layout-Konsistenz (C++ ↔ GLSL).
- [ ] **Linux nicht verifiziert** (laut README).

---

## 6. Roadmap

Legende: `[ ]` offen · `[~]` in Arbeit · `[x]` erledigt · `[!]` blockiert

### Phase 0 – Absicherung des PoC (Vorbedingung für alles Weitere)
- [ ] Automatisierte Tests einrichten (CTest / Catch2 o. ä.) für: BVH-Bau, `IRUpdateQueue`, `RoomPresets`, `SceneFileLoader`
- [ ] Struct-Layout-Tests: C++ `static_assert`s um Offset-Checks gegen GLSL ergänzen
- [ ] Linux-Build praktisch verifizieren (Build, Start, Vulkan-Fallback)
- [ ] Punkte aus Abschnitt 5 abarbeiten oder bewusst verwerfen
- [ ] Fehlermeldungen an die UI weitergeben (Vulkan nicht verfügbar / Fallback aktiv)

### Phase 1 – Akustische Qualität
- [ ] Validierung der Simulation gegen Referenzwerte (z. B. Nachhallzeit nach Sabine/Eyring für einen Testraum)
- [ ] Energie-Normierung der IR prüfen (1/d, Richtcharakteristik, Absorptionsverluste)
- [ ] Frequenzauflösung: Drei Bänder ausbauen oder Nutzen gegenüber dem Aufwand bewerten
- [ ] Streuung (`scattering`) im Shader auf Plausibilität prüfen
- [ ] Ray-Count vs. Qualität vs. Rechenzeit messen und Defaults begründen

### Phase 2 – Audio-Engine
- [ ] Audio-Thread vollständig allokationsfrei machen (Abschnitt 5)
- [ ] Glättung der Parameteränderungen (`SmoothedValue`) einführen oder Doku anpassen
- [ ] Mono- und Multichannel-Layouts sauber unterstützen oder explizit ausschließen
- [ ] Latenz und Tail-Länge korrekt an Samplerate koppeln
- [ ] Crossfade-Verhalten bei schnellen IR-Wechseln hörbar prüfen

### Phase 3 – UI / UX
- [ ] Anzeige des aktiven Berechnungsmodus (GPU oder Direktschall) und des Berechnungsstatus
- [ ] Bedienbarkeit der Raumliste prüfen (Umschalten zwischen Presets und eigenen Räumen)
- [ ] Visualisierung: Strahlstatistik / Energie-Histogramm (optional)

### Phase 4 – Szenenimport
- [ ] Robustheit der OBJ/FBX-Loader bei fehlerhaften Dateien (Fehlermeldungen, Grenzwerte)
- [x] Materialzuordnung aus MTL/FBX-Materialien per Namens-Schlüsselwort (`MaterialLibrary`) statt Standardwerten
- [ ] Einheitenkonvertierung (cm/m) und Skalierung beim Import
- [ ] Gespeicherte Raum-Einstellungen gegen verschobene Dateien robust machen (z. B. Dateiname + Hash)

### Phase 5 – Release & Packaging
- [ ] CI-Build für Windows und Linux auf jedem Push (nicht nur manuell im Release-Workflow)
- [ ] Linux-Paket (.deb) und Installer praktisch testen
- [ ] Versionierung: `CMakeLists.txt` (0.1.0) und Release-Tag synchron halten
- [ ] Code-Signierung für Windows-Binaries prüfen

---

## 7. Offene Fragen

- Soll NoWave langfristig ein Produkt sein (dann Phase 0 + 5 priorisieren) oder bleibt es ein Experiment?
- Ziel-Präzision der Akustik: Musikalisches Ergebnis oder physikalisch überprüfbare Werte?
- Ist macOS (AU/VST3) ein Ziel? Aktuell nicht vorgesehen.
- Soll die Vulkan-Abhängigkeit für Endnutzer bleiben oder ist eine CPU-Variante geplant?

---

## 8. Änderungsprotokoll dieser Datei

| Datum | Änderung |
|---|---|
| 2026-11-25 | Erstfassung nach Durchlesen des Projekts |
