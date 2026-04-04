# GPXPlanner — Virtual Partner Generator

Aplicació Qt/C++ per planificar tracks ciclistes i exportar-los amb timestamps
per al **Virtual Partner** dels dispositius Garmin.

---

## Entorn de desenvolupament

| Element | Versió |
|---|---|
| SO | Ubuntu 24 |
| IDE | Qt Creator 17 |
| Qt | 6.5.2 GCC 64 bit |
| Estàndard C++ | C++17 |
| Mòduls Qt | core · gui · widgets · xml · charts |

---

## Arquitectura — fitxers del projecte

### Fitxers de dades / lògica (header-only)

| Fitxer | Responsabilitat |
|---|---|
| `TrackSegment.h` | Defineix `TrackPoint` (lat/lon/ele/time) i `TrackSegment` (estadístiques d'un tram). Inclou la fórmula haversine per calcular distàncies. Cada segment té `targetPowerW`, `windSpeedMs` i `terrainFactor` (multiplicador de velocitat [0.05, 2.0]; 1.0 = asfaltat pla). |
| `RiderProfile.h` | Paràmetres físics del ciclista: massa, FTP, CdA, Crr, eficiència transmissió, densitat aire. Inclou ajust de densitat per altitud. |
| `StopPoint.h` | Una parada: índex del punt GPX, durada en segons, descripció. |
| `TimeEstimator.h` | **Motor físic principal.** Donada una potència (W) i un pendent (%), calcula la velocitat sostenible resolent l'equació de potència per bisecció. Model: P = (Fg + Fr + Fa) × v / η. La velocitat física resultant es multiplica per `terrainFactor` per incorporar dificultats no modelables (camí estret, fang, tècnica, exposició, etc.). |
| `GPXParser.h` | Llegeix fitxers `.gpx` (trkpt i rtept). Exporta GPX amb `<time>` a cada punt (format requerit pel Virtual Partner). Calcula estadístiques de segments (distància, D+, D−, pendent mitjà). |
| `TrackPlanner.h` | **Orquestrador.** Connecta càrrega GPX → segments → parades → càlcul de timestamps → exportació. Genera el resum en text. |
| `PlanSerializer.h` | Desa i carrega la planificació (trams, parades, potències, factors de terreny, perfil) en un fitxer `.plan.xml` separat. Gestiona l'autoguardat en un `.tmp`. |

### Interfície gràfica

| Fitxer | Responsabilitat |
|---|---|
| `ElevationChartView.h` | Subclasse de `QChartView`. Afegeix interacció directa al gràfic: drag de divisors, pan, zoom Ctrl+Scroll, reset amb doble clic, menú contextual per afegir/eliminar trams i parades. |
| `MainWindow.h` / `MainWindow.cpp` | Finestra principal. Gestiona tota la UI i connecta la lògica amb la vista. Inclou `TerrainDelegate`, un `QStyledItemDelegate` que mostra un `QDoubleSpinBox` [0.05, 2.00] pas 0.05 quan l'usuari edita la columna Terreny. |
| `main.cpp` | Punt d'entrada. |

---

## Model de dades de la UI

```
m_loadedPoints   QVector<TrackPoint>   — punts GPX carregats (cache, no es rellegen)
m_cumDistKm      QVector<double>       — distàncies acumulades per punt (km)
m_divisors       QVector<int>          — índexs GPX de les fronteres entre trams
                                         (sense el punt 0 ni l'últim)
m_stops          QVector<StopPoint>    — parades actuals
m_currentGpxPath QString               — path del GPX actiu
```

**Exemple amb 3 trams i 1000 punts:**
```
m_divisors = {333, 666}
Tram 1: punts [0   → 333]
Tram 2: punts [333 → 666]
Tram 3: punts [666 → 999]
```

---

## Model físic (TimeEstimator)

```
P_total = (F_gravetat + F_rodolament + F_aerodinàmica) × v / η_transmissió

F_gravetat     = m × g × sin(θ)
F_rodolament   = m × g × Crr × cos(θ)
F_aerodinàmica = 0.5 × ρ × CdA × (v + v_vent)²

v_efectiva = v_física × terrainFactor
```

La velocitat es resol per **bisecció** en [0, 30] m/s (64 iteracions).
Si la potència no és suficient per avançar, retorna 0.3 m/s (~1 km/h).
El `terrainFactor` s'aplica com a **multiplicador final** sobre la velocitat física
per modelar dificultats que el model de forces no captura (fang, rocam, camins tècnics…).

Valors per defecte orientats a MTB:
- Massa: 85 kg · FTP: 200 W · CdA: 0.40 m² · Crr: 0.015 · terrainFactor: 1.0

---

## Sistema de persistència de la planificació

El fitxer GPX original **mai es modifica**.

```
track.gpx                  ← track original (només lectura)
track.gpx.plan.xml         ← pla definitiu (botó "Desar pla")
track.gpx.plan.xml.tmp     ← autoguardat automàtic en cada canvi
```

**Flux:**
1. Cada canvi (divisor, parada, potència, terreny, càlcul) → desa al `.tmp`
2. Botó "📋 Desar pla" → promou `.tmp` → `.plan.xml` i esborra el temporal
3. En carregar un GPX → carrega `.tmp` si existeix, si no el `.plan.xml`
4. En tancar sense desar → el `.tmp` es conserva per a la propera sessió

**Contingut del `.plan.xml`:**
```xml
<GPXPlan version="1.0" gpxFile="track.gpx">
  <RiderProfile mass="85" ftp="200" cda="0.40" crr="0.015"/>
  <Segments>
    <Divisor pointIdx="333"/>
    <Divisor pointIdx="666"/>
    <Segment index="0" name="Pujada Collserola" powerW="220" terrain="0.70"/>
    <Segment index="1" name="Baixada" powerW="150" terrain="0.85"/>
    <Segment index="2" name="Pla" powerW="180" terrain="1.00"/>
  </Segments>
  <Stops>
    <Stop pointIdx="450" durationMin="10" description="Esmorzar"/>
  </Stops>
</GPXPlan>
```

---

## Interfície gràfica — layout

```
┌─────────────────────────────────────────────────────────────────────┐
│  Track entrada  [Explorar]  [Carregar GPX]                          │
│  Track sortida  [Desar com...]  [💾 Exportar GPX]                   │
│  Hora sortida   [▶ Calcular]   [📋 Desar pla]                       │
├─────────────────────────────────────────────────────────────────────┤
│  Gràfic d'elevació (redimensionable via splitter)                   │
│  · Barres de divisió de trams (colors)                              │
│  · Icones de parada (cercles taronja)                               │
│  · Clic dret: menú afegir tram / afegir parada / eliminar           │
│  · Arrossegar barres: mou divisors en temps real                    │
│  · Ctrl+Scroll: zoom horitzontal · Doble clic: reset zoom           │
├───────────────────────────┬─────────────────────────────────────────┤
│  Perfil del ciclista      │  Resum (QLabels)                        │
│  · Massa, FTP, CdA, Crr  │  · Trams, Distància, D+, D−            │
│                           │  · Temps total, Vel. mitjana, Arribada  │
├───────────────────────────┴─────────────────────────────────────────┤
│  Taula de trams (colors per tram)                                   │
│  Nom | Pk ini | Pk fi | Dist | Pend | D+ | D− | Alt fi |           │
│  Potència (editable) | Terreny (editable) | Velocitat | Temps |     │
│  Temps acum.                                                         │
├─────────────────────────────────────────────────────────────────────┤
│  Taula de parades (afegir des del gràfic, eliminar des de la taula) │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Taula de trams — columnes

| # | Nom | Editable | Descripció |
|---|---|---|---|
| 0 | Nom | ✅ | Nom del tram |
| 1 | Pk ini (km) | ❌ | Distància acumulada a l'inici |
| 2 | Pk fi (km) | ❌ | Distància acumulada al final |
| 3 | Dist.(km) | ❌ | Longitud del tram |
| 4 | Pend.(%) | ❌ | Pendent mitjà |
| 5 | D+(m) | ❌ | Desnivell positiu |
| 6 | D−(m) | ❌ | Desnivell negatiu |
| 7 | Alt.fi(m) | ❌ | Altitud al punt final |
| 8 | Pot.(W) | ✅ | Potència objectiu |
| 9 | Terreny | ✅ | Factor de terreny [0.05, 2.0]; 1.0 = asfaltat. Editor SpinBox (pas 0.05). |
| 10 | Vel.(km/h) | ❌ | Calculada post-▶ |
| 11 | Temps | ❌ | Temps del tram (h:min:s) |
| 12 | Temps acum. | ❌ | Temps acumulat des de la sortida |

---

## Interacció amb el gràfic (ElevationChartView)

| Acció | Efecte |
|---|---|
| Arrossegar barra de tram | Mou el divisor en temps real, actualitza estadístiques |
| Clic dret espai buit | Menú: afegir tram / afegir parada (mostra pk en km) |
| Clic dret sobre barra | Menú: eliminar divisor |
| Clic dret sobre icona ⏸ | Menú: eliminar parada |
| Ctrl + Scroll | Zoom horitzontal centrat al cursor |
| Drag (espai buit) | Pan horitzontal |
| Doble clic | Reset zoom (vista completa) |

Tolerància de selecció (SNAP_PX): ±10 píxels.
Separació mínima entre divisors: 5 punts GPX.

---

## Flux d'ús típic

1. **Carregar GPX** → el track es divideix automàticament en 3 trams iguals
2. **Ajustar divisors** al gràfic (arrossegar o clic dret)
3. **Afegir parades** des del gràfic (clic dret → "Afegir parada aquí")
4. **Editar potències** a la taula de trams
5. **Editar factor de terreny** per als trams amb ferm difícil (< 1.0) o ràpid (> 1.0)
6. **▶ Calcular** → omple velocitats, temps i temps acumulats
7. **📋 Desar pla** → guarda la planificació al `.plan.xml`
8. **💾 Exportar GPX** → genera el track amb timestamps per al Garmin

---

## Com continuar el desenvolupament en una nova sessió

Adjunta al xat els fitxers `.h` i `.cpp` del projecte i indica:

> *"Aquí tens el codi del GPXPlanner (Qt 6.5.2 / C++17 / Ubuntu 24).
> És un planificador de tracks GPX per crear Virtual Partners de Garmin.
> Vull fer la següent millora: ..."*

Els fitxers clau a adjuntar per reprendre el context:
- `MainWindow.h` + `MainWindow.cpp`
- `ElevationChartView.h`
- `PlanSerializer.h`
- `TrackPlanner.h` + `GPXParser.h`
- `TrackSegment.h` + `RiderProfile.h` + `TimeEstimator.h` + `StopPoint.h`
