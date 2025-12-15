#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMqttClient>
#include "gaugewidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QMqttClient *mqtt = nullptr;

    GaugeWidget *speedGauge = nullptr;

    int lastSpeed = 0;   // 0~100
    int lastSteer = 90;  // 0~180

    void setupMqtt(const QString& host = "127.0.0.1", int port = 1883);
    void appendLog(const QString& line);

    void setUiStateIdle();
    void setUiStateRunning();
    void setUiStateStopped();

    // MQTT 파싱/표시
    void handleMotorCmdJson(const QByteArray &msg);
    void handleCameraShotJson(const QByteArray &msg);

    // GUI -> MQTT publish (명세 기반 JSON)
    void publishMotorCmd(int speed, int steer, bool enable, bool emergencyStop, const QString& mode = "remote");

private slots:
    void onStartClicked();
    void onStopClicked();
    void onFinishClicked();

    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttMessage(const QByteArray &msg, const QMqttTopicName &topic);
};
#endif // MAINWINDOW_H
