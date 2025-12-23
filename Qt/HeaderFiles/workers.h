#pragma once
/**
 * @file workers.h
 * @brief GUI에서 무거운 작업(HTTP 이미지 다운로드, 맵 렌더링, 로그 파일 쓰기 등)을
 *        QThread로 분리하기 위한 Worker 객체들
 *
 * ✅ 원칙
 * - QWidget/QPixmap 등 UI 오브젝트는 절대 Worker Thread에서 만지지 않는다.
 * - Worker는 순수 데이터(QImage/QString/struct)만 emit
 */

#include <QObject>
#include <QImage>
#include <QHash>
#include <QQueue>
#include <QTimer>

#include "app_types.h"

class QNetworkAccessManager;

class LogWorker final : public QObject
{
    Q_OBJECT
public:
    explicit LogWorker(QObject *parent = nullptr);

public slots:
    void setLogFile(const QString& filePath);
    void appendLine(const QString& line);
    void flush();

signals:
    void logLine(const QString& line);

private:
    QString m_path;
    class QFile *m_file = nullptr;
};

class MotorWorker final : public QObject
{
    Q_OBJECT
public:
    explicit MotorWorker(QObject *parent = nullptr);

public slots:
    void onMotorCmd(const MotorCmd& cmd);

signals:
    void speedChanged(int speed);
    void steerChanged(int steer);
    void logLine(const QString& line);

private:
    int m_lastSpeed = -1;
    int m_lastSteer = -1;
};

class CameraWorker final : public QObject
{
    Q_OBJECT
public:
    explicit CameraWorker(QObject *parent = nullptr);

public slots:
    void prefetchShotThumb(const Shot& shot);
    void onShotClicked(const Shot& shot);

signals:
    void cameraReady(const QImage& img);
    void logLine(const QString& line);

private slots:
    void processPrefetchQueue();

private:
    QNetworkAccessManager *m_net = nullptr;
    QHash<QString, QImage> m_cache;
    QQueue<QString> m_queue;
    QTimer *m_timer = nullptr;

    QImage fetchImageSync(const QString& url, int timeoutMs = 8000);
};

class MapRenderWorker final : public QObject
{
    Q_OBJECT
public:
    explicit MapRenderWorker(QObject *parent = nullptr);

public slots:
    void onMapState(const MapMeta& meta);
    void onCameraShot(const Shot& shot);
    void onPose(const Pose& pose);

signals:
    void mapImageRendered(const QImage& rendered, const QVector<HitBox>& hitboxes);
    void logLine(const QString& line);

private slots:
    void renderNow();

private:
    QNetworkAccessManager *m_net = nullptr;
    QHash<QString, QImage> m_cache;

    MapMeta m_meta;
    QImage m_base;

    QVector<Shot> m_shots;

    Pose m_pose;
    bool m_hasPose = false;

    QTimer *m_renderTimer = nullptr;
    bool m_renderScheduled = false;

    void scheduleRender();

    QImage fetchImageSync(const QString& url, int timeoutMs = 10000);
    QImage loadLocalImage(const QString& path);

    QPoint worldToPixel(const MapMeta& meta, const QImage& img, double x, double y) const;
};
