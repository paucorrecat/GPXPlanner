#pragma once
#include <QString>
#include <QVector>
#include <QDateTime>
#include <cmath>

/**
 * TrackSegment.h
 * Defineix TrackPoint (un punt individual del GPX) i
 * TrackSegment (un tram entre dos punts amb les seves característiques).
 */

// ── TrackPoint ────────────────────────────────────────────────────────────────

struct TrackPoint {
    double lat       = 0.0;
    double lon       = 0.0;
    double elevM     = 0.0;   // elevació en metres
    bool   hasEle    = false;  // true si el trkpt tenia element <ele> al GPX original
    QDateTime time;           // s'omplirà en l'exportació

    // Distància haversine fins a un altre punt (metres)
    double distanceTo(const TrackPoint& other) const {
        const double R = 6371000.0; // radi Terra en metres
        double phi1 = qDegreesToRadians(lat);
        double phi2 = qDegreesToRadians(other.lat);
        double dphi = qDegreesToRadians(other.lat - lat);
        double dlam = qDegreesToRadians(other.lon - lon);
        double a = std::sin(dphi/2)*std::sin(dphi/2) +
                   std::cos(phi1)*std::cos(phi2)*
                   std::sin(dlam/2)*std::sin(dlam/2);
        return R * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0-a));
    }
};

// ── TrackSegment ──────────────────────────────────────────────────────────────

struct TrackSegment {
    int    startIdx      = 0;
    int    endIdx        = 0;
    double distanceM     = 0.0;   // distància horitzontal (m)
    double elevGainM     = 0.0;   // desnivell positiu (m)
    double elevLossM     = 0.0;   // desnivell negatiu (m)
    double avgGradePct   = 0.0;   // pendent mitjana (%)
    double avgElevM      = 0.0;   // elevació mitjana (m) per densitat aire

    // Paràmetres d'esforç (l'usuari els especifica per tram)
    double targetPowerW  = 150.0; // Potència objectiu (W)
    double windSpeedMs   = 0.0;   // Vent (m/s), + = vent de cara
    double terrainFactor = 1.0;   // Multiplicador de velocitat [0.05, 2.0]; 1.0 = asfaltat
    QString label;                 // Nom del tram (p.ex. "Pujada Collserola")

    // Resultat del càlcul
    double estimatedSpeedMs  = 0.0;  // velocitat estimada (m/s)
    double estimatedTimeSec  = 0.0;  // temps estimat (s)
};
