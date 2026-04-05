#pragma once
#include <QChartView>
#include <QChart>
#include <QValueAxis>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QVector>
#include <QCursor>
#include <QtMath>

/**
 * ElevationChartView.h
 *
 *  - Drag divisor (botó esquerre sobre barra ±10px)
 *  - Pan horitzontal (drag espai buit)
 *  - Ctrl+Scroll → zoom horitzontal centrat al cursor
 *  - Drag vertical sobre eix Y → zoom vertical (amunt=in, avall=out)
 *  - Doble clic  → reset zoom X i Y
 *  - Clic dret   → menú contextual unificat (tram / parada / eliminar)
 */
class ElevationChartView : public QChartView
{
    Q_OBJECT

public:
    explicit ElevationChartView(QChart* chart, QWidget* parent = nullptr)
        : QChartView(chart, parent)
    {
        setMouseTracking(true);
        setRenderHint(QPainter::Antialiasing);
        setDragMode(QGraphicsView::NoDrag);
    }

    void setAxes(QValueAxis* axisX, QValueAxis* axisY)
    {
        m_axisX = axisX;
        m_axisY = axisY;
        if (m_axisX) { m_fullXMin = m_axisX->min(); m_fullXMax = m_axisX->max(); }
    }

    void setDivisors(const QVector<int>& pts, int total, const QVector<double>& cum)
    {
        m_divisors = pts; m_totalPoints = total; m_cumDistKm = cum;
    }

    void setStops(const QVector<int>& stopPts) { m_stopIndices = stopPts; }

    void resetZoomRange(double xMin, double xMax)
    {
        m_fullXMin = xMin; m_fullXMax = xMax;
    }

    void resetYRange(double yMin, double yMax)
    {
        m_fullYMin = yMin; m_fullYMax = yMax;
    }

signals:
    void divisorMoved(int divisorIdx, int newPointIdx);
    void divisorAdded(int pointIdx);
    void divisorRemoved(int divisorIdx);
    void stopAdded(int pointIdx);
    void stopRemoved(int stopIdx);
    void zoomReset();

protected:

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            // Zoom vertical: drag sobre la zona de l'eix Y
            if (isOnYAxis(event->pos())) {
                m_yZoomActive = true;
                m_yZoomLastY  = event->pos().y();
                setCursor(Qt::SizeVerCursor);
                event->accept(); return;
            }
            int di = divisorAt(event->pos());
            if (di >= 0) {
                m_dragDivisorIdx = di; m_draggingDivisor = true;
                setCursor(Qt::SizeHorCursor); event->accept(); return;
            }
            m_panActive = true;
            m_panLastX = event->pos().x();
            m_panLastY = event->pos().y();
            setCursor(Qt::OpenHandCursor); event->accept(); return;
        }
        QChartView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_yZoomActive && m_axisY) {
            int dy = event->pos().y() - m_yZoomLastY;
            m_yZoomLastY = event->pos().y();
            if (dy) applyYZoom(dy);
            event->accept(); return;
        }
        if (m_draggingDivisor && m_dragDivisorIdx >= 0) {
            int pt = pixelToPointIndex(event->pos().x());
            if (pt >= 0) emit divisorMoved(m_dragDivisorIdx, pt);
            event->accept(); return;
        }
        if (m_panActive) {
            int dx = event->pos().x() - m_panLastX;
            int dy = event->pos().y() - m_panLastY;
            m_panLastX = event->pos().x();
            m_panLastY = event->pos().y();
            applyPan(-dx, dy);
            event->accept(); return;
        }
        // Cursors hover
        if (isOnYAxis(event->pos())) {
            setCursor(Qt::SizeVerCursor);
        } else {
            int di = divisorAt(event->pos());
            if (di >= 0) setCursor(Qt::SizeHorCursor);
            else if (stopAt(event->pos()) >= 0) setCursor(Qt::PointingHandCursor);
            else setCursor(Qt::ArrowCursor);
        }
        QChartView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (m_yZoomActive) {
            m_yZoomActive = false;
            setCursor(Qt::ArrowCursor);
            event->accept(); return;
        }
        if (m_draggingDivisor) {
            m_draggingDivisor = false; m_dragDivisorIdx = -1;
            setCursor(Qt::ArrowCursor); event->accept(); return;
        }
        if (m_panActive) {
            m_panActive = false; setCursor(Qt::ArrowCursor);
            event->accept(); return;
        }
        QChartView::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            if (m_axisX) m_axisX->setRange(m_fullXMin, m_fullXMax);
            if (m_axisY) m_axisY->setRange(m_fullYMin, m_fullYMax);
            emit zoomReset();
            event->accept(); return;
        }
        QChartView::mouseDoubleClickEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (!(event->modifiers() & Qt::ControlModifier) || !m_axisX) {
            event->ignore(); return;
        }
        double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;
        QPointF scenePos = mapToScene(event->position().toPoint());
        double  cursorKm = chart()->mapToValue(scenePos).x();
        double  lo = m_axisX->min(), hi = m_axisX->max();
        double  range    = hi - lo;
        double  newRange = qBound(0.5, range * factor, m_fullXMax - m_fullXMin);
        double  ratio    = (cursorKm - lo) / range;
        double  newLo    = cursorKm - ratio * newRange;
        double  newHi    = newLo + newRange;
        if (newLo < m_fullXMin) { newLo = m_fullXMin; newHi = newLo + newRange; }
        if (newHi > m_fullXMax) { newHi = m_fullXMax; newLo = newHi - newRange; }
        m_axisX->setRange(qMax(newLo, m_fullXMin), qMin(newHi, m_fullXMax));
        event->accept();
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        QMenu menu(this);
        int di = divisorAt(event->pos());
        int si = stopAt(event->pos());

        if (di >= 0) {
            auto* act = menu.addAction(
                QString("✕  Eliminar divisor de tram  (Tram %1 | Tram %2)")
                .arg(di+1).arg(di+2));
            connect(act, &QAction::triggered, this, [this,di]{ emit divisorRemoved(di); });
        } else if (si >= 0) {
            auto* act = menu.addAction("✕  Eliminar parada");
            connect(act, &QAction::triggered, this, [this,si]{ emit stopRemoved(si); });
        } else {
            int pt = pixelToPointIndex(event->pos().x());
            if (pt > 0 && pt < m_totalPoints - 1) {
                double km = (pt < m_cumDistKm.size()) ? m_cumDistKm[pt] : -1.0;
                QString pk = km >= 0 ? QString("  (pk %1 km)").arg(km,0,'f',1) : "";
                auto* a1 = menu.addAction(QString("＋  Afegir canvi de tram aquí%1").arg(pk));
                connect(a1, &QAction::triggered, this, [this,pt]{ emit divisorAdded(pt); });
                auto* a2 = menu.addAction(QString("⏸  Afegir parada aquí%1").arg(pk));
                connect(a2, &QAction::triggered, this, [this,pt]{ emit stopAdded(pt); });
            }
        }
        if (!menu.isEmpty()) menu.exec(event->globalPos());
    }

