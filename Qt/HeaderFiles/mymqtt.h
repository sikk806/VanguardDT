// mymqtt.h

#pragma once
#include <QObject>
#include <QMqttClient>
#include "app_types.h"

class MyMqtt final : public QObject
{
    Q_OBJECT
public:
    explicit MyMqtt(QObject *parent = nullptr);
    ~MyMqtt() override;

    void connectToBroker(const QString& host = "127.0.0.1", int port = 1883,
                         const QString& clientId = "qt_gui");

    void disconnectFromBroker();
    bool isConnected() const;

    void publishMotorCmd(int speed, int steer, bool enable, bool emergencyStop,
                         const QString& mode = "remote",
                         const QString& source = "qt_gui");
    void publishCameraCmd(bool start, int periodMs = 1000, const QString& source = "qt_gui");


signals:
    void logLine(const QString& line);

    void connected();
    void disconnected();

    void motorCmdReceived(const MotorCmd& cmd);
    void mapStateReceived(const MapMeta& meta);
    void cameraShotReceived(const Shot& shot);
    void poseReceived(const Pose& pose);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessage(const QByteArray &msg, const QMqttTopicName &topic);

private:
    QMqttClient *m_client = nullptr;

    void subscribeDefault();
    void handleMotorCmdJson(const QByteArray &msg);
    void handleMapStateJson(const QByteArray &msg);
    void handleCameraShotJson(const QByteArray &msg);
    void handlePoseJson(const QByteArray &msg);

    static QString nowHMS();
    static QString isoUtcNowZ();
};
