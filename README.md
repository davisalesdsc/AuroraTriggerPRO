## Novidades da v0.3
- **Botão direito no pad:** menu com `Load Sample / TCI...`, `Load from Browser`, `Audition` e **`Clear Slot (Init)`**, que esvazia o slot e volta todos os controles dele ao valor inicial. Também dá para arrastar arquivos do Browser, do Finder ou do Explorer direto para um pad.
- **Arquivos `.tci`:** cada `.tci` vira um instrumento com **camadas de velocidade** (de leve a forte) e **round robin**, escolhidos a cada batida pela força do hit. Lê arquivos V1, Trigger 2 (V2) e do Trigger Instrument Editor. Limitação: nas ondas **estéreo** de `.tci` V2, os primeiros ~2 ms do ataque ainda não são decodificáveis e tocam em silêncio; o Browser avisa quando isso acontece. Decodificador portado do [tci2wav](https://github.com/AidanScroggins/tci2wav) (MIT, ver `THIRD_PARTY_NOTICES.md`).
- **Browser fixo:** escolha uma pasta e a árvore de arquivos fica na tela; subpastas abrem no lugar, sem voltar. A pasta é lembrada. Duplo clique carrega no slot selecionado.
- **View Curves (como no Trigger 2):** por slot, envelope `A`/`S`/`R`, `DYN` (quanto a força do hit muda o volume), `VEL` (curva de velocidade, 50 = linear) e `RANGE` mínimo/máximo, com gráficos. `CURVE LINK` liga cada controle em todos os slots.
- **SAVE PRESET / LOAD PRESET:** salva tudo (knobs, curvas, mixer e qual arquivo está em cada slot) num arquivo `.atpreset` em `Documentos/Aurora Trigger/Presets`. O `LOAD PRESET` lista os presets salvos; também dá para abrir um `.atpreset` com duplo clique no Browser. O preset guarda o caminho dos samples, então funciona no mesmo computador (ou onde os samples estiverem no mesmo lugar).
- **Sessão lembra os sons:** o projeto da DAW guarda qual arquivo está em cada slot e recarrega ao abrir.

## Novidades da v0.2
- **Layout Metálico Modular:** Menu esquerdo dedicado (`Browser`, `Triggering`, `Settings`, `View Mixer`, `View Curves`, `LINK TUNE`, `FLIP ALL PHASES`, `SAVE PRESET` e bloco `MIDI CAPTURE` com `DRAG ON TRACK`).
- **Kit Multi-Mic `Chilli Snare` Integrado:** Já inicia com 3 canais sintetizados/configurados (`ChiliSnare Z1 mic`, `ChiliSnare SSDR mic` e `ChiliSnare NRG mic`) com níveis individuais e pads de disparo na base.
- **Medidores LED Segmentados (`0` a `-48 dB`):** Leitura em tempo real nos estágios de `INPUT` e `OUTPUT`.
- **Link Tune:** Sincroniza a afinação (`Tune`) entre todos os canais multi-mic simultaneamente.
- **Drag-and-Drop Nativo (`DRAG ON TRACK`):** Exporta e arrasta o `.MID` capturado direto para a trilha da DAW.

