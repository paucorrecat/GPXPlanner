#pragma once
#include "RiderProfile.h"
#include "TrackSegment.h"
#include "StopPoint.h"
#include "TimeEstimator.h"
#include "GPXParser.h"
#include <QVector>
#include <QDateTime>
#include <QString>

/**
 * TrackPlanner.h
 * Orquestrador principal.
 * Connecta la càrrega del GPX, els segments, les parades i l'exportació.
 */
class TrackPlanner {
public:

    // ── Configuració ─────────────────────────────────────────────────────────

    void setRiderProfile(const RiderProfile& profile) {
        m_profile   = profile;
        m_estimator = TimeEstimator(profile);
    }

    void setStartTime(const QDateTime& dt) { m_startTime = dt; }

    // ── Càrrega del GPX ───────────────────────────────────────────────────────

    bool loadGPX(const QString& path) {
        QString err;
        m_points = GPXParser::loadGPX(path, err);
        m_lastError = err;
        return !m_points.isEmpty();
    }

    // ── Gestió de segments ────────────────────────────────────────────────────

    /**
     * Defineix els trams manualment especificant índexs de punts.
     * Exemple: { {0,50}, {50,120}, {120,200} }
     */
    void defineSegments(const QVector<QPair<int,int>>& ranges) {
        m_segments = GPXParser::buildSegments(m_points, ranges);
    }

    /** Accés per modificar potència/vent/etiqueta de cada segment */
    QVector<TrackSegment>& segments() { return m_segments; }
    const QVector<TrackSegment>& segments() const { return m_segments; }

    // ── Gestió de parades ─────────────────────────────────────────────────────

    void addStop(const StopPoint& stop) { m_stops.append(stop); }
    void clearStops() { m_stops.clear(); }

    // ── Càlcul ────────────────────────────────────────────────────────────────

    /**
     * Calcula els temps de tots els segments i assigna timestamps
     * a tots els TrackPoints (necessari per al Virtual Partner).
     * Retorna resum en text.
     */
    QString compute() {
        if (m_points.isEmpty())
            return "No hi ha punts GPX carregats.";

        // 1. Calcula temps per segment
        for (TrackSegment& seg : m_segments)
            m_estimator.computeSegment(seg);

        // 2. Assigna timestamps punt a punt
        assignTimestamps();

        // 3. Genera resum
        return buildSummary();
    }

    // ── Exportació ────────────────────────────────────────────────────────────

    bool exportGPX(const QString& outputPath, const QString& trackName = "Track planificat") {
        QVector<GPXParser::WayPoint> waypoints;
        for (const TrackSegment& seg : m_segments) {
            int endIdx = qMin(seg.endIdx, m_points.size() - 1);
            if (endIdx < 0) continue;
            const TrackPoint& endPt = m_points[endIdx];
            if (!endPt.time.isValid()) continue;

            qint64 sec = m_startTime.secsTo(endPt.time);
            int h = static_cast<int>(sec / 3600);
            int m = static_cast<int>((sec % 3600) / 60);

            GPXParser::WayPoint wpt;
            wpt.lat   = endPt.lat;
            wpt.lon   = endPt.lon;
            wpt.elevM = endPt.elevM;
            wpt.name  = QString("%1 h %2  -  %1 h %2").arg(h).arg(m);
            waypoints.append(wpt);
        }
        return GPXParser::exportWithTimestamps(m_points, m_startTime, outputPath, trackName, waypoints);
    }

    QString lastError() const { return m_lastError; }

    // ── Estadístiques globals ─────────────────────────────────────────────────

    double totalDistanceKm() const {
        double d = 0;
        for (const auto& s : m_segments) d += s.distanceM;
        return d / 1000.0;
    }

    double totalElevGainM() const {
        double g = 0;
        for (const auto& s : m_segments) g += s.elevGainM;
        return g;
    }

    double totalTimeHours() const {
        double t = 0;
        for (const auto& s : m_segments) t += s.estimatedTimeSec;
        for (const auto& p : m_stops)    t += p.durationSec;
        return t / 3600.0;
    }

private:

    // ── Assignació de timestamps ──────────────────────────────────────────────