private:
    bool isOnYAxis(const QPoint& pos) const
    {
        if (!m_axisY) return false;
        QRectF plot = chart()->plotArea();
        return pos.x() < plot.left()
            && pos.y() >= plot.top()
            && pos.y() <= plot.bottom();
    }

    void applyYZoom(int dy)
    {
        if (!m_axisY) return;
        double factor   = qBound(0.2, 1.0 + dy * 0.005, 5.0);
        double lo       = m_axisY->min(), hi = m_axisY->max();
        double mid      = (lo + hi) * 0.5;
        double newRange = qMax(10.0, (hi - lo) * factor);
        double newLo    = mid - newRange * 0.5;
        double newHi    = mid + newRange * 0.5;
        m_axisY->setRange(newLo, newHi);
    }

    int pixelToPointIndex(int px) const
    {
        if (m_cumDistKm.isEmpty() || !m_axisX) return -1;
        double km = chart()->mapToValue(mapToScene(QPoint(px,0))).x();
        int best = 0; double bd = std::abs(m_cumDistKm[0]-km);
        for (int i=1; i<m_cumDistKm.size(); ++i) {
            double d = std::abs(m_cumDistKm[i]-km);
            if (d < bd) { bd=d; best=i; }
        }
        return best;
    }

    int divisorAt(const QPoint& pos) const
    {
        if (m_cumDistKm.isEmpty() || !m_axisX) return -1;
        for (int i=0; i<m_divisors.size(); ++i) {
            int pt = qBound(0, m_divisors[i], m_cumDistKm.size()-1);
            QPointF wp = mapFromScene(chart()->mapToPosition(
                QPointF(m_cumDistKm[pt], m_axisY ? m_axisY->min() : 0)));
            if (std::abs(wp.x()-pos.x()) <= SNAP_PX) return i;
        }
        return -1;
    }

    int stopAt(const QPoint& pos) const
    {
        if (m_cumDistKm.isEmpty() || !m_axisX || !m_axisY) return -1;
        for (int i=0; i<m_stopIndices.size(); ++i) {
            int pt = qBound(0, m_stopIndices[i], m_cumDistKm.size()-1);
            double yVal = m_axisY->min() + (m_axisY->max()-m_axisY->min())*0.15;
            QPointF wp = mapFromScene(chart()->mapToPosition(
                QPointF(m_cumDistKm[pt], yVal)));
            QPointF d = wp - QPointF(pos.x(), pos.y());
            if (std::sqrt(d.x()*d.x()+d.y()*d.y()) <= SNAP_PX+4) return i;
        }
        return -1;
    }

    void applyPan(int dPixelX, int dPixelY = 0)
    {
        if (m_axisX && dPixelX) {
            double lo=m_axisX->min(), hi=m_axisX->max(), range=hi-lo;
            double dKm = chart()->mapToValue(mapToScene(QPoint(dPixelX,0))).x()
                       - chart()->mapToValue(mapToScene(QPoint(0,0))).x();
            double newLo = qBound(m_fullXMin, lo+dKm, m_fullXMax-range);
            m_axisX->setRange(newLo, newLo+range);
        }
        if (m_axisY && dPixelY) {
            double lo=m_axisY->min(), hi=m_axisY->max(), range=hi-lo;
            // Pantalla: Y creix cap avall. Rang Y: valors alts a dalt.
            // Moure amunt (dPixelY<0) → dVal<0 → lo+dVal baixa el rang → corba puja ✓
            double dVal = dPixelY * range / chart()->plotArea().height();
            m_axisY->setRange(lo + dVal, hi + dVal);
        }
    }

    static constexpr int SNAP_PX = 10;

    QVector<int>    m_divisors, m_stopIndices;
    QVector<double> m_cumDistKm;
    int             m_totalPoints = 0;
    QValueAxis     *m_axisX = nullptr, *m_axisY = nullptr;
    double          m_fullXMin = 0.0, m_fullXMax = 1.0;
    double          m_fullYMin = 0.0, m_fullYMax = 1.0;
    bool            m_draggingDivisor = false;
    int             m_dragDivisorIdx  = -1;
    bool            m_panActive  = false;
    int             m_panLastX   = 0;
    int             m_panLastY   = 0;
    bool            m_yZoomActive = false;
    int             m_yZoomLastY  = 0;
};
