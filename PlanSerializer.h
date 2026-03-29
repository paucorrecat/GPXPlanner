#pragma once
#include "StopPoint.h"
#include "RiderProfile.h"
#include <QVector>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

/**
 * PlanSerializer.h
 *
 * Gestiona la persistència de la planificació en un fitxer .plan.xml
 * separat del GPX original (que mai es toca).
 *
 * Fitxers:
 *   track.gpx           → track original (només lectura)
 *   track.plan.xml      → pla definitiu (desa manual)
 *   track.plan.xml.tmp  → autoguardat (s'esborra en desar el definitiu)
 *
 * Format XML:
 *   <GPXPlan version="1.0" gpxFile="track.gpx">
 *     <RiderProfile mass="85" ftp="200" cda="0.40" crr="0.015"/>
 *     <Segments>
 *       <Segment name="Tram 1" startIdx="0" endIdx="450" powerW="180"/>
 *       ...
 *     </Segments>
 *     <Stops>
 *       <Stop pointIdx="450" durationMin="10" description="Esmorzar"/>
 *       ...
 *     </Stops>
 *   </GPXPlan>
 */
class PlanSerializer
{
public:

    struct Plan {
        RiderProfile        profile;
        QVector<int>        divisors;      // índexs GPX de les fronteres
        QVector<QString>    segNames;      // nom de cada tram
        QVector<double>     segPowers;     // potència de cada tram (W)
        QVector<double>     segTerrains;   // factor terreny de cada tram [0.05, 2.0]
        QVector<StopPoint>  stops;
    };

    // ── Paths ─────────────────────────────────────────────────────────────────

    static QString planPath(const QString& gpxPath)
    {
        return gpxPath + ".plan.xml";           // track.gpx → track.gpx.plan.xml
    }

    static QString tmpPath(const QString& gpxPath)
    {
        return planPath(gpxPath) + ".tmp";
    }

    // ── Escriptura ────────────────────────────────────────────────────────────

