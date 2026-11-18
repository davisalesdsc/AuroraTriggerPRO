# Aurora Trigger v0.2 (JUCE C++ Source)

Projeto completo em **C++ / JUCE 8** do plugin **Aurora Trigger v0.2**, pronto para compilar em **VST3**, **AU (Audio Unit)** e **Standalone** no macOS ou Windows.

## Novidades da v0.2
- **Layout Metálico Modular:** Menu esquerdo dedicado (`Browser`, `Triggering`, `Settings`, `View Mixer`, `View Curves`, `LINK TUNE`, `FLIP ALL PHASES`, `SAVE PRESET` e bloco `MIDI CAPTURE` com `DRAG ON TRACK`).
- **Kit Multi-Mic `Chilli Snare` Integrado:** Já inicia com 3 canais sintetizados/configurados (`ChiliSnare Z1 mic`, `ChiliSnare SSDR mic` e `ChiliSnare NRG mic`) com níveis individuais e pads de disparo na base.
- **Medidores LED Segmentados (`0` a `-48 dB`):** Leitura em tempo real nos estágios de `INPUT` e `OUTPUT`.
- **Link Tune:** Sincroniza a afinação (`Tune`) entre todos os canais multi-mic simultaneamente.
- **Drag-and-Drop Nativo (`DRAG ON TRACK`):** Exporta e arrasta o `.MID` capturado direto para a trilha da DAW.

## Como Compilar

O CI do GitHub (`.github/workflows/build.yml`) compila a cada push e publica o release a partir do `main`:

| Plataforma | Saída |
|---|---|
| macOS **Universal** (Intel + Apple Silicon, macOS 10.13+) | `.pkg` instalador (AU + VST3) e `.zip` (AU, VST3, Standalone) |
| Windows x64 | `.zip` (VST3 + Standalone `.exe`, runtime estático) |

O CI também valida: binário universal (`lipo`), versão mínima do macOS, `auval` no AU, instalação do `.pkg`
e carga do plugin como uma DAW (incluindo num Mac Intel nativo).

### Local

```bash
# macOS (gera binário universal por padrão) ou Windows (Visual Studio 2022)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Saída em `build/AuroraTrigger_artefacts/Release/`. O JUCE 8.0.15 é baixado automaticamente
(ou use `-DAURORA_JUCE_DIR=/caminho/JUCE`).

### Release

Cada push no `main` cria/atualiza o release `v<VERSION>` (versão no `project(...)` do `CMakeLists.txt`).
Suba o `VERSION` para iniciar um novo release; tags `v*` também publicam.
