#include "MainWindow.h"
#include "SettingsDialog.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QProgressDialog>
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
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QDir>
#include <QCloseEvent>
#include <QScatterSeries>
#include <QLineSeries>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  PARSEIG DE COORDENADES
//  Suporta els formats més habituals de GPS i cartografia:
//    N41° 34.932 E001° 50.028    (graus + minuts decimals, hemisferi prefix)
//    41° 34.932' N, 1° 50.028' E (graus + minuts decimals, hemisferi sufix)
//    41° 34' 55.9" N 1° 50' 1.7" E (graus, minuts, segons, hemisferi sufix)
//    N41° 34' 55.9" E001° 50' 1.7"  (graus, minuts, segons, hemisferi prefix)
//    41.5822 1.8340              (graus decimals, separats per espai o coma)
//    41,5822 1,8340              (graus decimals, coma com decimal europeu)
// Retorna true si el text s'ha pogut parsejar.
// ─────────────────────────────────────────────────────────────────────────────
static bool parseLatLon(const QString& raw, double& outLat, double& outLon)
{
    // Normalitza: majúscules, ° ' " → espai
    QString s = raw.trimmed().toUpper();
    s.replace(QChar(0x00B0), ' ');  // °
    s.replace(QChar(0x2019), ' ');  // ' tipogràfic
    s.replace(QChar(0x201D), ' ');  // " tipogràfic
    s.replace('\'', ' ');
    s.replace('"', ' ');

    // Coma: si no hi ha punt, és decimal europeu (41,5822 → 41.5822);
    //       si ja hi ha punt, la coma és separadora de coordenades.
    if (!s.contains('.'))
        s.replace(',', '.');
    else
        s.replace(',', ' ');

    s = s.simplified();

    // Funció interna: aplica el signe de l'hemisferi
    auto applyHemi = [](double v, QChar h) {
        return (h == 'S' || h == 'W') ? -v : v;
    };

    // ── Format DMS prefix: [NS] DD MM SS.s [EW] DDD MM SS.s ─────────────────
    // Provat ABANS del DDM per evitar matches parcials (3 tokens vs 2).
    // Ex: "N41 34 55.9 E001 50 1.7"
    {
        static const QRegularExpression re(
            R"(([NS])\s*(\d+)\s+(\d+)\s+(\d+\.?\d*)\s+([EW])\s*(\d+)\s+(\d+)\s+(\d+\.?\d*))");
        auto m = re.match(s);
        if (m.hasMatch()) {
            outLat = applyHemi(m.captured(2).toDouble()
                             + m.captured(3).toDouble() / 60.0
                             + m.captured(4).toDouble() / 3600.0, m.captured(1)[0]);
            outLon = applyHemi(m.captured(6).toDouble()
                             + m.captured(7).toDouble() / 60.0
                             + m.captured(8).toDouble() / 3600.0, m.captured(5)[0]);
            return true;
        }
    }

    // ── Format DMS sufix: DD MM SS.s [NS] DDD MM SS.s [EW] ──────────────────
    // Ex: "41 34 55.9 N 1 50 1.7 E"
    {
        static const QRegularExpression re(
            R"((\d+)\s+(\d+)\s+(\d+\.?\d*)\s*([NS])\s+(\d+)\s+(\d+)\s+(\d+\.?\d*)\s*([EW]))");
        auto m = re.match(s);
        if (m.hasMatch()) {
            outLat = applyHemi(m.captured(1).toDouble()
                             + m.captured(2).toDouble() / 60.0
                             + m.captured(3).toDouble() / 3600.0, m.captured(4)[0]);
            outLon = applyHemi(m.captured(5).toDouble()
                             + m.captured(6).toDouble() / 60.0
                             + m.captured(7).toDouble() / 3600.0, m.captured(8)[0]);
            return true;
        }
    }

    // ── Format DDM prefix: [NS] DD MM.mmm [EW] DDD MM.mmm ───────────────────
    // Ex: "N41 34.932 E001 50.028"
    {
        static const QRegularExpression re(
            R"(([NS])\s*(\d+)\s+(\d+\.?\d*)\s+([EW])\s*(\d+)\s+(\d+\.?\d*))");
        auto m = re.match(s);
        if (m.hasMatch()) {
            outLat = applyHemi(m.captured(2).toDouble()
                             + m.captured(3).toDouble() / 60.0, m.captured(1)[0]);
            outLon = applyHemi(m.captured(5).toDouble()
                             + m.captured(6).toDouble() / 60.0, m.captured(4)[0]);
            return true;
        }
    }

    // ── Format DDM sufix: DD MM.mmm [NS] DDD MM.mmm [EW] ────────────────────
    // Ex: "41 34.932 N 1 50.028 E"
    {
        static const QRegularExpression re(
            R"((\d+)\s+(\d+\.?\d*)\s*([NS])\s+(\d+)\s+(\d+\.?\d*)\s*([EW]))");
        auto m = re.match(s);
        if (m.hasMatch()) {
            outLat = applyHemi(m.captured(1).toDouble()
                             + m.captured(2).toDouble() / 60.0, m.captured(3)[0]);
            outLon = applyHemi(m.captured(4).toDouble()
                             + m.captured(5).toDouble() / 60.0, m.captured(6)[0]);
            return true;
        }
    }

    // ── Format graus decimals: DD.ddddd DD.ddddd ─────────────────────────────
    // Ex: "41.5822 1.8340"  o  "41,5822 1,8340" (ja normalitzat amb punt)
    {
        static const QRegularExpression re(
            R"(^([+-]?\d+\.?\d*)\s+([+-]?\d+\.?\d*)$)");
        auto m = re.match(s);
        if (m.hasMatch()) {
            bool ok1, ok2;
            const double lat = m.captured(1).toDouble(&ok1);
            const double lon = m.captured(2).toDouble(&ok2);
            if (ok1 && ok2 && qAbs(lat) <= 90.0 && qAbs(lon) <= 180.0) {
                outLat = lat;
                outLon = lon;
                return true;
            }
        }
    }

    return false;
}

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
    // ── Menú ─────────────────────────────────────────────────────────────────
    auto* mb = menuBar();

    // Menú "Fitxer"
    auto* menuFitxer = mb->addMenu("&Fitxer");

    auto* actLoad = menuFitxer->addAction("Carregar GPX…");
    actLoad->setShortcut(QKeySequence("Ctrl+O"));
    connect(actLoad, &QAction::triggered, this, &MainWindow::onLoadGPX);

    m_actSavePlan = menuFitxer->addAction("Desar pla");
    m_actSavePlan->setShortcut(QKeySequence("Ctrl+S"));
    m_actSavePlan->setEnabled(false);
    connect(m_actSavePlan, &QAction::triggered, this, &MainWindow::onSavePlan);

    menuFitxer->addSeparator();

    m_actExport = menuFitxer->addAction("Exportar GPX…");
    m_actExport->setShortcut(QKeySequence("Ctrl+E"));
    m_actExport->setEnabled(false);
    connect(m_actExport, &QAction::triggered, this, &MainWindow::onExport);

    menuFitxer->addSeparator();

    auto* actQuit = menuFitxer->addAction("Sortir");
    actQuit->setShortcut(QKeySequence("Ctrl+Q"));
    connect(actQuit, &QAction::triggered, qApp, &QApplication::quit);

    // Menú "Eines"
    auto* menuEines = mb->addMenu("&Eines");

    m_actCompute = menuEines->addAction("Calcular");
    m_actCompute->setShortcut(QKeySequence("F5"));
    m_actCompute->setEnabled(false);
    connect(m_actCompute, &QAction::triggered, this, &MainWindow::onCompute);

    m_actDivisorByCoords = menuEines->addAction("Divisor per coordenades…");
    m_actDivisorByCoords->setShortcut(QKeySequence("Ctrl+D"));
    m_actDivisorByCoords->setEnabled(false);
    m_actDivisorByCoords->setToolTip(
        "Afegeix un divisor de tram al punt del track més proper a les coordenades introduïdes.");
    connect(m_actDivisorByCoords, &QAction::triggered, this, &MainWindow::onAddDivisorByCoords);

    m_actFixElevation = menuEines->addAction("Corregir elevació");
    m_actFixElevation->setShortcut(QKeySequence("F6"));
    m_actFixElevation->setEnabled(false);
    m_actFixElevation->setToolTip(
        "Interpola linealment l'elevació dels punts que no tenien element <ele> al GPX original.");
    connect(m_actFixElevation, &QAction::triggered, this, &MainWindow::onFixElevation);

    m_actImportDem = menuEines->addAction("Importar altures DEM");
    m_actImportDem->setShortcut(QKeySequence("F7"));
    m_actImportDem->setEnabled(false);
    m_actImportDem->setToolTip(
        "Substitueix les altures del track amb dades SRTM1 (.hgt) de la carpeta DEM configurada.");
    connect(m_actImportDem, &QAction::triggered, this, &MainWindow::onImportDemElevation);

    menuEines->addSeparator();

    auto* actSettings = menuEines->addAction("Configuració…");
    actSettings->setShortcut(QKeySequence("Ctrl+,"));
    connect(actSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);

    // Menú "Ajuda"
    auto* menuAjuda = mb->addMenu("A&juda");

    auto* actAbout = menuAjuda->addAction("Sobre GPXPlanner…");
    connect(actAbout, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "Sobre GPXPlanner",
            "<b>GPXPlanner</b><br>"
            "Generador de Virtual Partner per a Garmin.<br><br>"
            "Planificador de tracks ciclistes GPX amb càlcul de timestamps<br>"
            "basat en potència, pendent i perfil del ciclista.");
    });

    // ── Barra d'eines ─────────────────────────────────────────────────────────
    auto* toolBar = addToolBar("Barra d'eines");
    toolBar->setMovable(false);
    toolBar->addAction(actLoad);
    toolBar->addAction(m_actSavePlan);
    toolBar->addSeparator();
    toolBar->addAction(m_actCompute);
    toolBar->addAction(m_actDivisorByCoords);
    toolBar->addAction(m_actImportDem);
    toolBar->addAction(m_actExport);
    toolBar->addAction(m_actFixElevation);
    toolBar->addSeparator();
    toolBar->addAction(actSettings);

    // ── Widget central ────────────────────────────────────────────────────────
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setSpacing(6);
    root->setContentsMargins(8,8,8,8);

    root->addWidget(buildFilePanel());

    auto* vSplit = new QSplitter(Qt::Vertical);
    vSplit->setChildrenCollapsible(false);

    auto* midSplit = new QSplitter(Qt::Horizontal);
    midSplit->addWidget(buildRiderPanel());
    midSplit->addWidget(buildSummaryPanel());
    midSplit->setStretchFactor(0,1);
    midSplit->setStretchFactor(1,2);
    vSplit->addWidget(midSplit);

    vSplit->addWidget(buildElevationChart());

    auto* botSplit = new QSplitter(Qt::Vertical);
    botSplit->addWidget(buildSegmentsPanel());
    botSplit->addWidget(buildStopsPanel());
    botSplit->setStretchFactor(0,4);
    botSplit->setStretchFactor(1,1);
    vSplit->addWidget(botSplit);

    vSplit->setSizes({160, 220, 320});
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

    // Fila 2: botons d'acció
    m_btnFixElevation = new QPushButton("🔧  Corregir elevació");
    m_btnFixElevation->setEnabled(false);
    m_btnFixElevation->setStyleSheet(
        "font-weight:bold; background:#9C27B0; color:white; padding:4px 10px;");
    m_btnFixElevation->setToolTip(
        "Interpola linealment l'elevació dels punts que no tenien element <ele> al GPX original.");
    connect(m_btnFixElevation, &QPushButton::clicked, this, &MainWindow::onFixElevation);
    g->addWidget(m_btnFixElevation, 2, 1);

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
    connect(m_segTable, &QTableWidget::cellChanged, this, &MainWindow::onSegTableCellChanged);
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
    g->setVerticalSpacing(6);
    g->setHorizontalSpacing(8);

    auto mkVal = []() {
        auto* l = new QLabel("—");
        l->setStyleSheet(
            "background:#f9f9f9; border:1px solid #ddd; border-radius:3px;"
            "padding:2px 6px; font-weight:bold;");
        return l;
    };

    // Fila 0: Trams | Distància total
    g->addWidget(new QLabel("Trams:"),           0, 0);
    m_lblNSegs = mkVal();     g->addWidget(m_lblNSegs,     0, 1);
    g->addWidget(new QLabel("Distància total:"), 0, 2);
    m_lblTotalDist = mkVal(); g->addWidget(m_lblTotalDist, 0, 3);

    // Fila 1: D+ | D-
    g->addWidget(new QLabel("D+ total:"),        1, 0);
    m_lblTotalDplus  = mkVal(); g->addWidget(m_lblTotalDplus,  1, 1);
    g->addWidget(new QLabel("D− total:"),        1, 2);
    m_lblTotalDminus = mkVal(); g->addWidget(m_lblTotalDminus, 1, 3);

    // Fila 2: Hora sortida (editable) | Hora d'arribada
    g->addWidget(new QLabel("Hora de sortida:"), 2, 0);
    m_startTime = new QDateTimeEdit(QDateTime::currentDateTime());
    m_startTime->setDisplayFormat("dd/MM/yyyy  HH:mm");
    m_startTime->setCalendarPopup(true);
    g->addWidget(m_startTime, 2, 1);
    g->addWidget(new QLabel("Hora d'arribada:"), 2, 2);
    m_lblArrival = mkVal(); g->addWidget(m_lblArrival, 2, 3);

    // Fila 3: Temps total | Velocitat mitjana
    g->addWidget(new QLabel("Temps total:"),     3, 0);
    m_lblTotalTime = mkVal(); g->addWidget(m_lblTotalTime, 3, 1);
    g->addWidget(new QLabel("Velocitat mitja:"), 3, 2);
    m_lblAvgSpeed  = mkVal(); g->addWidget(m_lblAvgSpeed,  3, 3);

    g->setRowStretch(4, 1);
    g->setColumnStretch(1, 1);
    g->setColumnStretch(3, 1);
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
    connect(m_chartView, &ElevationChartView::zoomReset,      this, &MainWindow::redrawDivisors);

    v->addWidget(m_chartView);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ELEVATION CHART UPDATE
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updateElevationChart(const QVector<TrackPoint>& points,
                                      QProgressDialog* prog, int progBase, int progEnd)
{
    if (points.isEmpty()) return;

    const int n = points.size();
    QVector<QPointF> upper, lower;
    upper.reserve(n); lower.reserve(n);

    double minElev = points[0].elevM, maxElev = points[0].elevM;
    const int reportStep = qMax(1, n / (progEnd - progBase > 0 ? (progEnd - progBase) : 1));

    for (int i = 0; i < n; ++i) {
        double km = m_cumDistKm[i], e = points[i].elevM;
        upper.append({km, e});
        lower.append({km, 0.0});
        minElev = qMin(minElev, e); maxElev = qMax(maxElev, e);

        if (prog && (i % reportStep == 0)) {
            prog->setValue(progBase + (i * (progEnd - progBase)) / n);
            QApplication::processEvents();
        }
    }

    // replace() actualitza la sèrie en un sol cop (molt més ràpid que append per append)
    m_upperLine->replace(upper);
    m_lowerLine->replace(lower);

    double margin = qMax((maxElev - minElev) * 0.15, 10.0);
    double xMax   = m_cumDistKm.last();
    m_axisX->setRange(0, xMax);
    m_axisY->setRange(qMax(0.0, minElev - margin), maxElev + margin);
    m_chartView->resetZoomRange(0, xMax);
    m_chartView->resetYRange(qMax(0.0, minElev - margin), maxElev + margin);
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

    m_segTable->blockSignals(true);

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
        // Pk fi: editable per a tots els trams excepte l'últim (el seu final és fix)
        if (i < nSeg-1)
            rw(ColPkEnd, QString::number(m_cumDistKm[qMin(s1,m_totalPoints-1)],'f',2));
        else
            ro(ColPkEnd, QString::number(m_cumDistKm[qMin(s1,m_totalPoints-1)],'f',2));

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

    m_segTable->blockSignals(false);
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
    m_actSavePlan->setEnabled(true);
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
void MainWindow::onSegTableCellChanged(int row, int col)
{
    if (col != ColPkEnd) return;
    if (m_cumDistKm.isEmpty()) return;
    // Només trams que tenen divisor (no l'últim)
    if (row >= m_divisors.size()) return;

    auto* it = m_segTable->item(row, col);
    if (!it) return;

    bool ok;
    double km = it->text().replace(',','.').toDouble(&ok);

    QVector<int> boundaries = {0};
    for (int d : m_divisors) boundaries.append(d);
    boundaries.append(m_totalPoints-1);

    // Rang factible: estrictament entre inici del tram actual i inici del tram següent al següent
    double minKm = m_cumDistKm[boundaries[row]];
    double maxKm = m_cumDistKm[boundaries[row+2]];

    if (!ok || km <= minKm || km >= maxKm) {
        // Valor no factible: restaura el valor original i avisa
        m_segTable->blockSignals(true);
        it->setText(QString::number(m_cumDistKm[m_divisors[row]],'f',2));
        m_segTable->blockSignals(false);
        setStatus(QString("Pk fi fora de rang: ha d'estar entre %1 i %2 km.")
                  .arg(minKm,0,'f',2).arg(maxKm,0,'f',2), true);
        return;
    }

    // Troba el punt GPX més proper al km introduït
    // Binary search sobre m_cumDistKm
    auto it2 = std::lower_bound(m_cumDistKm.begin(), m_cumDistKm.end(), km);
    int idx = static_cast<int>(it2 - m_cumDistKm.begin());
    if (idx > 0 && idx < m_cumDistKm.size()) {
        // Tria el veí més proper
        if (std::abs(m_cumDistKm[idx-1] - km) < std::abs(m_cumDistKm[idx] - km))
            --idx;
    }
    idx = qBound(boundaries[row]+1, idx, boundaries[row+2]-1);

    if (idx == m_divisors[row]) {
        // Cap canvi real; restaura text amb el valor exacte
        m_segTable->blockSignals(true);
        it->setText(QString::number(m_cumDistKm[idx],'f',2));
        m_segTable->blockSignals(false);
        return;
    }

    m_divisors[row] = idx;
    rebuildSegmentTable();
    redrawDivisors();
    autoSaveTmp();
}

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

    QProgressDialog prog("Carregant fitxer GPX…", QString(), 0, 7, this);
    prog.setWindowTitle("Carregant…");
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);
    prog.setValue(0);
    QApplication::processEvents();

    // Pas 1: llegir punts GPX
    prog.setLabelText("Llegint punts GPX…");
    prog.setValue(1); QApplication::processEvents();
    QString err;
    m_loadedPoints = GPXParser::loadGPX(path, err);
    if (m_loadedPoints.isEmpty()) {
        QString msg = err.isEmpty() ? "El fitxer no conté punts vàlids." : err;
        setStatus(msg, true);
        QMessageBox::warning(this, "Error llegint GPX", msg);
        return;
    }

    // Pas 2: carregar al planificador
    prog.setLabelText("Inicialitzant planificador…");
    prog.setValue(2); QApplication::processEvents();
    if (!m_planner.loadGPX(path)) {
        setStatus("Error carregant GPX: "+m_planner.lastError(), true); return;
    }

    m_totalPoints    = m_loadedPoints.size();
    m_currentGpxPath = path;

    // Pas 3: calcular distàncies acumulades
    prog.setLabelText("Calculant distàncies…");
    prog.setValue(3); QApplication::processEvents();
    m_cumDistKm.resize(m_totalPoints);
    m_cumDistKm[0] = 0.0;
    for (int i=1; i<m_totalPoints; ++i)
        m_cumDistKm[i] = m_cumDistKm[i-1]
                       + m_loadedPoints[i-1].distanceTo(m_loadedPoints[i]) / 1000.0;

    // Divisors inicials per defecte (3 trams)
    m_divisors.clear(); m_stops.clear(); m_stopTable->setRowCount(0);
    int step = m_totalPoints / 3;
    if (step > 0) { m_divisors.append(step); m_divisors.append(step*2); }

    m_settings.setValue("lastInputPath", path);
    m_settings.setValue("lastInputDir",  QFileInfo(path).absolutePath());

    // Proposa nom de sortida basat en el fitxer d'entrada
    QFileInfo fi(path);
    m_outputPath->setText(fi.dir().filePath(fi.baseName()+"_planificat.gpx"));

    // Pas 4-6: gràfic d'elevació (ocupa la majoria del temps, rep el rang complet)
    prog.setLabelText("Construint gràfic d'elevació…");
    prog.setValue(4); QApplication::processEvents();
    updateElevationChart(m_loadedPoints, &prog, 4, 6);

    // Pas 6: taula de trams
    prog.setLabelText("Construint taula de trams…");
    prog.setValue(6); QApplication::processEvents();
    rebuildSegmentTable();
    redrawDivisors();

    // Pas 7: pla existent
    prog.setLabelText("Comprovant pla existent…");
    prog.setValue(7); QApplication::processEvents();
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
    m_actCompute->setEnabled(true);
    m_actSavePlan->setEnabled(true);
    m_actImportDem->setEnabled(true);
    m_actDivisorByCoords->setEnabled(true);

    // Activa el botó de correcció d'elevació si hi ha punts sense <ele>
    bool anyMissing = std::any_of(m_loadedPoints.begin(), m_loadedPoints.end(),
                                  [](const TrackPoint& p){ return !p.hasEle; });
    m_btnFixElevation->setEnabled(anyMissing);
    m_actFixElevation->setEnabled(anyMissing);

    updateTitleBar();
}