    void assignTimestamps() {
        if (m_points.isEmpty()) return;

        // Construeix mapa: índex de punt → temps acumulat (ms)
        QMap<int, qint64> stopMap;
        for (const StopPoint& sp : m_stops)
            stopMap[sp.trackPointIdx] += sp.durationSec * 1000LL;

        qint64 elapsedMs = 0;
        m_points[0].time = m_startTime.addMSecs(elapsedMs);

        for (const TrackSegment& seg : m_segments) {
            int nPts = seg.endIdx - seg.startIdx;
            if (nPts <= 0 || seg.estimatedSpeedMs <= 0) continue;

            double msPerPoint = (seg.estimatedTimeSec * 1000.0) / nPts;

            for (int i = seg.startIdx + 1; i <= seg.endIdx && i < m_points.size(); ++i) {
                elapsedMs += static_cast<qint64>(msPerPoint);

                // Afegeix parada si n'hi ha en aquest punt
                if (stopMap.contains(i))
                    elapsedMs += stopMap[i];

                m_points[i].time = m_startTime.addMSecs(elapsedMs);
            }
        }
    }

    // ── Resum en text ─────────────────────────────────────────────────────────

    QString buildSummary() const {
        QString s;
        s += QStringLiteral("═══════════════════════════════════\n");
        s += QStringLiteral("  RESUM DEL TRACK PLANIFICAT\n");
        s += QStringLiteral("═══════════════════════════════════\n\n");

        for (int i = 0; i < m_segments.size(); ++i) {
            const TrackSegment& seg = m_segments[i];
            double speedKmh = seg.estimatedSpeedMs * 3.6;
            int    mins     = static_cast<int>(seg.estimatedTimeSec / 60.0);
            int    secs     = static_cast<int>(seg.estimatedTimeSec) % 60;

            s += QStringLiteral("Tram %1: %2\n").arg(i+1).arg(seg.label.isEmpty() ? "—" : seg.label);
            s += QStringLiteral("  Distància : %1 km\n").arg(seg.distanceM/1000.0, 0,'f',2);
            s += QStringLiteral("  Pendent   : %1 %\n").arg(seg.avgGradePct, 0,'f',1);
            s += QStringLiteral("  D+ / D-   : +%1 m / -%2 m\n").arg(seg.elevGainM,0,'f',0).arg(seg.elevLossM,0,'f',0);
            s += QStringLiteral("  Potència  : %1 W\n").arg(seg.targetPowerW,0,'f',0);
            s += QStringLiteral("  Velocitat : %1 km/h\n").arg(speedKmh,0,'f',1);
            s += QStringLiteral("  Temps     : %1 min %2 s\n\n").arg(mins).arg(secs,2,10,QChar('0'));
        }

        // Parades
        if (!m_stops.isEmpty()) {
            s += QStringLiteral("── Parades ──────────────────────────\n");
            for (const StopPoint& sp : m_stops) {
                s += QStringLiteral("  Punt %1 — %2 — %3 min\n")
                     .arg(sp.trackPointIdx)
                     .arg(sp.description.isEmpty() ? "Parada" : sp.description)
                     .arg(sp.durationSec / 60);
            }
            s += "\n";
        }

        s += QStringLiteral("── TOTALS ───────────────────────────\n");
        s += QStringLiteral("  Distància total : %1 km\n").arg(totalDistanceKm(),0,'f',2);
        s += QStringLiteral("  Desnivell positiu: +%1 m\n").arg(totalElevGainM(),0,'f',0);
        s += QStringLiteral("  Temps total      : %1 h %2 min\n")
             .arg(static_cast<int>(totalTimeHours()))
             .arg(static_cast<int>((totalTimeHours() - static_cast<int>(totalTimeHours())) * 60));
        s += QStringLiteral("  Sortida          : %1\n").arg(m_startTime.toString("dd/MM/yyyy hh:mm"));
        s += QStringLiteral("  Arribada estimada: %1\n")
             .arg(m_startTime.addSecs(static_cast<qint64>(totalTimeHours()*3600)).toString("dd/MM/yyyy hh:mm"));

        return s;
    }

    // ── Membres ───────────────────────────────────────────────────────────────

    RiderProfile          m_profile;
    TimeEstimator         m_estimator{m_profile};
    QDateTime             m_startTime{QDateTime::currentDateTime()};
    QVector<TrackPoint>   m_points;
    QVector<TrackSegment> m_segments;
    QVector<StopPoint>    m_stops;
    QString               m_lastError;
};
