#include "drawmap.h"

#include <QLabel>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QDateTime>
#include <QMetaObject>

#include <cmath>


DrawMap::DrawMap(QLabel *mapLabel, QObject *parent)
    : QObject(parent), m_label(mapLabel)
{
    if (m_label) {
        // 클릭을 여기서 받기 위해 eventFilter 설치
        m_label->installEventFilter(this);
        m_label->setAlignment(Qt::AlignCenter);
    }
}

void DrawMap::setMapMeta(const MapMeta& meta)
{
    m_currentMap = meta;
}

void DrawMap::setBaseMapImage(const QImage& img)
{
    m_baseMapImage = img;
    m_useExternalRendered = false; // 기본 모드로 복귀
}

void DrawMap::addShot(const Shot& shot)
{
    m_shots.push_back(shot);
    requestUpdate();
}

void DrawMap::clearShots()
{
    m_shots.clear();
    m_hitboxes.clear();
}

void DrawMap::setPose(const Pose& pose)
{
    m_pose = pose;
    m_hasPose = true;
    requestUpdate();
}

void DrawMap::clearPose()
{
    m_hasPose = false;
}

QString DrawMap::nowHMS()
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

QPoint DrawMap::worldToPixel(double x, double y) const
{
    if (m_currentMap.resolution <= 0.0)
        return QPoint(-1, -1);
    if (m_baseMapImage.isNull())
        return QPoint(-1, -1);

    // 1) 월드(m) -> 셀(cell)
    const double cx = (x - m_currentMap.originX) / m_currentMap.resolution;
    const double cy = (y - m_currentMap.originY) / m_currentMap.resolution;

    // 2) 셀(cell) -> 픽셀(px) 스케일 (이미지 크기와 셀 크기가 다를 때 보정)
    const int imgW = m_baseMapImage.width();
    const int imgH = m_baseMapImage.height();

    // width_cells/height_cells가 0이면 fallback (이미지=셀이라고 가정)
    const double gridW = (m_currentMap.widthCells  > 0) ? m_currentMap.widthCells  : imgW;
    const double gridH = (m_currentMap.heightCells > 0) ? m_currentMap.heightCells : imgH;

    const double scaleX = imgW / gridW;
    const double scaleY = imgH / gridH;

    const int px = int(cx * scaleX);
    const int py = int((gridH - 1.0 - cy) * scaleY);  // y 뒤집기 (맵 좌표계 ↔ 이미지 좌표계)

    return QPoint(px, py);
}

void DrawMap::requestUpdate()
{
    if (m_updatePending) return;
    m_updatePending = true;

    QTimer::singleShot(0, this, [this](){
        m_updatePending = false;

        if (m_useExternalRendered && !m_lastRendered.isNull())
            updateLabelPixmapFromImage(m_lastRendered);
        else
            render();
    });
}


void DrawMap::updateLabelPixmapFromImage(const QImage& img)
{
    if (!m_label) return;
    if (img.isNull()) return;

    m_label->setPixmap(QPixmap::fromImage(img).scaled(
        m_label->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}

void DrawMap::render()
{
    if (!m_label) return;
    if (m_baseMapImage.isNull()) return;

    // 외부 렌더 모드에서는 내부 렌더를 호출하지 않음
    if (m_useExternalRendered && !m_lastRendered.isNull()) {
        updateLabelPixmapFromImage(m_lastRendered);
        return;
    }

    QImage draw = m_baseMapImage.convertToFormat(QImage::Format_ARGB32);
    QPainter p(&draw);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(draw.rect(), QColor(0, 0, 0, 55));

    m_hitboxes.clear();

    const int r = 1;

    // shot marker
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 0, 0, 255));

    for (int i = 0; i < m_shots.size(); ++i) {
        const Shot& s = m_shots[i];

        QPoint pt = worldToPixel(s.x, s.y);
        if (pt.x() < 0 || pt.y() < 0 || pt.x() >= draw.width() || pt.y() >= draw.height())
            continue;

        QRect rc(pt.x() - r, pt.y() - r, 2*r, 2*r);
        p.drawEllipse(rc);

        m_hitboxes.push_back(HitBox{rc, i});
    }

    // robot pose marker (optional)
    if (m_hasPose) {
        QPoint pt = worldToPixel(m_pose.x, m_pose.y);
        if (pt.x() >= 0 && pt.y() >= 0 && pt.x() < draw.width() && pt.y() < draw.height()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 200, 255, 220));
            p.drawEllipse(QRect(pt.x()-5, pt.y()-5, 10, 10));

            // heading line
            p.setPen(QPen(QColor(0, 200, 255, 220), 2));
            const double len = 18.0;
            const double dx = len * std::cos(-m_pose.theta); // y flip 고려
            const double dy = len * std::sin(-m_pose.theta);
            p.drawLine(QPointF(pt), QPointF(pt.x() + dx, pt.y() + dy));
        }
    }

    updateLabelPixmapFromImage(draw);
}

