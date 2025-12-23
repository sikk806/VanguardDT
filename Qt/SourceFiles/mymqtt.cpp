#include "mymqtt.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttTopicFilter>

static int clampInt(int v, int lo, int hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

MyMqtt::MyMqtt(QObject *parent)
    : QObject(parent)
{
    m_client = new QMqttClient(this);

    connect(m_client, &QMqttClient::connected, this, &MyMqtt::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MyMqtt::onDisconnected);
    connect(m_client, &QMqttClient::messageReceived, this, &MyMqtt::onMessage);
}

MyMqtt::~MyMqtt() = default;

void MyMqtt::connectToBroker(const QString& host, int port, const QString& clientId)
{
    if (!m_client) return;

    if (m_client->state() == QMqttClient::Connected ||
        m_client->state() == QMqttClient::Connecting) {
        emit logLine(QString("[%1] [MQTT] already connected/connecting").arg(nowHMS()));
        return;
    }

    m_client->setHostname(host);
    m_client->setPort(port);
    m_client->setClientId(clientId);

    emit logLine(QString("[%1] [MQTT] connect %2:%3").arg(nowHMS(), host).arg(port));
    m_client->connectToHost();
}

void MyMqtt::disconnectFromBroker()
{
    if (!m_client) return;

    if (m_client->state() == QMqttClient::Disconnected) {
        emit logLine(QString("[%1] [MQTT] already disconnected").arg(nowHMS()));
        return;
    }

    emit logLine(QString("[%1] [MQTT] disconnect").arg(nowHMS()));
    m_client->disconnectFromHost();
}

bool MyMqtt::isConnected() const
{
    return m_client && m_client->state() == QMqttClient::Connected;
}

void MyMqtt::subscribeDefault()
{
    // 프로젝트 명세 기반 토픽
    m_client->subscribe(QMqttTopicFilter("robot/map_state"), 0);
    m_client->subscribe(QMqttTopicFilter("robot/camera_shot"), 0);
    m_client->subscribe(QMqttTopicFilter("robot/motor_cmd"), 0);
    m_client->subscribe(QMqttTopicFilter("robot/pose"), 0);

    // 이전에 쓰던 토픽 유지하고 싶으면 추가
    m_client->subscribe(QMqttTopicFilter("rccar/state/#"), 0);
}

void MyMqtt::onConnected()
{
    emit logLine(QString("[%1] [MQTT] connected").arg(nowHMS()));
    subscribeDefault();

    emit connected();
}

void MyMqtt::onDisconnected()
{
    emit logLine(QString("[%1] [MQTT] disconnected").arg(nowHMS()));

    emit disconnected();
}

void MyMqtt::onMessage(const QByteArray &msg, const QMqttTopicName &topic)
{
    const QString t = topic.name();

    if (t == "robot/motor_cmd") { handleMotorCmdJson(msg); return; }
    if (t == "robot/map_state") { handleMapStateJson(msg); return; }
    if (t == "robot/camera_shot") { handleCameraShotJson(msg); return; }
    if (t == "robot/pose") { handlePoseJson(msg); return; }

    // 그 외는 raw로 로그만 남김 (디버깅용)
    emit logLine(QString("[%1] %2 : %3").arg(nowHMS(), t, QString::fromUtf8(msg)));
}

QString MyMqtt::nowHMS()
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

QString MyMqtt::isoUtcNowZ()
{
    // Qt::ISODateWithMs = "yyyy-MM-ddTHH:mm:ss.zzz"
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) + "Z";
}

void MyMqtt::handleMotorCmdJson(const QByteArray &msg)
{
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(msg, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logLine(QString("[%1] [motor_cmd] JSON parse error: %2")
                     .arg(nowHMS(), err.errorString()));
        return;
    }

    const QJsonObject o = doc.object();

    MotorCmd cmd;
    cmd.source = o.value("source").toString("unknown");
    cmd.mode   = o.value("mode").toString("unknown");
    cmd.speed  = clampInt(o.value("speed").toInt(0), 0, 100);
    cmd.steer  = clampInt(o.value("steer").toInt(90), 0, 180);
    cmd.enable = o.value("enable").toBool(true);
    cmd.emergencyStop = o.value("emergency_stop").toBool(false);
    cmd.timestamp = o.value("timestamp").toString("");

    emit motorCmdReceived(cmd);

    emit logLine(QString("[%1] [motor_cmd] src=%2 mode=%3 speed=%4 steer=%5 enable=%6 estop=%7")
                 .arg(nowHMS(),
                      cmd.source, cmd.mode,
                      QString::number(cmd.speed),
                      QString::number(cmd.steer),
                      cmd.enable ? "true" : "false",
                      cmd.emergencyStop ? "true" : "false"));
}

