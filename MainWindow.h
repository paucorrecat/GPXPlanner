#pragma once
#include <QMainWindow>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QDoubleSpinBox>
#include "TrackPlanner.h"
#include "ElevationChartView.h"
#include "PlanSerializer.h"
#include "SrtmReader.h"

#include <QAreaSeries>
#include <QLineSeries>
#include <QValueAxis>
#include <QChart>

QT_BEGIN_NAMESPACE
class QAction;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLabel;
class QSplitter;
class QGroupBox;
class QProgressDialog;
QT_END_NAMESPACE

// ── Delegat SpinBox per a la columna Terreny ──────────────────────────────────
// Mostra un QDoubleSpinBox [0.05, 2.00] pas 0.05 quan l'usuari edita la cel·la.
class TerrainDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit TerrainDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem&,
                          const QModelIndex&) const override
    {
        auto* spin = new QDoubleSpinBox(parent);
        spin->setRange(0.05, 2.00);
        spin->setSingleStep(0.05);
        spin->setDecimals(2);
        spin->setFrame(false);
        // Forcem fons blanc i text negre per evitar que el color
        // de selecció de la taula tapi els botons de fletxa.
        spin->setStyleSheet(
            "QDoubleSpinBox {"
            "  background: white;"
            "  color: black;"
            "  selection-background-color: #cce8ff;"
            "  selection-color: black;"
            "}");
        return spin;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        double val = index.data(Qt::EditRole).toDouble();
        static_cast<QDoubleSpinBox*>(editor)->setValue(val);
    }

    void setModelData(QWidget* editor,
                      QAbstractItemModel* model,
                      const QModelIndex& index) const override
    {
        double val = static_cast<QDoubleSpinBox*>(editor)->value();
        model->setData(index, QString::number(val, 'f', 2), Qt::EditRole);
    }

    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&) const override
    {
        editor->setGeometry(option.rect);
    }
};

// ── MainWindow ────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;           // neteja temporal en tancar

private slots:
    void onBrowseInput();
    void onBrowseOutput();
    void onLoadGPX();
    void onFixElevation();
    void onCompute();
    void onExport();
    void onRemoveStop();
    void onSavePlan();                // desa pla definitiu

    void onOpenSettings();
    void onImportDemElevation();
    void onAddDivisorByCoords();

    void onDivisorMoved(int divisorIdx, int newPointIdx);
    void onDivisorAdded(int pointIdx);
    void onDivisorRemoved(int divisorIdx);
    void onStopAdded(int pointIdx);
    void onStopRemoved(int stopIdx);
    void onSegTableCellChanged(int row, int col);

private:
    // ── UI builders ──────────────────────────────────────────────────────────
    void buildUI();
    QWidget* buildFilePanel();
    QWidget* buildRiderPanel();
    QWidget* buildSegmentsPanel();
    QWidget* buildStopsPanel();
    QWidget* buildSummaryPanel();
    QWidget* buildElevationChart();

    // ── Persistència ─────────────────────────────────────────────────────────
    void saveSettings();
    void loadSettings();
    void autoSaveTmp();
    void applyPlan(const PlanSerializer::Plan& plan);
    PlanSerializer::Plan currentPlan() const;

    // ── Helpers ──────────────────────────────────────────────────────────────
    void rebuildSegmentTable();
    void refreshSegmentStats();
    void refreshSpeedTimeColumns();
    void redrawDivisors();
    void updateSummaryLabels();
    void updateSegCountDisplay();
    void updateElevationChart(const QVector<TrackPoint>& points,
                              QProgressDialog* prog = nullptr,
                              int progBase = 0, int progEnd = 0);
    void setStatus(const QString& msg, bool error = false);
    void updateTitleBar();

    // Columnes de la taula de trams
    enum SegCol {
        ColName=0,
        ColPkStart, ColPkEnd,
        ColDist, ColGrade, ColDplus, ColDminus, ColAltEnd,
        ColPower,
        ColTerrain,                   // factor terreny [0.05, 2.0]
        ColSpeed, ColTime, ColCumTime,
        ColCount
    };

    // ── QActions (menú + barra d'eines) ─────────────────────────────────────
    QAction*            m_actSavePlan        = nullptr;
    QAction*            m_actCompute         = nullptr;
    QAction*            m_actExport          = nullptr;
    QAction*            m_actFixElevation    = nullptr;
    QAction*            m_actImportDem       = nullptr;
    QAction*            m_actDivisorByCoords = nullptr;

    // ── Membres UI ───────────────────────────────────────────────────────────
    QLineEdit*          m_inputPath      = nullptr;
    QLineEdit*          m_outputPath     = nullptr;
    QPushButton*        m_btnLoad        = nullptr;
    QPushButton*        m_btnFixElevation = nullptr;
    QPushButton*        m_btnCompute     = nullptr;
    QPushButton*        m_btnExport      = nullptr;
    QPushButton*        m_btnSavePlan    = nullptr;
    QDateTimeEdit*      m_startTime      = nullptr;

    QDoubleSpinBox*     m_mass           = nullptr;
    QDoubleSpinBox*     m_ftp            = nullptr;
    QDoubleSpinBox*     m_cda            = nullptr;
    QDoubleSpinBox*     m_crr            = nullptr;

    QTableWidget*       m_segTable       = nullptr;
    QTableWidget*       m_stopTable      = nullptr;
    QLabel*             m_statusBar      = nullptr;

    // Gràfic
    ElevationChartView* m_chartView      = nullptr;
    QChart*             m_chart          = nullptr;
    QAreaSeries*        m_areaSeries     = nullptr;
    QLineSeries*        m_upperLine      = nullptr;
    QLineSeries*        m_lowerLine      = nullptr;
    QValueAxis*         m_axisX          = nullptr;
    QValueAxis*         m_axisY          = nullptr;

    // Labels resum
    QLabel*             m_lblTotalDist   = nullptr;
    QLabel*             m_lblTotalDplus  = nullptr;
    QLabel*             m_lblTotalDminus = nullptr;
    QLabel*             m_lblNSegs       = nullptr;
    QLabel*             m_lblTotalTime   = nullptr;
    QLabel*             m_lblArrival     = nullptr;
    QLabel*             m_lblAvgSpeed    = nullptr;

    // ── Model ────────────────────────────────────────────────────────────────
    QVector<int>        m_divisors;
    QVector<StopPoint>  m_stops;
    QVector<double>     m_cumDistKm;

    TrackPlanner        m_planner;
    SrtmReader          m_srtmReader;
    QSettings           m_settings;
    int                 m_totalPoints    = 0;
    QVector<TrackPoint> m_loadedPoints;
    QString             m_currentGpxPath;
    bool                m_hasUnsavedChanges = false;

    static const QList<QColor> k_segColors;

    // Format de temps h:min:s
    static QString formatTime(double totalSec) {
        int h   = static_cast<int>(totalSec) / 3600;
        int min = (static_cast<int>(totalSec) % 3600) / 60;
        int sec = static_cast<int>(totalSec) % 60;
        if (h > 0)
            return QString("%1h %2:%3")
                   .arg(h).arg(min,2,10,QChar('0')).arg(sec,2,10,QChar('0'));
        return QString("%1:%2")
               .arg(min).arg(sec,2,10,QChar('0'));
    }
};
