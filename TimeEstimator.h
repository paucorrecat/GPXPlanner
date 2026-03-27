#pragma once
#include "RiderProfile.h"
#include "TrackSegment.h"
#include <cmath>
#include <stdexcept>

/**
 * TimeEstimator.h
 * Motor físic principal.
 *
 * MODEL DE POTÈNCIA CICLISTA:
 *
 *   P_total = (F_gravitat + F_rodolament + F_aerodinàmica) × v / η_transmissió
 *
 *   F_gravitat    = m × g × sin(θ)          [θ = angle del pendent]
 *   F_rodolament  = m × g × Crr × cos(θ)
 *   F_aerodinàmica= 0.5 × ρ × CdA × (v + v_vent)²
 *
 * Donat P objectiu, resolem per v numèricament (bisecció).
 */
class TimeEstimator {
public:
    explicit TimeEstimator(const RiderProfile& profile)
        : m_profile(profile) {}

    /**
     * Calcula la velocitat sostenible (m/s) donada una potència (W) i un pendent (%).
     * windSpeedMs > 0 = vent de cara, < 0 = vent de cua.
     */
    double estimateSpeed(double powerW, double gradePct,
                         double windSpeedMs = 0.0,
                         double altitudeM   = 0.0) const
    {
        if (powerW <= 0.0) return 0.0;

        const double gradeRad = std::atan(gradePct / 100.0);
        const double rho      = m_profile.airDensityAtAltitude(altitudeM);
        const double m        = m_profile.totalMassKg;
        const double g        = m_profile.gravityMs2;
        const double Crr      = m_profile.crr;
        const double CdA      = m_profile.cda;
        const double eta      = m_profile.drivetrainEff;

        // Forces independents de la velocitat
        const double Fg = m * g * std::sin(gradeRad);
        const double Fr = m * g * Crr * std::cos(gradeRad);

        // Funció: P_necessària(v) - P_disponible = 0
        // Resolem per bisecció en [0, 30] m/s (~108 km/h)
        auto powerNeeded = [&](double v) -> double {
            double vEfectiu = v + windSpeedMs;
            double Fa = 0.5 * rho * CdA * vEfectiu * std::abs(vEfectiu);
            return ((Fg + Fr + Fa) * v) / eta;
        };

        // Comprovem si és assolible (pendent molt pronunciada)
        double vMax = 30.0;
        if (powerNeeded(0.001) > powerW) {
            // No es pot avançar amb aquesta potència — retornem velocitat mínima
            return 0.3; // ~1 km/h (l'usuari empeny la bici)
        }

        // Bisecció
        double lo = 0.0, hi = vMax;
        for (int i = 0; i < 64; ++i) {
            double mid = (lo + hi) / 2.0;
            if (powerNeeded(mid) < powerW) lo = mid;
            else                           hi = mid;
        }
        return (lo + hi) / 2.0;
    }

    /**
     * Calcula el temps estimat (s) per a un segment complet.
     * Actualitza segment.estimatedSpeedMs i segment.estimatedTimeSec.
     */
    void computeSegment(TrackSegment& segment) const {
        double speedMs = estimateSpeed(
            segment.targetPowerW,
            segment.avgGradePct,
            segment.windSpeedMs,
            segment.avgElevM
        );
        segment.estimatedSpeedMs = speedMs;
        if (speedMs > 0.0)
            segment.estimatedTimeSec = segment.distanceM / speedMs;
        else
            segment.estimatedTimeSec = 9999.0; // pràcticament aturat
    }

private:
    RiderProfile m_profile;
};