void MyMqtt::handleMapStateJson(const QByteArray &msg)
{
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(msg, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logLine(QString("[%1] [map_state] JSON parse error: %2")
                     .arg(nowHMS(), err.errorString()));
        return;
    }

    const QJsonObject o = doc.object();

    MapMeta meta;
    meta.mapId      = o.value("map_id").toString("unknown");
    meta.imageUrl   = o.value("image_url").toString("");
    meta.widthCells = o.value("width_cells").toInt(0);
    meta.heightCells= o.value("height_cells").toInt(0);
    meta.resolution = o.value("resolution").toDouble(0.05);
    meta.originX    = o.value("origin_x").toDouble(0.0);
    meta.originY    = o.value("origin_y").toDouble(0.0);
    meta.originYaw  = o.value("origin_yaw").toDouble(0.0);
    meta.timestamp  = o.value("timestamp").toString("");

    emit mapStateReceived(meta);

    emit logLine(QString("[%1] [map_state] map_id=%2 url=%3 cells=%4x%5 res=%6 origin=(%7,%8)")
                 .arg(nowHMS(),
                      meta.mapId,
                      meta.imageUrl,
                      QString::number(meta.widthCells),
                      QString::number(meta.heightCells),
                      QString::number(meta.resolution, 'f', 3),
                      QString::number(meta.originX, 'f', 3),
                      QString::number(meta.originY, 'f', 3)));
}

void MyMqtt::handleCameraShotJson(const QByteArray &msg)
{
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(msg, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logLine(QString("[%1] [camera_shot] JSON parse error: %2")
                     .arg(nowHMS(), err.errorString()));
        return;
    }

    const QJsonObject o = doc.object();

    Shot s;
    s.shotId   = o.value("shot_id").toString("unknown");
    s.mapId    = o.value("map_id").toString("");
    s.x        = o.value("x").toDouble(0.0);
    s.y        = o.value("y").toDouble(0.0);
    s.theta    = o.value("theta").toDouble(0.0);
    s.imageUrl = o.value("image_url").toString("");
    s.thumbUrl = o.value("thumbnail_url").toString("");
    s.timestamp= o.value("timestamp").toString("");

    emit cameraShotReceived(s);

    const QString best = !s.imageUrl.isEmpty() ? s.imageUrl : s.thumbUrl;
    emit logLine(QString("[%1] [camera_shot] map_id=%2 shot_id=%3 (x,y)=(%4,%5) img=%6")
                 .arg(nowHMS(),
                      s.mapId,
                      s.shotId,
                      QString::number(s.x, 'f', 2),
                      QString::number(s.y, 'f', 2),
                      best));
}

void MyMqtt::handlePoseJson(const QByteArray &msg)
{
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(msg, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logLine(QString("[%1] [pose] JSON parse error: %2")
                     .arg(nowHMS(), err.errorString()));
        return;
    }

    const QJsonObject o = doc.object();

    Pose p;
    p.mapId = o.value("map_id").toString("");
    p.x = o.value("x").toDouble(0.0);
    p.y = o.value("y").toDouble(0.0);
    p.theta = o.value("theta").toDouble(0.0);
    p.frameId = o.value("frame_id").toString("map");
    p.timestamp = o.value("timestamp").toString("");

    emit poseReceived(p);

    emit logLine(QString("[%1] [pose] map_id=%2 (x,y,th)=(%3,%4,%5)")
                 .arg(nowHMS(),
                      p.mapId,
                      QString::number(p.x, 'f', 2),
                      QString::number(p.y, 'f', 2),
                      QString::number(p.theta, 'f', 2)));
}

void MyMqtt::publishMotorCmd(int speed, int steer, bool enable, bool emergencyStop,
                            const QString& mode, const QString& source)
{
    if (!isConnected()) {
        emit logLine(QString("[%1] [PUB] mqtt not connected").arg(nowHMS()));
        return;
    }

    const int sp = clampInt(speed, 0, 100);
    const int st = clampInt(steer, 0, 180);

    const int flags = (enable ? 0x01 : 0x00) | (emergencyStop ? 0x02 : 0x00);

    QJsonObject raw;
    raw["speed_byte"] = sp;
    raw["steer_byte"] = st;
    raw["flags_byte"] = flags;

    QJsonObject o;
    o["source"] = source;
    o["speed"] = sp;
    o["steer"] = st;
    o["enable"] = enable;
    o["emergency_stop"] = emergencyStop;
    o["raw"] = raw;
    o["mode"] = mode;
    o["timestamp"] = isoUtcNowZ();

    const QByteArray payload = QJsonDocument(o).toJson(QJsonDocument::Compact);

    m_client->publish(QMqttTopicName("robot/motor_cmd"), payload, 0, false);

    emit logLine(QString("[%1] [PUB] robot/motor_cmd speed=%2 steer=%3 enable=%4 estop=%5")
                 .arg(nowHMS(),
                      QString::number(sp),
                      QString::number(st),
                      enable ? "true" : "false",
                      emergencyStop ? "true" : "false"));
}

void MyMqtt::publishCameraCmd(bool start, int periodMs, const QString& source)
{
    if (!m_client || m_client->state() != QMqttClient::Connected) return;

    QJsonObject o;
    o["source"] = source;
    o["action"] = start ? "start" : "stop";
    if (start) o["period_ms"] = clampInt(periodMs, 100, 60000);
    o["timestamp"] = isoUtcNowZ();

    const QByteArray payload = QJsonDocument(o).toJson(QJsonDocument::Compact);
    m_client->publish(QMqttTopicName("robot/camera_cmd"), payload, 0, false);

    emit logLine(QString("[%1] [PUB] robot/camera_cmd action=%2 period_ms=%3")
                     .arg(nowHMS(),
                          start ? "start" : "stop",
                          QString::number(periodMs)));
}