void MainWindow::onFixElevation()
{
    if (m_loadedPoints.isEmpty()) return;

    const int n = m_loadedPoints.size();

    // Marca quins punts eren originalment invàlids (sense <ele>)
    QVector<bool> missing(n);
    int totalMissing = 0;
    for (int i = 0; i < n; ++i) {
        missing[i] = !m_loadedPoints[i].hasEle;
        if (missing[i]) ++totalMissing;
    }
    if (totalMissing == 0) {
        setStatus("Cap punt sense elevació trobat.");
        m_btnFixElevation->setEnabled(false);
        m_actFixElevation->setEnabled(false);
        return;
    }

    for (int i = 0; i < n; ++i) {
        if (!missing[i]) continue;

        // Veí vàlid anterior (original)
        int prev = -1;
        for (int j = i - 1; j >= 0; --j)
            if (!missing[j]) { prev = j; break; }

        // Veí vàlid posterior (original)
        int next = -1;
        for (int j = i + 1; j < n; ++j)
            if (!missing[j]) { next = j; break; }

        double elevInterp;
        if (prev < 0 && next < 0) {
            continue;  // no hi ha cap punt vàlid — cas extrem, deixem 0
        } else if (prev < 0) {
            elevInterp = m_loadedPoints[next].elevM;
        } else if (next < 0) {
            elevInterp = m_loadedPoints[prev].elevM;
        } else {
            double total = m_cumDistKm[next] - m_cumDistKm[prev];
            if (total > 0.0) {
                double dPrev = m_cumDistKm[i] - m_cumDistKm[prev];
                double dNext = m_cumDistKm[next] - m_cumDistKm[i];
                elevInterp = m_loadedPoints[prev].elevM * (dNext / total)
                           + m_loadedPoints[next].elevM * (dPrev / total);
            } else {
                elevInterp = (m_loadedPoints[prev].elevM + m_loadedPoints[next].elevM) / 2.0;
            }
        }

        m_loadedPoints[i].elevM  = elevInterp;
        m_loadedPoints[i].hasEle = true;
    }

    // Sincronitza els punts corregits amb el planificador
    m_planner.setPoints(m_loadedPoints);

    // Refresca gràfic, taula de trams, resum i barra d'estat
    updateElevationChart(m_loadedPoints);
    rebuildSegmentTable();
    redrawDivisors();
    updateSummaryLabels();
    setStatus(QString("Elevació corregida: %1 punts interpolats.").arg(totalMissing));
    m_btnFixElevation->setEnabled(false);
    m_actFixElevation->setEnabled(false);
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
    m_actExport->setEnabled(true);
    m_actImportDem->setEnabled(true);
    m_actDivisorByCoords->setEnabled(true);
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
    // demFolder es gestiona des del diàleg de configuració; assegurem el valor per defecte
    if (!m_settings.contains("demFolder"))
        m_settings.setValue("demFolder", QDir::homePath() + "/DEM");
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
    // Inicialitza demFolder amb el valor per defecte si no s'ha configurat mai
    if (!m_settings.contains("demFolder"))
        m_settings.setValue("demFolder", QDir::homePath() + "/DEM");
    m_srtmReader.setFolder(m_settings.value("demFolder").toString());
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

// ─────────────────────────────────────────────────────────────────────────────
//  SLOTS — Menú / Barra d'eines
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onAddDivisorByCoords()
{
    if (m_loadedPoints.isEmpty()) return;

    // Demana les coordenades a l'usuari
    bool ok;
    const QString text = QInputDialog::getText(
        this,
        "Divisor per coordenades",
        "Introdueix les coordenades del punt de divisió:\n\n"
        "Formats acceptats:\n"
        "  N41° 34.932 E001° 50.028\n"
        "  41° 34.932' N, 1° 50.028' E\n"
        "  41° 34' 55.9\" N  1° 50' 1.7\" E\n"
        "  41.5822 1.8340",
        QLineEdit::Normal, QString(), &ok);

    if (!ok || text.trimmed().isEmpty()) return;

    double lat, lon;
    if (!parseLatLon(text, lat, lon)) {
        QMessageBox::warning(this, "Format no reconegut",
            "No s'han pogut interpretar les coordenades introduïdes.\n\n"
            "Comprova el format i torna-ho a provar.");
        return;
    }

    // Cerca el punt del track més proper per distància haversine
    TrackPoint ref;
    ref.lat = lat;
    ref.lon = lon;

    int    bestIdx  = 0;
    double bestDist = m_loadedPoints[0].distanceTo(ref);
    for (int i = 1; i < m_loadedPoints.size(); ++i) {
        const double d = m_loadedPoints[i].distanceTo(ref);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }

    // Delega a onDivisorAdded, que ja gestiona GAP, potències, etc.
    onDivisorAdded(bestIdx);

    // onDivisorAdded pot rebutjar el punt si és massa proper a un divisor existent;
    // comprovem si realment s'ha afegit per informar l'usuari.
    if (m_divisors.contains(bestIdx)) {
        setStatus(QString("Divisor afegit al punt #%1 · pk %2 km · a %3 m de les coordenades.")
            .arg(bestIdx)
            .arg(m_cumDistKm[bestIdx], 0, 'f', 2)
            .arg(bestDist, 0, 'f', 0));
    } else {
        setStatus(QString("No s'ha pogut afegir el divisor al pk %1 km "
                          "(massa proper a un divisor existent).")
            .arg(m_cumDistKm[qBound(0, bestIdx, m_totalPoints-1)], 0, 'f', 2),
                  /*error=*/true);
    }
}

void MainWindow::onImportDemElevation()
{
    if (m_loadedPoints.isEmpty()) return;

    // 1. Comprova quins fitxers .hgt cal i quins falten
    const SrtmReader::HgtNeeds needs = m_srtmReader.checkNeeds(m_loadedPoints);

    if (!needs.missing.isEmpty()) {
        QString llista = needs.missing.join("\n  • ");
        QMessageBox::warning(this, "Fitxers DEM no trobats",
            QString("Falten els fitxers SRTM següents a la carpeta DEM configurada:\n\n"
                    "  • %1\n\n"
                    "Descarrega'ls de https://earthexplorer.usgs.gov o\n"
                    "https://opentopography.org i posa'ls a la carpeta DEM configurada.")
            .arg(llista));
        return;
    }

    // 2. Demana confirmació
    const int n = m_loadedPoints.size();
    const auto resp = QMessageBox::question(this, "Importar altures DEM",
        QString("Vols substituir les altures de tots els %1 punts del track\n"
                "amb les dades SRTM? Aquesta acció no es pot desfer.")
        .arg(n),
        "Sí, importar", "Cancel·la");
    if (resp != 0) return;   // 0 = primer botó ("Sí, importar")

    // 3. Assigna les elevacions DEM
    for (TrackPoint& p : m_loadedPoints)
        p.elevM = m_srtmReader.elevationAt(p.lat, p.lon);

    // Sincronitza els punts corregits amb el planificador
    m_planner.setPoints(m_loadedPoints);

    // 4. Refresca UI
    updateElevationChart(m_loadedPoints);
    refreshSegmentStats();
    autoSaveTmp();
    setStatus(QString("✓ Altures DEM importades — %1 punts actualitzats.").arg(n));
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(this);

    // Pre-omple amb els valors actuals
    dlg.setMassKg(m_mass->value());
    dlg.setFtpW(m_ftp->value());
    dlg.setCda(m_cda->value());
    dlg.setCrr(m_crr->value());
    dlg.setDemFolder(m_settings.value("demFolder",
                                      QDir::homePath() + "/DEM").toString());

    if (dlg.exec() != QDialog::Accepted) return;

    // Aplica els valors als spinboxes de la finestra principal
    m_mass->setValue(dlg.massKg());
    m_ftp ->setValue(dlg.ftpW());
    m_cda ->setValue(dlg.cda());
    m_crr ->setValue(dlg.crr());

    // Desa la carpeta DEM a QSettings i actualitza el lector SRTM
    m_settings.setValue("demFolder", dlg.demFolder());
    m_srtmReader.setFolder(dlg.demFolder());

    // Si hi ha un GPX carregat, actualitza el pla temporal
    if (!m_currentGpxPath.isEmpty())
        autoSaveTmp();
}
