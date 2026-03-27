#pragma once
#include <QString>

/**
 * StopPoint.h
 * Parada en un punt del track
 */
struct StopPoint {
    int     trackPointIdx = 0;      // índex del punt GPX on s'atura
    int     durationSec   = 0;      // temps de parada (segons)
    QString description;            // "Esmorzar", "Font", etc.
};
