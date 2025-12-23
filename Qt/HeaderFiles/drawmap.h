#pragma once
/**
 * @file drawmap.h
 * @brief "맵 이미지 + shot 마커" 렌더링과 클릭 히트테스트 담당
 *
 * - 기본 모드: baseMapImage를 받아서 내부 QPainter로 렌더(render())
 * - 고급 모드: Worker Thread에서 미리 렌더된 QImage를 받아 setRenderedImage()로 표시
 *   (UI 스레드는 Pixmap 스케일/클릭 히트만 담당)
 */

#include <QObject>
#include <QImage>
#include <QVector>
#include <QRect>
#include <QPair>
#include <QTimer>

#include "app_types.h"

class QLabel;

class DrawMap final : public QObject
{
    Q_OBJECT
public:
    explicit DrawMap(QLabel *mapLabel, QObject *parent = nullptr);

    void setMapMeta(const MapMeta& meta);
    void setBaseMapImage(const QImage& img);

    void addShot(const Shot& shot);
    void clearShots();

    void setPose(const Pose& pose);
    void clearPose();

    // 기본 모드 렌더
    void render();

    // Worker Thread가 그려준 이미지/히트박스를 그대로 표시 (외부 렌더 모드로 전환)
    void setRenderedImage(const QImage& rendered,
                          const QVector<HitBox>& hitboxes);

signals:
    void logLine(const QString& line);
    void shotClicked(const Shot& shot);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QLabel *m_label = nullptr;

    MapMeta m_currentMap;
    QImage  m_baseMapImage;   // "원본 이미지" 좌표계 기준

    QVector<Shot> m_shots;

    Pose m_pose;
    bool m_hasPose = false;

    // 클릭 판정용: (원본 이미지 좌표계 hitbox, shotIndex)
    QVector<HitBox> m_hitboxes;

    bool m_updatePending = false;
    void requestUpdate();

    // 외부 렌더 모드
    bool m_useExternalRendered = false;
    QImage m_lastRendered; // 원본 이미지 크기

    QPoint worldToPixel(double x, double y) const;

    // QLabel의 마우스 좌표 -> "원본 이미지" 좌표로 변환
    bool labelPosToImagePos(const QPoint& labelPos, QPoint& outImgPos) const;

    int findHitMarkerIndex(const QPoint& imgPos) const;

    void updateLabelPixmapFromImage(const QImage& img);

    static QString nowHMS();
};
