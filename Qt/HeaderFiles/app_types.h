#pragma once
/**
 * @file app_types.h
 * @brief MQTT 메시지(JSON)와 GUI 내부에서 공용으로 쓰는 데이터 타입 모음
 *
 * - UI(Qt Widgets)와는 완전히 분리되어야 함 (순수 데이터)
 * - thread 간 signal/slot(QueuedConnection) 전송을 위해 Q_DECLARE_METATYPE 포함
 */

#include <QString>
#include <QMetaType>
#include <QRect>
#include <QVector>

struct Shot {
    QString shotId;
    QString mapId;

    double x = 0.0;     // world (meters), map frame
    double y = 0.0;
    double theta = 0.0;

    QString imageUrl;
    QString thumbUrl;
    QString timestamp;
};

struct MapMeta {
    QString mapId;

    int widthCells = 0;
    int heightCells = 0;
    double resolution = 0.05;

    double originX = 0.0;
    double originY = 0.0;
    double originYaw = 0.0;

    QString imageUrl;
    QString timestamp;
};

struct Pose {
    QString mapId;
    double x = 0.0;
    double y = 0.0;
    double theta = 0.0;

    QString frameId = "map";
    QString timestamp;
};

struct MotorCmd {
    QString source = "unknown";
    QString mode   = "unknown";

    int speed = 0;           // 0~100 (UI 기준)
    int steer = 90;          // 0~180 (UI 기준)

    bool enable = true;
    bool emergencyStop = false;

    QString timestamp;
};

struct CameraCmd {
    QString source = "qt_gui";
    QString action = "start";   // "start" or "stop"
    int periodMs = 1000;
    QString timestamp;
};


struct HitBox {
    QRect rect;
    int shotIndex = -1;
};

Q_DECLARE_METATYPE(Shot)
Q_DECLARE_METATYPE(MapMeta)
Q_DECLARE_METATYPE(Pose)
Q_DECLARE_METATYPE(MotorCmd)
Q_DECLARE_METATYPE(HitBox)
Q_DECLARE_METATYPE(QVector<HitBox>)
