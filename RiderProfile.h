#pragma once

/**
 * RiderProfile.h
 * Paràmetres físics del ciclista i la bicicleta
 */
struct RiderProfile {
    double totalMassKg     = 85.0;  // ciclista + bici + equipament (kg)
    double ftpWatts        = 200.0; // Functional Threshold Power (W)
    double cda             = 0.35;  // Coeficient aerodinàmic x àrea frontal (m²)  [MTB ~0.35-0.45]
    double crr             = 0.012; // Coeficient de rodolament  [MTB terra ~0.012-0.02]
    double drivetrainEff   = 0.97;  // Eficiència transmissió (97%)
    double airDensity      = 1.225; // Densitat aire (kg/m³) a nivell del mar
    double gravityMs2      = 9.81;  // Gravetat (m/s²)

    // Ajust densitat per altitud (aproximació simple)
    double airDensityAtAltitude(double altitudeM) const {
        return airDensity * std::exp(-altitudeM / 8500.0);
    }
};
