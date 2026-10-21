#pragma once
#include <algorithm>
#include <cmath>

// ============================================================================
//  Modelo físico de transformador de entrada step-up com núcleo saturável
//  Baseado nas especificações do RCA 901046-501 / T-1.
//
//  Circuito (tudo referido ao primário):
//
//    Vs ──[Rs]──[Rp]──●──────────●──[Rsec/n²]──[Rload/n²]──┐
//                     │          │                          │
//                   (Lm(i))   V_A (tensão no núcleo)       Vout'
//                     │                                     │
//                    GND                                   GND
//
//  O núcleo é um indutor magnetizante não-linear com curva B-H em 3 ramos
//  (domínios magnéticos saturando em fluxos diferentes => joelho gradual):
//      λ(i) = Lair·i + Σ_j λ_j · u_j/√(1+u_j²),   u_j = s_j·i / λ_j
//  onde λ = fluxo concatenado (V·s). Como dλ/dt = V_A, a saturação depende
//  de volt-segundos: graves saturam primeiro (comportamento real de trafo).
//
//  A equação implícita (integração trapezoidal) é resolvida a cada amostra
//  com Newton protegido por bracket (sempre converge).
// ============================================================================

namespace t1
{
    // ------------------------------------------------------------------
    //  Especificações (as que você passou)
    // ------------------------------------------------------------------
    struct Spec
    {
        double sourceOhms = 250.0;    // tap do primário: 250 Ω (padrão) ou 30 Ω
        double turnsRatio = 17.0;     // 1:17 (tap 250 Ω)  ... 1:43 (tap 30 Ω)
        double dcrPrimary = 53.0;     // Ω
        double dcrSecondary = 4800.0; // Ω
        double lPrimary = 5.0;        // H, indutância de pequeno sinal (100 Hz)
        double loadOhms = 50000.0;    // Ω, grade de válvula (~50k)

        // ----- NÃO estão nas specs (suposições, ajustáveis) -----
        double lAirRatio = 100.0;     // L0 / Lair: queda de indutância ao saturar
        double fluxSat = 3.0e-4;      // λs em V·s (no nível normalizado do plugin)
        // Peso (fração da permeabilidade inicial) e fluxo relativo de cada ramo
        double w[3] = { 0.50, 0.30, 0.20 };
        double m[3] = { 1.0,  4.0,  16.0 };
    };

    // Presets de tap prontos: 250 Ω ↔ 1:17  |  30 Ω ↔ 1:43
    inline Spec tap250() { Spec s; s.sourceOhms = 250.0; s.turnsRatio = 17.0; return s; }
    inline Spec tap30()  { Spec s; s.sourceOhms = 30.0;  s.turnsRatio = 43.0; return s; }

    // ------------------------------------------------------------------
    //  Núcleo (um canal)
    // ------------------------------------------------------------------
    class TransformerCore
    {
    public:
        void prepare (double sampleRate, const Spec& s)
        {
            spec = s;
            h = 0.5 / sampleRate; // T/2 (trapezoidal)

            const double n2 = spec.turnsRatio * spec.turnsRatio;
            rSecRef  = spec.dcrSecondary / n2;
            rLoadRef = spec.loadOhms / n2;

            const double rTh = spec.sourceOhms + spec.dcrPrimary; // série fonte + DCR primário
            const double rL  = rSecRef + rLoadRef;                // caminho de carga

            rPar    = (rTh * rL) / (rTh + rL);
            kVeq    = rL / (rTh + rL);      // Vs -> Veq (Thévenin visto pelo núcleo)
            kOut    = rLoadRef / rL;        // V_A -> tensão na carga
            g0      = rLoadRef / (rTh + rL);// ganho de banda passante (Vs -> Vout')

            l0   = spec.lPrimary;
            lAir = spec.lPrimary / spec.lAirRatio;
            lamTot = 0.0;
            for (int j = 0; j < 3; ++j)
            {
                lamJ[j] = spec.fluxSat * spec.m[j];
                sJ[j]   = spec.w[j] * (l0 - lAir);
                lamTot += lamJ[j];
            }
            kLin = lAir + h * rPar;         // inclinação mínima de G(i)

            reset();
        }

        void reset()
        {
            iPrev = 0.0;
            vPrev = 0.0;
            lamPrev = 0.0;
        }

        // x     : amostra de áudio (±1 = fundo de escala)
        // drive : tensão de pico de fundo de escala aplicada ao primário
        // retorna amostra com ganho de pequeno sinal = 1 (0 dB) na banda média
        double process (double x, double drive)
        {
            const double veq = x * drive * kVeq;

            // G(i) = kLin·i + Σ λ_j·f(s_j·i/λ_j) − r = 0   (monótona crescente)
            const double r = lamPrev + h * (veq + vPrev);

            double lo = (r - lamTot) / kLin;     // |Σ λ_j f| <= λtot  => raiz dentro do bracket
            double hi = (r + lamTot) / kLin;
            double i  = std::clamp (iPrev, lo, hi);

            for (int it = 0; it < 24; ++it)
            {
                double G = kLin * i - r, dG = kLin;
                for (int j = 0; j < 3; ++j)
                {
                    const double u  = sJ[j] * i / lamJ[j];
                    const double q  = 1.0 / std::sqrt (1.0 + u * u);
                    G  += lamJ[j] * u * q;
                    dG += sJ[j] * q * q * q;
                }

                if (G > 0.0) hi = i; else lo = i;

                const double step = G / dG;
                if (std::abs (step) < 1.0e-13 + 1.0e-12 * std::abs (i))
                {
                    i = std::clamp (i - step, lo, hi);   // convergiu
                    break;
                }

                double next = i - step;
                if (next <= lo || next >= hi)            // Newton saiu do bracket: bisseção
                    next = 0.5 * (lo + hi);
                i = next;
            }

            double lam = lAir * i;
            for (int j = 0; j < 3; ++j)
            {
                const double u = sJ[j] * i / lamJ[j];
                lam += lamJ[j] * u / std::sqrt (1.0 + u * u);
            }
            const double vA  = veq - rPar * i;

            iPrev = i;
            vPrev = vA;
            lamPrev = lam;

            const double vOut = vA * kOut;                 // referido ao primário
            return vOut / (g0 * drive);                    // normaliza: ganho 0 dB
        }

        double passbandGain() const { return g0; }

    private:
        Spec spec;
        double h = 0, rSecRef = 0, rLoadRef = 0, rPar = 0, kVeq = 0, kOut = 0, g0 = 1;
        double l0 = 0, lAir = 0, kLin = 0, lamTot = 0;
        double lamJ[3] = {}, sJ[3] = {};
        double iPrev = 0, vPrev = 0, lamPrev = 0;
    };

    // ------------------------------------------------------------------
    //  Mapeamento do knob (0..10) para "drive"
    //  0  -> 0 dB  (quase linear; só graves muito altos colorem)
    //  10 -> +46 dB (saturação forte, mesmo em médios)
    // ------------------------------------------------------------------
    inline double knobToDrive (double knob)
    {
        return std::pow (10.0, (knob * 4.6) / 20.0);
    }
}
