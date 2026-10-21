# RCA T1 Transformer (VST3)

Saturador minimalista: só o transformador de entrada step-up estilo **RCA 901046-501 / T-1**.
Um parâmetro (**Saturation**, 0–10). Ganho de pequeno sinal = 0 dB em qualquer posição do knob.

## Compilar (Windows, via GitHub)
1. Suba esta pasta para um repositório no GitHub.
2. Aba **Actions** → *Build VST3 (Windows)* (roda sozinho a cada push, ou "Run workflow").
3. Baixe o artefato `RCA-T1-Transformer-VST3-Windows` e copie a pasta `.vst3` para
   `C:\Program Files\Common Files\VST3\`.

## Como as specs entram no modelo (Source/TransformerCore.h, struct `Spec`)
| Spec | Uso no modelo |
|---|---|
| Fonte 250 Ω (tap padrão) / 30 Ω | `sourceOhms` – impedância da fonte em série com o primário |
| Relação 1:17 … 1:43 | `turnsRatio` – reflete carga e DCR do secundário ao primário (÷n²) |
| DCR 53 Ω / 4,8 kΩ | `dcrPrimary` / `dcrSecondary` |
| Carga ~50 kΩ (grade) | `loadOhms` |
| L primário 5 H @100 Hz | `lPrimary` – indutância magnetizante de pequeno sinal |
| L secundário 1300 H | consistente com 1:16,1 (√(1300/5)); não entra separado |
| Caneca de Mu-Metal | sem equivalente em DSP (sem hum/EMI por construção) |

- `t1::tap250()` = 250 Ω ↔ 1:17 (padrão, usado em `prepareToPlay`)
- `t1::tap30()`  = 30 Ω ↔ 1:43 (troque em `PluginProcessor.cpp` se quiser)

## Suposições (NÃO estavam nas specs — ajuste à vontade)
- `fluxSat` (fluxo de saturação) e a curva B-H em 3 ramos (joelho gradual), `lAirRatio` = 100.
- Mapeamento do knob: 0 → 0 dB de drive, 10 → +46 dB (`knobToDrive`).
- Sem indutância de dispersão/capacitância de enrolamento: agudos planos.