    static bool save(const QString& gpxPath, const Plan& plan, bool isTemp = false)
    {
        QString outPath = isTemp ? tmpPath(gpxPath) : planPath(gpxPath);
        QFile file(outPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;

        QXmlStreamWriter xml(&file);
        xml.setAutoFormatting(true);
        xml.writeStartDocument();

        xml.writeStartElement("GPXPlan");
        xml.writeAttribute("version", "1.0");
        xml.writeAttribute("gpxFile", QFileInfo(gpxPath).fileName());

        // Perfil
        xml.writeStartElement("RiderProfile");
        xml.writeAttribute("mass", QString::number(plan.profile.totalMassKg, 'f', 1));
        xml.writeAttribute("ftp",  QString::number(plan.profile.ftpWatts,    'f', 0));
        xml.writeAttribute("cda",  QString::number(plan.profile.cda,         'f', 3));
        xml.writeAttribute("crr",  QString::number(plan.profile.crr,         'f', 4));
        xml.writeEndElement();

        // Segments: guardem els divisors + nom + potència
        xml.writeStartElement("Segments");
        // Reconstruïm els índexs d'inici/fi a partir dels divisors
        // startIdx del segment i = divisors[i-1] (o 0), endIdx = divisors[i] (o últim)
        // Però no sabem l'últim aquí, així que guardem els divisors directament
        for (int i = 0; i < plan.divisors.size(); ++i) {
            xml.writeStartElement("Divisor");
            xml.writeAttribute("pointIdx", QString::number(plan.divisors[i]));
            xml.writeEndElement();
        }
        for (int i = 0; i < plan.segNames.size(); ++i) {
            xml.writeStartElement("Segment");
            xml.writeAttribute("index",   QString::number(i));
            xml.writeAttribute("name",    plan.segNames.value(i, QString("Tram %1").arg(i+1)));
            xml.writeAttribute("powerW",  QString::number(plan.segPowers.value(i, 150.0), 'f', 0));
            xml.writeAttribute("terrain", QString::number(plan.segTerrains.value(i, 1.0), 'f', 2));
            xml.writeEndElement();
        }
        xml.writeEndElement(); // Segments

        // Parades
        xml.writeStartElement("Stops");
        for (const StopPoint& sp : plan.stops) {
            xml.writeStartElement("Stop");
            xml.writeAttribute("pointIdx",    QString::number(sp.trackPointIdx));
            xml.writeAttribute("durationMin", QString::number(sp.durationSec / 60));
            xml.writeAttribute("description", sp.description);
            xml.writeEndElement();
        }
        xml.writeEndElement(); // Stops

        xml.writeEndElement(); // GPXPlan
        xml.writeEndDocument();
        return true;
    }

    // ── Lectura ───────────────────────────────────────────────────────────────

    static bool load(const QString& gpxPath, Plan& planOut, bool tryTemp = true)
    {
        // Prioritza el temporal si existeix
        QString path = (tryTemp && QFile::exists(tmpPath(gpxPath)))
                       ? tmpPath(gpxPath)
                       : planPath(gpxPath);

        if (!QFile::exists(path)) return false;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;

        QXmlStreamReader xml(&file);
        planOut = Plan{};

        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) continue;

            const auto name = xml.name();

            if (name == QLatin1String("RiderProfile")) {
                auto a = xml.attributes();
                planOut.profile.totalMassKg = a.value("mass").toDouble();
                planOut.profile.ftpWatts    = a.value("ftp").toDouble();
                planOut.profile.cda         = a.value("cda").toDouble();
                planOut.profile.crr         = a.value("crr").toDouble();
            }
            else if (name == QLatin1String("Divisor")) {
                planOut.divisors.append(xml.attributes().value("pointIdx").toInt());
            }
            else if (name == QLatin1String("Segment")) {
                auto a = xml.attributes();
                int idx = a.value("index").toInt();
                // Expandeix els vectors si cal
                while (planOut.segNames.size()    <= idx) planOut.segNames.append("");
                while (planOut.segPowers.size()   <= idx) planOut.segPowers.append(150.0);
                while (planOut.segTerrains.size() <= idx) planOut.segTerrains.append(1.0);
                planOut.segNames[idx]    = a.value("name").toString();
                planOut.segPowers[idx]   = a.value("powerW").toDouble();
                planOut.segTerrains[idx] = a.value("terrain").isEmpty() ? 1.0 : a.value("terrain").toDouble();
            }
            else if (name == QLatin1String("Stop")) {
                auto a = xml.attributes();
                StopPoint sp;
                sp.trackPointIdx = a.value("pointIdx").toInt();
                sp.durationSec   = a.value("durationMin").toInt() * 60;
                sp.description   = a.value("description").toString();
                planOut.stops.append(sp);
            }
        }
        return !xml.hasError();
    }

    // ── Promoció temporal → definitiu ─────────────────────────────────────────

    /** Copia .tmp → .plan.xml i esborra el temporal. */
    static bool promoteTmpToFinal(const QString& gpxPath)
    {
        QString tmp   = tmpPath(gpxPath);
        QString final = planPath(gpxPath);
        if (!QFile::exists(tmp)) return false;
        QFile::remove(final);
        bool ok = QFile::copy(tmp, final);
        if (ok) QFile::remove(tmp);
        return ok;
    }

    /** Esborra el temporal (per exemple en tancar sense desar). */
    static void discardTmp(const QString& gpxPath)
    {
        QFile::remove(tmpPath(gpxPath));
    }

    static bool hasPlan(const QString& gpxPath)
    {
        return QFile::exists(planPath(gpxPath)) || QFile::exists(tmpPath(gpxPath));
    }

    static bool hasUnsavedTmp(const QString& gpxPath)
    {
        return QFile::exists(tmpPath(gpxPath));
    }
};
