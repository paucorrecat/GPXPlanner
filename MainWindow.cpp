#include "MainWindow.h"

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QTextEdit>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QDir>
#include <QCloseEvent>
#include <QScatterSeries>
#include <QLineSeries>

// ── Colors estàtics ───────────────────────────────────────────────────────────
const QList<QColor> MainWindow::k_segColors = {
    QColor(52,152,219), QColor(46,204,113),  QColor(155,89,182),
    QColor(231,76,60),  QColor(241,196,15),  QColor(26,188,156),
    QColor(230,126,34), QColor(236,64,122),
};

// ── Constructor / Destructor ──────────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_settings("GPXPlanner","GPXPlanner")
{
    setWindowTitle("GPX Planner — Virtual Partner Generator");
    setMinimumSize(1050, 760);
    buildUI();
    loadSettings();
}

MainWindow::~MainWindow()
{
    // En tancar: si hi ha temporal però no definitiu, deixem el temporal.
    // Si hi ha definitiu guardat, esborrem el temporal (ja estava inclòs).
    if (!m_currentGpxPath.isEmpty()
        && !m_hasUnsavedChanges
        && PlanSerializer::hasUnsavedTmp(m_currentGpxPath))
    {
        PlanSerializer::discardTmp(m_currentGpxPath);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  BUILD UI
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildUI()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setSpacing(6);
    root->setContentsMargins(8,8,8,8);

    root->addWidget(buildFilePanel());

    auto* vSplit = new QSplitter(Qt::Vertical);
    vSplit->setChildrenCollapsible(false);
    vSplit->addWidget(buildElevationChart());

    auto* midSplit = new QSplitter(Qt::Horizontal);
    midSplit->addWidget(buildRiderPanel());
    midSplit->addWidget(buildSummaryPanel());
    midSplit->setStretchFactor(0,1);
    midSplit->setStretchFactor(1,2);
    vSplit->addWidget(midSplit);

    auto* botSplit = new QSplitter(Qt::Vertical);
    botSplit->addWidget(buildSegmentsPanel());
    botSplit->addWidget(buildStopsPanel());
    botSplit->setStretchFactor(0,4);
    botSplit->setStretchFactor(1,1);
    vSplit->addWidget(botSplit);

    vSplit->setSizes({220, 160, 320});
    root->addWidget(vSplit, 1);

    m_statusBar = new QLabel("Carrega un fitxer GPX per començar.");
    m_statusBar->setFrameShape(QFrame::StyledPanel);
    m_statusBar->setStyleSheet("padding:3px; background:#f0f0f0;");
    root->addWidget(m_statusBar);
}

// ── File panel ────────────────────────────────────────────────────────────────
// Millora 6: "Exportar GPX" i "Desar pla" van a la fila de sortida
QWidget* MainWindow::buildFilePanel()
{
    auto* box = new QGroupBox("Fitxers");
    auto* g   = new QGridLayout(box);

    // Fila 0: entrada
    g->addWidget(new QLabel("Track entrada (.gpx):"), 0, 0);
    m_inputPath = new QLineEdit;
    m_inputPath->setPlaceholderText("Selecciona o arrossega un fitxer GPX…");
    g->addWidget(m_inputPath, 0, 1);
    auto* btnIn = new QPushButton("Explorar…");
    connect(btnIn, &QPushButton::clicked, this, &MainWindow::onBrowseInput);
    g->addWidget(btnIn, 0, 2);
    m_btnLoad = new QPushButton("Carregar GPX");
    m_btnLoad->setStyleSheet("font-weight:bold;");
    connect(m_btnLoad, &QPushButton::clicked, this, &MainWindow::onLoadGPX);
    g->addWidget(m_btnLoad, 0, 3);

    // Fila 1: sortida + exportar + desar pla (millora 6)
    g->addWidget(new QLabel("Track sortida (.gpx):"), 1, 0);
    m_outputPath = new QLineEdit;
    m_outputPath->setPlaceholderText("On desar el track amb timestamps…");
    g->addWidget(m_outputPath, 1, 1);
    auto* btnOut = new QPushButton("Desar com…");
    connect(btnOut, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
    g->addWidget(btnOut, 1, 2);

    m_btnExport = new QPushButton("💾  Exportar GPX");
    m_btnExport->setEnabled(false);
    m_btnExport->setStyleSheet(
        "font-weight:bold; background:#4CAF50; color:white; padding:4px 10px;");
    connect(m_btnExport, &QPushButton::clicked, this, &MainWindow::onExport);
    g->addWidget(m_btnExport, 1, 3);

    // Fila 2: hora sortida + Calcular + Desar pla
    g->addWidget(new QLabel("Hora de sortida:"), 2, 0);
    m_startTime = new QDateTimeEdit(QDateTime::currentDateTime());
    m_startTime->setDisplayFormat("dd/MM/yyyy  HH:mm");
    m_startTime->setCalendarPopup(true);
    g->addWidget(m_startTime, 2, 1);

    m_btnCompute = new QPushButton("▶  Calcular");
    m_btnCompute->setEnabled(false);
    m_btnCompute->setStyleSheet(
        "font-weight:bold; background:#2196F3; color:white; padding:4px 10px;");
    connect(m_btnCompute, &QPushButton::clicked, this, &MainWindow::onCompute);
    g->addWidget(m_btnCompute, 2, 2);

    m_btnSavePlan = new QPushButton("📋  Desar pla");
    m_btnSavePlan->setEnabled(false);
    m_btnSavePlan->setStyleSheet(
        "font-weight:bold; background:#FF9800; color:white; padding:4px 10px;");
    m_btnSavePlan->setToolTip("Desa la planificació (trams, parades, potències) al fitxer .plan.xml");
    connect(m_btnSavePlan, &QPushButton::clicked, this, &MainWindow::onSavePlan);
    g->addWidget(m_btnSavePlan, 2, 3);

    g->setColumnStretch(1, 1);
    return box;
}

// ── Rider panel ───────────────────────────────────────────────────────────────
// Millora 1: sense QLabel "Trams"
QWidget* MainWindow::buildRiderPanel()
{
    auto* box = new QGroupBox("Perfil del ciclista");
    auto* g   = new QGridLayout(box);

    auto addRow = [&](int row, const QString& lbl, QDoubleSpinBox*& spin,
                      double mn, double mx, double val, double step,
                      const QString& sfx, int dec=3)
    {
        g->addWidget(new QLabel(lbl), row, 0);
        spin = new QDoubleSpinBox;
        spin->setRange(mn,mx); spin->setValue(val);
        spin->setSingleStep(step); spin->setSuffix(sfx); spin->setDecimals(dec);
        g->addWidget(spin, row, 1);
    };

    addRow(0, "Massa total (ciclista+bici):", m_mass, 40,   200,  85.0, 1.0,  " kg", 1);
    addRow(1, "FTP (potència llindar):",      m_ftp,  50,   500, 200.0, 5.0,  " W",  0);
    addRow(2, "CdA (aerodinàmica MTB):",      m_cda,  0.1,  1.0,  0.40, 0.01, " m²", 2);
    addRow(3, "Crr (rodolament terra):",       m_crr,  0.001,0.05, 0.015,0.001,"",   3);

    // Millora 1: NO hi ha fila de "Trams" aquí
    g->setRowStretch(4, 1);
    return box;
}

// ── Segments panel ────────────────────────────────────────────────────────────
// Columnes: Nom | Pk ini | Pk fi | Dist | Pend | D+ | D- | Alt fi | Pot | Vel | Temps | Temps acum
QWidget* MainWindow::buildSegmentsPanel()
{
    auto* box = new QGroupBox("Trams (segments)");
    auto* v   = new QVBoxLayout(box);

    m_segTable = new QTableWidget(0, ColCount);
    m_segTable->setHorizontalHeaderLabels({
        "Nom",
        "Pk ini (km)", "Pk fi (km)",
        "Dist.(km)", "Pend.(%)", "D+(m)", "D−(m)", "Alt.fi(m)",
        "Pot.(W)",
        "Terreny (×)",
        "Vel.(km/h)", "Temps", "Temps acum."
    });
    m_segTable->setItemDelegateForColumn(ColTerrain, new TerrainDelegate(m_segTable));
    m_segTable->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_segTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_segTable->setAlternatingRowColors(false);   // els colors de fila els posem nosaltres
    m_segTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Desactiva el highlight blau de selecció — els colors de fila ja indiquen el tram
    m_segTable->setStyleSheet(
        "QTableWidget::item:selected {"
        "  background: transparent;"
        "  color: black;"
        "}");
    v->addWidget(m_segTable);

    auto* hint = new QLabel(
        "💡  Clic dret al gràfic per afegir/eliminar divisors i parades.  "
        "Arrossega les barres verticals per moure-les.  "
        "Ctrl+Scroll: zoom · Doble clic: reset zoom.");
    hint->setStyleSheet("color:gray; font-size:10px;");
    hint->setWordWrap(true);
    v->addWidget(hint);
    return box;
}

// ── Stops panel ───────────────────────────────────────────────────────────────
QWidget* MainWindow::buildStopsPanel()
{
    auto* box = new QGroupBox("Parades");
    auto* v   = new QVBoxLayout(box);

    m_stopTable = new QTableWidget(0, 3);
    m_stopTable->setHorizontalHeaderLabels(
        {"Punt GPX (índex)", "Durada (min)", "Descripció"});
    m_stopTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_stopTable->setAlternatingRowColors(true);
    m_stopTable->setMaximumHeight(140);
    v->addWidget(m_stopTable);

    auto* btns  = new QHBoxLayout;
    auto* hint2 = new QLabel("⏸  Afegeix parades fent clic dret al gràfic.");
    hint2->setStyleSheet("color:gray; font-size:10px;");
    auto* btnDel = new QPushButton("－ Eliminar parada seleccionada");
    connect(btnDel, &QPushButton::clicked, this, &MainWindow::onRemoveStop);
    btns->addWidget(hint2); btns->addStretch(); btns->addWidget(btnDel);
    v->addLayout(btns);
    return box;
}

// ── Summary panel ─────────────────────────────────────────────────────────────
QWidget* MainWindow::buildSummaryPanel()
{
    auto* box = new QGroupBox("Resum");
    auto* g   = new QGridLayout(box);
    g->setVerticalSpacing(4);

    auto mkVal = []() {
        auto* l = new QLabel("—");
        l->setStyleSheet(
            "background:#f9f9f9; border:1px solid #ddd; border-radius:3px;"
            "padding:2px 6px; font-weight:bold;");
        return l;
    };

    int row = 0;
    auto add = [&](const QString& lbl, QLabel*& out) {
        g->addWidget(new QLabel(lbl), row, 0);
        out = mkVal(); g->addWidget(out, row, 1); ++row;
    };

    add("Trams:",             m_lblNSegs);
    add("Distància total:",   m_lblTotalDist);
    add("D+ total:",          m_lblTotalDplus);
    add("D− total:",          m_lblTotalDminus);
    add("Temps total:",       m_lblTotalTime);
    add("Velocitat mitjana:", m_lblAvgSpeed);
    add("Hora d'arribada:",   m_lblArrival);

    g->setRowStretch(row, 1);
    g->setColumnStretch(1, 1);
    return box;
}

// ── Elevation chart ───────────────────────────────────────────────────────────
// Millora 4: llegenda oculta | Millora a (títol eliminat)
QWidget* MainWindow::buildElevationChart()
{
    auto* box = new QGroupBox(
        "Perfil d'elevació  —  Clic dret: afegir tram/parada  ·  "
        "Ctrl+Scroll: zoom  ·  Doble clic: reset zoom");
    auto* v = new QVBoxLayout(box);
    box->setMinimumHeight(150);

    m_upperLine  = new QLineSeries();
    m_lowerLine  = new QLineSeries();
    m_areaSeries = new QAreaSeries(m_upperLine, m_lowerLine);

    QLinearGradient grad(QPointF(0,0), QPointF(0,1));
    grad.setColorAt(0.0, QColor(255,140,  0, 210));
    grad.setColorAt(1.0, QColor(255,200,100,  60));
    grad.setCoordinateMode(QGradient::ObjectBoundingMode);
    m_areaSeries->setBrush(grad);
    m_areaSeries->setPen(QPen(QColor(220,80,0), 1.8));

    m_chart = new QChart();
    m_chart->addSeries(m_areaSeries);
    m_chart->setTitle("");
    m_chart->legend()->hide();          // Millora 4: llegenda sempre oculta
    m_chart->setMargins(QMargins(2,2,2,2));
    m_chart->setBackgroundRoundness(0);

    m_axisX = new QValueAxis();
    m_axisX->setTitleText("Distància (km)");
    m_axisX->setLabelFormat("%.1f");
    m_axisX->setTickCount(8);
    m_axisX->setGridLineVisible(true);

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("m");
    m_axisY->setLabelFormat("%d");
    m_axisY->setTickCount(4);
    m_axisY->setGridLineVisible(true);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_areaSeries->attachAxis(m_axisX);
    m_areaSeries->attachAxis(m_axisY);

    m_chartView = new ElevationChartView(m_chart);
    m_chartView->setAxes(m_axisX, m_axisY);
    m_chartView->setMinimumHeight(130);

    connect(m_chartView, &ElevationChartView::divisorMoved,   this, &MainWindow::onDivisorMoved);
    connect(m_chartView, &ElevationChartView::divisorAdded,   this, &MainWindow::onDivisorAdded);
    connect(m_chartView, &ElevationChartView::divisorRemoved, this, &MainWindow::onDivisorRemoved);
    connect(m_chartView, &ElevationChartView::stopAdded,      this, &MainWindow::onStopAdded);
    connect(m_chartView, &ElevationChartView::stopRemoved,    this, &MainWindow::onStopRemoved);

    v->addWidget(m_chartView);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ELEVATION CHART UPDATE
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updateElevationChart(const QVector<TrackPoint>& points)
{
    if (points.isEmpty()) return;
    m_upperLine->clear(); m_lowerLine->clear();

    double minElev = points[0].elevM, maxElev = points[0].elevM;
    m_upperLine->append(0.0, points[0].elevM);
    m_lowerLine->append(0.0, 0.0);

    for (int i=1; i<points.size(); ++i) {
        double km = m_cumDistKm[i], e = points[i].elevM;
        m_upperLine->append(km, e); m_lowerLine->append(km, 0.0);
        minElev = qMin(minElev,e); maxElev = qMax(maxElev,e);
    }
    double margin = qMax((maxElev-minElev)*0.15, 10.0);
    double xMax   = m_cumDistKm.last();
    m_axisX->setRange(0, xMax);
    m_axisY->setRange(qMax(0.0, minElev-margin), maxElev+margin);
    m_chartView->resetZoomRange(0, xMax);
}

// ─────────────────────────────────────────────────────────────────────────────
//  REDRAW DIVISORS
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::redrawDivisors()
{
    const auto list = m_chart->series();
    for (auto* s : list)
        if (s != m_areaSeries) { m_chart->removeSeries(s); delete s; }

    if (m_cumDistKm.isEmpty()) return;

    QVector<int> boundaries = {0};
    for (int d : m_divisors) boundaries.append(d);
    boundaries.append(m_totalPoints-1);
    int nSeg = boundaries.size()-1;

    double yMin = m_axisY->min(), yMax = m_axisY->max();

    for (int i=0; i<nSeg; ++i) {
        int s0 = qBound(0,boundaries[i],  m_totalPoints-1);
        int s1 = qBound(0,boundaries[i+1],m_totalPoints-1);
        QColor col = k_segColors[i % k_segColors.size()];

        if (i > 0) {
            double x = m_cumDistKm[s0];
            auto* vl = new QLineSeries();
            vl->setPen(QPen(col, 2.0, Qt::DashLine));
            vl->append(x,yMin); vl->append(x,yMax);
            m_chart->addSeries(vl);
            vl->attachAxis(m_axisX); vl->attachAxis(m_axisY);
        }

        // Scatter només per posicionar l'etiqueta dins del gràfic (sense llegenda)
        double midX = (m_cumDistKm[s0]+m_cumDistKm[s1])/2.0;
        double midY = yMax-(yMax-yMin)*0.07;
        auto* sc = new QScatterSeries();
        sc->setMarkerSize(8);
        sc->setColor(col); sc->setBorderColor(col.darker(130));
        sc->append(midX, midY);
        m_chart->addSeries(sc);
        sc->attachAxis(m_axisX); sc->attachAxis(m_axisY);
    }

    // Icones de parada
    if (!m_stops.isEmpty()) {
        double yStop = yMin+(yMax-yMin)*0.15;
        auto* ss = new QScatterSeries();
        ss->setMarkerShape(QScatterSeries::MarkerShapeCircle);
        ss->setMarkerSize(14);
        ss->setColor(QColor(255,87,34,200));
        ss->setBorderColor(QColor(180,40,0));
        for (const StopPoint& sp : m_stops) {
            int idx = qBound(0,sp.trackPointIdx,m_totalPoints-1);
            ss->append(m_cumDistKm[idx], yStop);
        }
        m_chart->addSeries(ss);
        ss->attachAxis(m_axisX); ss->attachAxis(m_axisY);
    }

    // Millora 4: llegenda sempre oculta
    m_chart->legend()->hide();

    QVector<int> stopIdxs;
    for (const StopPoint& sp : m_stops) stopIdxs.append(sp.trackPointIdx);
    m_chartView->setDivisors(m_divisors, m_totalPoints, m_cumDistKm);
    m_chartView->setStops(stopIdxs);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SEGMENT TABLE
// ─────────────────────────────────────────────────────────────────────────────

// Millora 5: cada fila de la taula rep el color de fons del seu tram
static QColor rowBgColor(const QColor& segColor) {
    // Versió molt pàl·lida del color del tram per al fons de la fila
    return QColor(
        segColor.red()  + (255-segColor.red())  * 4/5,
        segColor.green()+ (255-segColor.green())* 4/5,
        segColor.blue() + (255-segColor.blue()) * 4/5
    );
}

void MainWindow::rebuildSegmentTable()
{
    if (m_loadedPoints.isEmpty()) return;

    QVector<QString> oldNames;
    QVector<double>  oldPowers;
    QVector<double>  oldTerrains;
    for (int i=0; i<m_segTable->rowCount(); ++i) {
        oldNames    << (m_segTable->item(i,ColName)    ? m_segTable->item(i,ColName)->text()               : QString("Tram %1").arg(i+1));
        oldPowers   << (m_segTable->item(i,ColPower)   ? m_segTable->item(i,ColPower)->text().toDouble()   : 150.0);
        oldTerrains << (m_segTable->item(i,ColTerrain) ? m_segTable->item(i,ColTerrain)->text().toDouble() : 1.0);
    }

    QVector<int> boundaries = {0};
    for (int d : m_divisors) boundaries.append(d);
    boundaries.append(m_totalPoints-1);
    int nSeg = boundaries.size()-1;
    m_segTable->setRowCount(nSeg);

    for (int i=0; i<nSeg; ++i) {
        int s0 = boundaries[i], s1 = boundaries[i+1];
        auto segs = GPXParser::buildSegments(m_loadedPoints, {{s0,s1}});

        QString name    = (i < oldNames.size())    ? oldNames[i]    : QString("Tram %1").arg(i+1);
        double  power   = (i < oldPowers.size())   ? oldPowers[i]   : 150.0;
        double  terrain = (i < oldTerrains.size()) ? oldTerrains[i] : 1.0;

        QColor segCol = k_segColors[i % k_segColors.size()];
        QColor bgRO   = rowBgColor(segCol);           // fons columnes RO
        QColor bgName = segCol.lighter(175);           // fons columna Nom (més saturada)
        QColor bgRW   = segCol.lighter(195);           // fons columna Potència (editable)

        auto ro = [&](int col, const QString& txt, QColor bg = QColor()) {
            if (!m_segTable->item(i,col))
                m_segTable->setItem(i, col, new QTableWidgetItem());
            auto* it = m_segTable->item(i,col);
            it->setText(txt);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            it->setBackground(bg.isValid() ? bg : bgRO);
        };
        auto rw = [&](int col, const QString& txt, QColor bg = QColor()) {
            if (!m_segTable->item(i,col))
                m_segTable->setItem(i, col, new QTableWidgetItem());
            auto* it = m_segTable->item(i,col);
            it->setText(txt);
            it->setFlags(it->flags() | Qt::ItemIsEditable);
            it->setBackground(bg.isValid() ? bg : bgRW);
        };

        // Millora 5: nom amb fons del color del tram (més visible)
        rw(ColName, name, bgName);

        ro(ColPkStart, QString::number(m_cumDistKm[s0],'f',2));
        ro(ColPkEnd,   QString::number(m_cumDistKm[qMin(s1,m_totalPoints-1)],'f',2));

        if (!segs.isEmpty()) {
            const auto& seg = segs[0];
            ro(ColDist,   QString::number(seg.distanceM/1000.0,'f',2));
            ro(ColGrade,  QString::number(seg.avgGradePct,'f',1));
            ro(ColDplus,  QString::number(seg.elevGainM,'f',0));
            ro(ColDminus, QString::number(seg.elevLossM,'f',0));
            double altFi = m_loadedPoints[qMin(s1,m_totalPoints-1)].elevM;
            ro(ColAltEnd, QString::number(altFi,'f',0));
        } else {
            for (int c : {ColDist,ColGrade,ColDplus,ColDminus,ColAltEnd}) ro(c,"—");
        }

        rw(ColPower,   QString::number(power,'f',0));
        rw(ColTerrain, QString::number(terrain,'f',2));
        ro(ColSpeed,   "—");
        ro(ColTime,    "—");
        ro(ColCumTime, "—");
    }

    updateSegCountDisplay();
    updateSummaryLabels();
}

void MainWindow::refreshSegmentStats()
{
    QVector<int> boundaries = {0};
    for (int d : m_divisors) boundaries.append(d);
    boundaries.append(m_totalPoints-1);
    int nSeg = qMin(m_segTable->rowCount(), boundaries.size()-1);

    for (int i=0; i<nSeg; ++i) {
        int s0=boundaries[i], s1=boundaries[i+1];
        auto segs = GPXParser::buildSegments(m_loadedPoints, {{s0,s1}});
        if (segs.isEmpty()) continue;

        QColor bgRO = rowBgColor(k_segColors[i % k_segColors.size()]);

        auto upd = [&](int col, const QString& txt) {
            if (!m_segTable->item(i,col))
                m_segTable->setItem(i,col,new QTableWidgetItem());
            auto* it = m_segTable->item(i,col);
            it->setText(txt);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            it->setBackground(bgRO);
        };

        upd(ColPkStart, QString::number(m_cumDistKm[s0],'f',2));
        upd(ColPkEnd,   QString::number(m_cumDistKm[qMin(s1,m_totalPoints-1)],'f',2));
        upd(ColDist,    QString::number(segs[0].distanceM/1000.0,'f',2));
        upd(ColGrade,   QString::number(segs[0].avgGradePct,'f',1));
        upd(ColDplus,   QString::number(segs[0].elevGainM,'f',0));
        upd(ColDminus,  QString::number(segs[0].elevLossM,'f',0));
        double altFi = m_loadedPoints[qMin(s1,m_totalPoints-1)].elevM;
        upd(ColAltEnd,  QString::number(altFi,'f',0));
    }
}

// Millora 2: format h:min:s   Millora 3: columna Temps acumulat
void MainWindow::refreshSpeedTimeColumns()
{
    const auto& segs = m_planner.segments();
    double cumSec = 0.0;

    for (int i=0; i<segs.size() && i<m_segTable->rowCount(); ++i) {
        double kmh = segs[i].estimatedSpeedMs * 3.6;
        double sec = segs[i].estimatedTimeSec;

        // Afegeix parades que cauen dins d'aquest tram
        int s0 = (i==0) ? 0 : m_divisors[i-1];
        int s1 = (i < m_divisors.size()) ? m_divisors[i] : m_totalPoints-1;
        for (const StopPoint& sp : m_stops)
            if (sp.trackPointIdx >= s0 && sp.trackPointIdx <= s1)
                sec += sp.durationSec;

        cumSec += sec;

        QColor bgCalc = k_segColors[i%k_segColors.size()].lighter(195);
        QColor bgCalcG= k_segColors[i%k_segColors.size()].lighter(185);

        auto upd = [&](int col, const QString& txt, QColor bg) {
            if (!m_segTable->item(i,col))
                m_segTable->setItem(i,col,new QTableWidgetItem());
            auto* it = m_segTable->item(i,col);
            it->setText(txt);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            it->setBackground(bg);
        };

        upd(ColSpeed,   QString("%1 km/h").arg(kmh,0,'f',1), bgCalc);
        upd(ColTime,    formatTime(sec),                      bgCalc);    // millora 2
        upd(ColCumTime, formatTime(cumSec),                   bgCalcG);   // millora 3
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SUMMARY LABELS
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updateSummaryLabels()
{
    int nSeg = m_divisors.size()+1;
    m_lblNSegs->setText(QString::number(nSeg));

    if (m_cumDistKm.isEmpty()) return;

    double totalDist=m_cumDistKm.last(), dp=0, dm=0;
    for (int i=1; i<m_loadedPoints.size(); ++i) {
        double de=m_loadedPoints[i].elevM-m_loadedPoints[i-1].elevM;
        if (de>0) dp+=de; else dm-=de;
    }
    m_lblTotalDist->setText(QString("%1 km").arg(totalDist,0,'f',1));
    m_lblTotalDplus->setText(QString("+%1 m").arg(dp,0,'f',0));
    m_lblTotalDminus->setText(QString("−%1 m").arg(dm,0,'f',0));

    const auto& segs = m_planner.segments();
    bool hasTime = !segs.isEmpty() && segs[0].estimatedTimeSec > 0.0;
    if (hasTime) {
        double totalSec=0;
        for (const auto& s : segs) totalSec += s.estimatedTimeSec;
        for (const auto& sp : m_stops) totalSec += sp.durationSec;

        m_lblTotalTime->setText(formatTime(totalSec));
        double avgKmh = (totalSec>0) ? totalDist/(totalSec/3600.0) : 0.0;
        m_lblAvgSpeed->setText(QString("%1 km/h").arg(avgKmh,0,'f',1));
        m_lblArrival->setText(
            m_startTime->dateTime().addSecs(static_cast<qint64>(totalSec))
            .toString("dd/MM/yyyy  HH:mm"));
    } else {
        m_lblTotalTime->setText("—");
        m_lblAvgSpeed->setText("—");
        m_lblArrival->setText("—");
    }
}

void MainWindow::updateSegCountDisplay()
{
    m_lblNSegs->setText(QString::number(m_divisors.size()+1));
}

void MainWindow::updateTitleBar()
{
    QString title = "GPX Planner — Virtual Partner Generator";
    if (!m_currentGpxPath.isEmpty()) {
        title += "  [" + QFileInfo(m_currentGpxPath).fileName() + "]";
        if (m_hasUnsavedChanges) title += "  *";
    }
    setWindowTitle(title);
}

// ─────────────────────────────────────────────────────────────────────────────
//  AUTOGUARDAT / SERIALITZACIÓ
// ─────────────────────────────────────────────────────────────────────────────
PlanSerializer::Plan MainWindow::currentPlan() const
{
    PlanSerializer::Plan plan;
    plan.profile.totalMassKg = m_mass->value();
    plan.profile.ftpWatts    = m_ftp->value();
    plan.profile.cda         = m_cda->value();
    plan.profile.crr         = m_crr->value();
    plan.divisors = m_divisors;
    plan.stops    = m_stops;
    for (int i=0; i<m_segTable->rowCount(); ++i) {
        plan.segNames    << (m_segTable->item(i,ColName)    ? m_segTable->item(i,ColName)->text()               : QString("Tram %1").arg(i+1));
        plan.segPowers   << (m_segTable->item(i,ColPower)   ? m_segTable->item(i,ColPower)->text().toDouble()   : 150.0);
        plan.segTerrains << (m_segTable->item(i,ColTerrain) ? m_segTable->item(i,ColTerrain)->text().toDouble() : 1.0);
    }
    return plan;
}

void MainWindow::autoSaveTmp()
{
    if (m_currentGpxPath.isEmpty()) return;
    PlanSerializer::save(m_currentGpxPath, currentPlan(), /*isTemp=*/true);
    m_hasUnsavedChanges = true;
    m_btnSavePlan->setEnabled(true);
    updateTitleBar();
}

void MainWindow::applyPlan(const PlanSerializer::Plan& plan)
{
    // Perfil
    m_mass->setValue(plan.profile.totalMassKg > 0 ? plan.profile.totalMassKg : 85.0);
    m_ftp ->setValue(plan.profile.ftpWatts    > 0 ? plan.profile.ftpWatts    : 200.0);
    m_cda ->setValue(plan.profile.cda         > 0 ? plan.profile.cda         : 0.40);
    m_crr ->setValue(plan.profile.crr         > 0 ? plan.profile.crr         : 0.015);

    // Divisors (valida que siguin dins del rang)
    m_divisors.clear();
    for (int d : plan.divisors)
        if (d > 0 && d < m_totalPoints-1) m_divisors.append(d);

    // Parades
    m_stops = plan.stops;
    m_stopTable->setRowCount(0);
    for (const StopPoint& sp : m_stops) {
        int row = m_stopTable->rowCount();
        m_stopTable->insertRow(row);
        m_stopTable->setItem(row,0,new QTableWidgetItem(QString::number(sp.trackPointIdx)));
        m_stopTable->setItem(row,1,new QTableWidgetItem(QString::number(sp.durationSec/60)));
        m_stopTable->setItem(row,2,new QTableWidgetItem(sp.description));
    }

    rebuildSegmentTable();

    // Aplica noms, potències i factor terreny des del pla
    for (int i=0; i<m_segTable->rowCount() && i<plan.segNames.size(); ++i) {
        if (!plan.segNames[i].isEmpty() && m_segTable->item(i,ColName))
            m_segTable->item(i,ColName)->setText(plan.segNames[i]);
        if (i < plan.segPowers.size() && m_segTable->item(i,ColPower))
            m_segTable->item(i,ColPower)->setText(
                QString::number(plan.segPowers[i],'f',0));
        if (i < plan.segTerrains.size() && m_segTable->item(i,ColTerrain))
            m_segTable->item(i,ColTerrain)->setText(
                QString::number(plan.segTerrains[i],'f',2));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SLOTS — ElevationChartView
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onDivisorMoved(int di, int newPt)
{
    if (di<0 || di>=m_divisors.size()) return;
    if (newPt<=0 || newPt>=m_totalPoints-1) return;
    const int GAP=5;
    int lo=(di>0)?m_divisors[di-1]+GAP:GAP;
    int hi=(di<m_divisors.size()-1)?m_divisors[di+1]-GAP:m_totalPoints-1-GAP;
    newPt=qBound(lo,newPt,hi);
    if (newPt==m_divisors[di]) return;
    m_divisors[di]=newPt;
    refreshSegmentStats();
    redrawDivisors();
    autoSaveTmp();
}

void MainWindow::onDivisorAdded(int pt)
{
    if (pt<=0||pt>=m_totalPoints-1) return;
    const int GAP=5;
    int ins=0;
    while (ins<m_divisors.size() && m_divisors[ins]<pt) ++ins;
    if (ins>0 && pt-m_divisors[ins-1]<GAP) return;
    if (ins<m_divisors.size() && m_divisors[ins]-pt<GAP) return;

    // Capturem potència i terreny de tots els segments actuals abans de la inserció.
    // rebuildSegmentTable reutilitza oldValues[i] per a la fila i, però en inserir
    // un nou segment a ins+1, les files ins+1..N queden desplaçades incorrectament.
    QVector<double> oldPowers, oldTerrains;
    for (int i=0; i<m_segTable->rowCount(); ++i) {
        oldPowers   << (m_segTable->item(i,ColPower)   ? m_segTable->item(i,ColPower)->text().toDouble()   : 150.0);
        oldTerrains << (m_segTable->item(i,ColTerrain) ? m_segTable->item(i,ColTerrain)->text().toDouble() : 1.0);
    }

    m_divisors.insert(ins,pt);
    rebuildSegmentTable();

    // Corregim les files afectades per la inserció:
    //   ins+1 (meitat dreta del tram dividit) → hereda els valors del tram original (ins)
    //   ins+2..N-1                            → cada una agafa els valors de la fila i-1
    for (int i=ins+1; i<m_segTable->rowCount(); ++i) {
        int src = (i == ins+1) ? ins : i-1;
        if (src < oldPowers.size()) {
            if (m_segTable->item(i,ColPower))
                m_segTable->item(i,ColPower)->setText(QString::number(oldPowers[src],'f',0));
            if (m_segTable->item(i,ColTerrain))
                m_segTable->item(i,ColTerrain)->setText(QString::number(oldTerrains[src],'f',2));
        }
    }

    redrawDivisors(); autoSaveTmp();
    setStatus(QString("Tram afegit. Ara hi ha %1 trams.").arg(m_divisors.size()+1));
}

void MainWindow::onDivisorRemoved(int di)
{
    if (di<0||di>=m_divisors.size()) return;

    // Capturem totes les potències i factors de terreny actuals.
    QVector<double> oldPowers, oldTerrains;
    for (int i=0; i<m_segTable->rowCount(); ++i) {
        oldPowers   << (m_segTable->item(i,ColPower)   ? m_segTable->item(i,ColPower)->text().toDouble()   : 150.0);
        oldTerrains << (m_segTable->item(i,ColTerrain) ? m_segTable->item(i,ColTerrain)->text().toDouble() : 1.0);
    }

    // Longituds dels dos trams que es fusionen (di i di+1).
    QVector<int> boundaries = {0};
    for (int d : m_divisors) boundaries.append(d);
    boundaries.append(m_totalPoints-1);

    double distA = m_cumDistKm[boundaries[di+1]] - m_cumDistKm[boundaries[di]];
    double distB = m_cumDistKm[boundaries[di+2]] - m_cumDistKm[boundaries[di+1]];
    double totalDist = distA + distB;

    double powerA   = oldPowers[di];
    double powerB   = oldPowers[di+1];
    double terrainA = oldTerrains[di];
    double terrainB = oldTerrains[di+1];

    // Terreny: mitjana ponderada per distància.
    double newTerrain = (totalDist > 0)
                      ? (terrainA * distA + terrainB * distB) / totalDist
                      : (terrainA + terrainB) / 2.0;

    // Potència: mitjana ponderada per temps estimat (si hi ha càlcul previ),
    // sinó per distància.
    double newPower;
    const auto& planSegs = m_planner.segments();
    if (planSegs.size() == m_divisors.size()+1
        && planSegs[di].estimatedTimeSec > 0
        && planSegs[di+1].estimatedTimeSec > 0)
    {
        double timeA  = planSegs[di].estimatedTimeSec;
        double timeB  = planSegs[di+1].estimatedTimeSec;
        double totalT = timeA + timeB;
        newPower = (powerA * timeA + powerB * timeB) / totalT;
    } else {
        newPower = (totalDist > 0)
                 ? (powerA * distA + powerB * distB) / totalDist
                 : (powerA + powerB) / 2.0;
    }

    m_divisors.remove(di);
    rebuildSegmentTable();

    // Corregim les files afectades:
    //   di        → valors fusionats calculats
    //   di+1..N-2 → cada fila i agafa els valors de l'antiga fila i+1
    //   (rebuildSegmentTable assigna oldValues[i] a la fila i, que no té en compte
    //    la fusió i deixa les files di+1 en endavant desplaçades una posició)
    if (m_segTable->item(di, ColPower))
        m_segTable->item(di, ColPower)->setText(QString::number(newPower, 'f', 0));
    if (m_segTable->item(di, ColTerrain))
        m_segTable->item(di, ColTerrain)->setText(QString::number(newTerrain, 'f', 2));
    for (int i=di+1; i<m_segTable->rowCount(); ++i) {
        int src = i+1;
        if (src < oldPowers.size()) {
            if (m_segTable->item(i, ColPower))
                m_segTable->item(i, ColPower)->setText(QString::number(oldPowers[src], 'f', 0));
            if (m_segTable->item(i, ColTerrain))
                m_segTable->item(i, ColTerrain)->setText(QString::number(oldTerrains[src], 'f', 2));
        }
    }

    redrawDivisors(); autoSaveTmp();
    setStatus(QString("Divisor eliminat. Ara hi ha %1 trams.").arg(m_divisors.size()+1));
}

void MainWindow::onStopAdded(int pt)
{
    if (pt<0||pt>=m_totalPoints) return;
    StopPoint sp; sp.trackPointIdx=pt; sp.durationSec=600; sp.description="Parada";
    int ins=0;
    while (ins<m_stops.size() && m_stops[ins].trackPointIdx<pt) ++ins;
    m_stops.insert(ins,sp);
    int row=m_stopTable->rowCount(); m_stopTable->insertRow(row);
    m_stopTable->setItem(row,0,new QTableWidgetItem(QString::number(pt)));
    m_stopTable->setItem(row,1,new QTableWidgetItem("10"));
    m_stopTable->setItem(row,2,new QTableWidgetItem("Parada"));
    redrawDivisors(); autoSaveTmp();
    setStatus(QString("Parada afegida al pk %1 km.")
              .arg(m_cumDistKm[qBound(0,pt,m_totalPoints-1)],0,'f',1));
}

void MainWindow::onStopRemoved(int si)
{
    if (si<0||si>=m_stops.size()) return;
    m_stops.remove(si);
    if (si<m_stopTable->rowCount()) m_stopTable->removeRow(si);
    redrawDivisors(); autoSaveTmp();
}

// ─────────────────────────────────────────────────────────────────────────────
//  SLOTS — Botons principals
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onBrowseInput()
{
    QString lastDir=m_settings.value("lastInputDir",QDir::homePath()).toString();
    QString path=QFileDialog::getOpenFileName(this,"Selecciona el track GPX",lastDir,
        "Fitxers GPX (*.gpx);;Tots els fitxers (*)");
    if (!path.isEmpty()) {
        m_inputPath->setText(path);
        if (m_outputPath->text().isEmpty()) {
            QFileInfo fi(path);
            m_outputPath->setText(fi.dir().filePath(fi.baseName()+"_planificat.gpx"));
        }
    }
}

void MainWindow::onBrowseOutput()
{
    QString lastDir=m_settings.value("lastOutputDir",QDir::homePath()).toString();
    QString path=QFileDialog::getSaveFileName(this,"Desar track calculat",lastDir,
        "Fitxers GPX (*.gpx);;Tots els fitxers (*)");
    if (!path.isEmpty()) {
        if (!path.endsWith(".gpx",Qt::CaseInsensitive)) path+=".gpx";
        m_outputPath->setText(path);
        m_settings.setValue("lastOutputDir",QFileInfo(path).absolutePath());
    }
}

void MainWindow::onLoadGPX()
{
    QString path=m_inputPath->text().trimmed();
    if (path.isEmpty()) { onBrowseInput(); path=m_inputPath->text().trimmed(); }
    if (path.isEmpty()) return;

    if (!m_planner.loadGPX(path)) {
        setStatus("Error carregant GPX: "+m_planner.lastError(), true); return;
    }
    QString err;
    m_loadedPoints=GPXParser::loadGPX(path,err);
    if (m_loadedPoints.isEmpty()) {
        setStatus("El fitxer no conté punts vàlids.",true); return;
    }
    m_totalPoints=m_loadedPoints.size();
    m_currentGpxPath=path;

    m_cumDistKm.resize(m_totalPoints);
    m_cumDistKm[0]=0.0;
    for (int i=1;i<m_totalPoints;++i)
        m_cumDistKm[i]=m_cumDistKm[i-1]+m_loadedPoints[i-1].distanceTo(m_loadedPoints[i])/1000.0;

    // Divisors inicials per defecte (3 trams)
    m_divisors.clear(); m_stops.clear(); m_stopTable->setRowCount(0);
    int step=m_totalPoints/3;
    if (step>0) { m_divisors.append(step); m_divisors.append(step*2); }

    m_settings.setValue("lastInputPath",path);
    m_settings.setValue("lastInputDir",QFileInfo(path).absolutePath());

    updateElevationChart(m_loadedPoints);
    rebuildSegmentTable();
    redrawDivisors();

    // Intenta carregar pla existent (temporal primer, després definitiu)
    if (PlanSerializer::hasPlan(path)) {
        PlanSerializer::Plan plan;
        if (PlanSerializer::load(path, plan)) {
            applyPlan(plan);
            redrawDivisors();
            bool fromTmp = PlanSerializer::hasUnsavedTmp(path);
            setStatus(QString("GPX + pla %1 carregats — %2 trams.")
                .arg(fromTmp ? "(temporal)" : "")
                .arg(m_divisors.size()+1));
            m_hasUnsavedChanges = fromTmp;
        }
    } else {
        m_hasUnsavedChanges = false;
        setStatus(QString("GPX carregat — %1 punts · %2 km  |  Clic dret al gràfic per gestionar trams.")
            .arg(m_totalPoints).arg(m_cumDistKm.last(),0,'f',1));
    }

    updateSummaryLabels();
    m_btnCompute->setEnabled(true);
    m_btnSavePlan->setEnabled(true);
    updateTitleBar();
}

void MainWindow::onCompute()
{
    if (m_loadedPoints.isEmpty()) { setStatus("Primer carrega un fitxer GPX.",true); return; }

    RiderProfile profile;
    profile.totalMassKg=m_mass->value(); profile.ftpWatts=m_ftp->value();
    profile.cda=m_cda->value();          profile.crr=m_crr->value();
    m_planner.setRiderProfile(profile);
    m_planner.setStartTime(m_startTime->dateTime());

    QVector<int> boundaries={0};
    for (int d:m_divisors) boundaries.append(d);
    boundaries.append(m_totalPoints-1);

    QVector<QPair<int,int>> ranges;
    for (int i=0;i<boundaries.size()-1;++i) ranges.append({boundaries[i],boundaries[i+1]});
    m_planner.defineSegments(ranges);

    auto& segs=m_planner.segments();
    for (int i=0;i<segs.size()&&i<m_segTable->rowCount();++i) {
        segs[i].label         = m_segTable->item(i,ColName)    ? m_segTable->item(i,ColName)->text()               : "";
        segs[i].targetPowerW  = m_segTable->item(i,ColPower)   ? m_segTable->item(i,ColPower)->text().toDouble()   : 150.0;
        segs[i].terrainFactor = m_segTable->item(i,ColTerrain) ? m_segTable->item(i,ColTerrain)->text().toDouble() : 1.0;
        segs[i].windSpeedMs   = 0.0;
    }

    m_planner.clearStops();
    for (int i=0;i<m_stopTable->rowCount();++i) {
        StopPoint sp;
        sp.trackPointIdx=m_stopTable->item(i,0)?m_stopTable->item(i,0)->text().toInt():0;
        sp.durationSec  =m_stopTable->item(i,1)?m_stopTable->item(i,1)->text().toInt()*60:0;
        sp.description  =m_stopTable->item(i,2)?m_stopTable->item(i,2)->text():"";
        m_planner.addStop(sp);
    }

    m_planner.compute();
    refreshSpeedTimeColumns();
    updateSummaryLabels();
    redrawDivisors();
    m_btnExport->setEnabled(true);
    autoSaveTmp();
    setStatus("Càlcul completat. Revisa el resum i exporta el GPX.");
    saveSettings();
}

void MainWindow::onExport()
{
    QString outPath=m_outputPath->text().trimmed();
    if (outPath.isEmpty()) { onBrowseOutput(); outPath=m_outputPath->text().trimmed(); }
    if (outPath.isEmpty()) return;
    if (!m_planner.exportGPX(outPath,QFileInfo(outPath).baseName())) {
        setStatus("Error exportant el GPX!",true);
        QMessageBox::critical(this,"Error","No s'ha pogut exportar:\n"+outPath); return;
    }
    m_settings.setValue("lastOutputDir",QFileInfo(outPath).absolutePath());
    setStatus("✓ GPX exportat: "+outPath);
    QMessageBox::information(this,"Exportació correcta",
        "Track exportat!\n\n"+outPath+
        "\n\nAra pots carregar-lo al Garmin i usar el Virtual Partner.");
}

void MainWindow::onSavePlan()
{
    if (m_currentGpxPath.isEmpty()) return;
    if (PlanSerializer::promoteTmpToFinal(m_currentGpxPath)) {
        m_hasUnsavedChanges = false;
        updateTitleBar();
        setStatus("✓ Pla desat: " + PlanSerializer::planPath(m_currentGpxPath));
    } else {
        // No hi havia temporal — desa directament
        PlanSerializer::save(m_currentGpxPath, currentPlan(), false);
        m_hasUnsavedChanges = false;
        updateTitleBar();
        setStatus("✓ Pla desat: " + PlanSerializer::planPath(m_currentGpxPath));
    }
}

void MainWindow::onRemoveStop()
{
    int row=m_stopTable->currentRow();
    if (row>=0) {
        if (row<m_stops.size()) m_stops.remove(row);
        m_stopTable->removeRow(row);
        redrawDivisors(); autoSaveTmp();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PERSISTÈNCIA (QSettings — preferències d'usuari)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::saveSettings()
{
    m_settings.setValue("lastInputPath",  m_inputPath->text());
    m_settings.setValue("lastOutputPath", m_outputPath->text());
    m_settings.setValue("mass", m_mass->value());
    m_settings.setValue("ftp",  m_ftp->value());
    m_settings.setValue("cda",  m_cda->value());
    m_settings.setValue("crr",  m_crr->value());
}

void MainWindow::loadSettings()
{
    QString li=m_settings.value("lastInputPath","").toString();
    QString lo=m_settings.value("lastOutputPath","").toString();
    if (!li.isEmpty()) m_inputPath->setText(li);
    if (!lo.isEmpty()) m_outputPath->setText(lo);
    m_mass->setValue(m_settings.value("mass",85.0).toDouble());
    m_ftp ->setValue(m_settings.value("ftp",200.0).toDouble());
    m_cda ->setValue(m_settings.value("cda",0.40).toDouble());
    m_crr ->setValue(m_settings.value("crr",0.015).toDouble());
    if (!li.isEmpty() && QFile::exists(li))
        setStatus(QString("Darrer track: %1  → Prem 'Carregar GPX' per continuar.")
                  .arg(QFileInfo(li).fileName()));
}

// ─────────────────────────────────────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setStatus(const QString& msg, bool error)
{
    m_statusBar->setText(msg);
    m_statusBar->setStyleSheet(
        error ? "padding:3px;background:#FFEBEE;color:#C62828;"
              : "padding:3px;background:#f0f0f0;color:#212121;");
}
