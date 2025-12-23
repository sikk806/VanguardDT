#include "gaugewidget.h"
#include <QPainter>
#include <QtMath>

GaugeWidget::GaugeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(140, 140);

    // ✅ "displayValue" 프로퍼티를 애니메이션
    m_anim = new QPropertyAnimation(this, "displayValue", this);
    m_anim->setDuration(180);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

int GaugeWidget::clamp(int v) const
{
    if (v < m_min) return m_min;
    if (v > m_max) return m_max;
    return v;
}

double GaugeWidget::ratio() const
{
    if (m_max == m_min) return 0.0;
    return (double)(m_displayValue - m_min) / (double)(m_max - m_min);
}

void GaugeWidget::setRange(int minV, int maxV)
{
    if (minV >= maxV) return;
    m_min = minV;
    m_max = maxV;

    // ✅ 범위 변경 시 현재 값들을 안전하게 클램프 (여기서 setValue 호출 X)
    m_displayValue = clamp(m_displayValue);
    m_targetValue  = clamp(m_targetValue);
    update();
}

void GaugeWidget::setDisplayValue(int v)
{
    // ✅ 애니메이션이 계속 호출하는 setter: 여기서 애니메이션 재시작 절대 금지
    v = clamp(v);
    if (m_displayValue == v) return;
    m_displayValue = v;
    emit displayValueChanged(m_displayValue);
    update();
}

void GaugeWidget::setValue(int v)
{
    v = clamp(v);
    m_targetValue = v;

    if (!m_animated) {
        setDisplayValue(v);
        return;
    }

    if (m_anim->state() == QAbstractAnimation::Running)
        m_anim->stop();

    m_anim->setStartValue(m_displayValue);
    m_anim->setEndValue(v);
    m_anim->start();
}

void GaugeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int side = qMin(width(), height());
    const QPointF c(width() / 2.0, height() / 2.0);
    const double r = side * 0.40;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 24));
    p.drawEllipse(c, r * 1.15, r * 1.15);

    QPen ringPen(QColor(60, 60, 70));
    ringPen.setWidthF(r * 0.12);
    ringPen.setCapStyle(Qt::RoundCap);
    p.setPen(ringPen);
    p.setBrush(Qt::NoBrush);

    const double startDeg = 225.0;
    const double spanDeg  = 270.0;

    QRectF arcRect(c.x() - r, c.y() - r, 2*r, 2*r);
    p.drawArc(arcRect, (int)((90 - startDeg) * 16), (int)(-spanDeg * 16));

    QConicalGradient grad(c, 90);
    grad.setColorAt(0.00, QColor(0, 200, 255));
    grad.setColorAt(0.35, QColor(0, 255, 140));
    grad.setColorAt(0.70, QColor(255, 220, 0));
    grad.setColorAt(1.00, QColor(255, 60, 120));

    QPen progPen(QBrush(grad), r * 0.12);
    progPen.setCapStyle(Qt::RoundCap);
    p.setPen(progPen);

    const double vRatio = qBound(0.0, ratio(), 1.0);
    const double vSpan = spanDeg * vRatio;
    p.drawArc(arcRect, (int)((90 - startDeg) * 16), (int)(-vSpan * 16));

    p.setPen(QColor(230, 230, 235));
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(side * 0.10);
    p.setFont(f);
    p.drawText(QRectF(0, c.y() - side*0.05, width(), side*0.15),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::number(m_displayValue));

    QFont f2 = font();
    f2.setPointSizeF(side * 0.06);
    p.setFont(f2);
    p.setPen(QColor(180, 180, 190));
    p.drawText(QRectF(0, c.y() + side*0.08, width(), side*0.12),
               Qt::AlignHCenter | Qt::AlignTop,
               m_unit);

    QFont f3 = font();
    f3.setPointSizeF(side * 0.055);
    p.setFont(f3);
    p.setPen(QColor(160, 160, 170));
    p.drawText(QRectF(0, c.y() - side*0.28, width(), side*0.12),
               Qt::AlignHCenter | Qt::AlignVCenter,
               m_title);
}