void DrawMap::setRenderedImage(const QImage& rendered,
                              const QVector<HitBox>& hitboxes)
{
    if (rendered.isNull()) return;

    m_useExternalRendered = true;
    m_lastRendered = rendered;

    // 클릭 좌표 변환은 "원본 이미지 크기"가 필요하므로 baseMapImage도 동일 크기로 둔다.
    m_baseMapImage = rendered;
    m_hitboxes = hitboxes;

    updateLabelPixmapFromImage(m_lastRendered);

    requestUpdate();;
}

bool DrawMap::labelPosToImagePos(const QPoint& labelPos, QPoint& outImgPos) const
{
    if (!m_label) return false;

    const QPixmap pm = m_label->pixmap();
    if (pm.isNull() || m_baseMapImage.isNull()) return false;

    const QSize lbl = m_label->size();
    const QSize pmSize = pm.size();

    const Qt::Alignment al = m_label->alignment();

    int offsetX = 0;
    int offsetY = 0;

    if (al & Qt::AlignHCenter) offsetX = (lbl.width() - pmSize.width()) / 2;
    else if (al & Qt::AlignRight) offsetX = (lbl.width() - pmSize.width());
    else offsetX = 0;

    if (al & Qt::AlignVCenter) offsetY = (lbl.height() - pmSize.height()) / 2;
    else if (al & Qt::AlignBottom) offsetY = (lbl.height() - pmSize.height());
    else offsetY = 0;

    const QRect pixRect(offsetX, offsetY, pmSize.width(), pmSize.height());
    if (!pixRect.contains(labelPos)) return false;

    const double sx = (labelPos.x() - pixRect.x()) / double(pixRect.width());
    const double sy = (labelPos.y() - pixRect.y()) / double(pixRect.height());

    const int ix = int(sx * m_baseMapImage.width());
    const int iy = int(sy * m_baseMapImage.height());

    outImgPos = QPoint(ix, iy);
    return true;
}

int DrawMap::findHitMarkerIndex(const QPoint& imgPos) const
{
    for (const auto& hb : m_hitboxes) {
        if (hb.rect.contains(imgPos)) return hb.shotIndex;
    }
    return -1;
}

bool DrawMap::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_label && event->type() == QEvent::Resize) {
        // 리사이즈 직후 재표시: 
        // - 기본 모드: render()
        // - 외부 렌더 모드: 마지막 이미지 스케일만
        QMetaObject::invokeMethod(this, [this]() {
            if (m_useExternalRendered && !m_lastRendered.isNull()) {
                updateLabelPixmapFromImage(m_lastRendered);
            } else {
                this->render();
            }
        }, Qt::QueuedConnection);
        return false;
    }

    if (obj == m_label && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);

        QPoint imgPos;
        const bool ok = labelPosToImagePos(me->pos(), imgPos);

        emit logLine(QString("[%1] [click] label=(%2,%3) ok=%4 img=(%5,%6)")
                         .arg(nowHMS())
                         .arg(me->pos().x()).arg(me->pos().y())
                         .arg(ok ? "true" : "false")
                         .arg(imgPos.x()).arg(imgPos.y()));

        if (!ok) return true;

        const int idx = findHitMarkerIndex(imgPos);
        emit logLine(QString("[%1] [click] hit idx=%2").arg(nowHMS()).arg(idx));

        if (idx >= 0) {
            if (idx < m_shots.size()) {
                emit shotClicked(m_shots[idx]);
            } else {
                emit logLine(QString("[%1] [click] hit idx out of range: %2 (shots=%3)")
                                 .arg(nowHMS()).arg(idx).arg(m_shots.size()));
            }
        }
        return true;
    }

    return QObject::eventFilter(obj, event);
}
