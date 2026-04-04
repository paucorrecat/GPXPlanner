#pragma once
#include "TrackSegment.h"
#include <QVector>
#include <QString>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDateTime>
#include <QtMath>

/**
 * GPXParser.h
 * Llegeix fitxers GPX i exporta GPX amb timestamps per al Virtual Partner Garmin.
 */
class GPXParser {
public:

    struct WayPoint {
        double  lat;
        double  lon;
        double  elevM;
        QString name;
    };

    // ── Lectura ──────────────────────────────────────────────────────────────

    static QVector<TrackPoint> loadGPX(const QString& filePath, QString& errorOut)
    {
        QVector<TrackPoint> points;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            errorOut = QStringLiteral("No es pot obrir: ") + filePath;
            return points;
        }

        QXmlStreamReader xml(&file);
        TrackPoint current;
        bool inTrkpt = false;

        while (!xml.atEnd()) {
            xml.readNext();

            if (xml.isStartElement()) {
                if (xml.name() == QLatin1String("trkpt") ||
                    xml.name() == QLatin1String("rtept")) {
                    inTrkpt = true;
                    current = {};
                    current.lat = xml.attributes().value("lat").toDouble();
                    current.lon = xml.attributes().value("lon").toDouble();
                }
                else if (inTrkpt && xml.name() == QLatin1String("ele")) {
                    current.elevM = xml.readElementText().toDouble();
                }
                else if (inTrkpt && xml.name() == QLatin1String("time")) {
                    current.time = QDateTime::fromString(
                        xml.readElementText(), Qt::ISODate);
                }
            }
            else if (xml.isEndElement()) {
                if (xml.name() == QLatin1String("trkpt") ||
                    xml.name() == QLatin1String("rtept")) {
                    points.append(current);
                    inTrkpt = false;
                }
            }
        }

        if (xml.hasError())
            errorOut = xml.errorString();

        return points;
    }

    // ── Exportació amb timestamps (per al Virtual Partner) ──────────────────

    /**
     * Exporta un GPX amb <time> a cada punt.
     * El Virtual Partner de Garmin segueix els timestamps per simular el ritme.
     *
     * @param points     Punts del track (lat/lon/ele ja omplerts)
     * @param startTime  Hora d'inici del recorregut
     * @param filePath   Fitxer de sortida
     * @param trackName  Nom del track
     */
    static bool exportWithTimestamps(
        QVector<TrackPoint>& points,
        const QDateTime& startTime,
        const QString& filePath,
        const QString& trackName = "Track planificat",
        const QVector<WayPoint>& waypoints = {})
    {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;

        QXmlStreamWriter xml(&file);
        xml.setAutoFormatting(true);
        xml.writeStartDocument();

        xml.writeStartElement("gpx");
        xml.writeAttribute("version",   "1.1");
        xml.writeAttribute("creator",   "GPXPlanner Qt");
        xml.writeAttribute("xmlns",     "http://www.topografix.com/GPX/1/1");
        xml.writeAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");

        for (const WayPoint& wpt : waypoints) {
            xml.writeStartElement("wpt");
            xml.writeAttribute("lat", QString::number(wpt.lat, 'f', 8));
            xml.writeAttribute("lon", QString::number(wpt.lon, 'f', 8));
            xml.writeTextElement("ele",  QString::number(wpt.elevM, 'f', 1));
            xml.writeTextElement("name", wpt.name);
            xml.writeEndElement(); // wpt
        }

        xml.writeStartElement("trk");
        xml.writeTextElement("name", trackName);
        xml.writeStartElement("trkseg");

        for (const TrackPoint& pt : points) {
            xml.writeStartElement("trkpt");
            xml.writeAttribute("lat", QString::number(pt.lat,  'f', 8));
            xml.writeAttribute("lon", QString::number(pt.lon,  'f', 8));
            xml.writeTextElement("ele",  QString::number(pt.elevM, 'f', 1));
            if (pt.time.isValid())
                xml.writeTextElement("time", pt.time.toUTC().toString(Qt::ISODate));
            xml.writeEndElement(); // trkpt
        }

        xml.writeEndElement(); // trkseg
        xml.writeEndElement(); // trk
        xml.writeEndElement(); // gpx
        xml.writeEndDocument();

        return true;
    }

    // ── Càlcul de segments a partir dels punts ───────────────────────────────

    /**
     * Divideix el track en N segments iguals (o definits per índexs)
     * i calcula distància, pendent i elevació mitjana de cadascun.
     */
    static QVector<TrackSegment> buildSegments(
        const QVector<TrackPoint>& points,
        const QVector<QPair<int,int>>& segmentRanges)
    {
        QVector<TrackSegment> segments;
        for (const auto& range : segmentRanges) {
            TrackSegment seg;
            seg.startIdx = range.first;
            seg.endIdx   = range.second;

            double totalDist = 0.0;
            double totalElev = 0.0;
            double gainM     = 0.0;
            double lossM     = 0.0;

            for (int i = range.first; i < range.second && i+1 < points.size(); ++i) {
                double d = points[i].distanceTo(points[i+1]);
                totalDist += d;
                double de = points[i+1].elevM - points[i].elevM;
                if (de > 0) gainM += de;
                else        lossM -= de;
                totalElev += points[i].elevM;
            }

            seg.distanceM   = totalDist;
            seg.elevGainM   = gainM;
            seg.elevLossM   = lossM;
            seg.avgElevM    = (range.second > range.first)
                              ? totalElev / (range.second - range.first) : 0.0;
            seg.avgGradePct = (totalDist > 0)
                              ? ((gainM - lossM) / totalDist) * 100.0 : 0.0;
            segments.append(seg);
        }
        return segments;
    }
};
