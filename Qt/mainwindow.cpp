#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "gaugewidget.h"

#include <QMessageBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>

static QString nowHMS() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

static QString isoUtcNowZ() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) + "Z";
}

static QString fileNameFromUrlOrPath(const QString& s) {
    // url/path에서 파일명만 대충 추출
    int slash = s.lastIndexOf('/');
    if (slash >= 0 && slash + 1 < s.size()) return s.mid(slash + 1);
    return s;
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("MainWindow");

    speedGauge = new GaugeWidget(ui->veloGraph);
    speedGauge->setTitle("SPEED");
    speedGauge->setUnit("%");
    speedGauge->setRange(0, 100);

    speedGauge->setAnimated(true);

    auto *l = new QHBoxLayout(ui->veloGraph);
    l->setContentsMargins(8,8,8,8);
    l->addWidget(speedGauge);

    setupMqtt("127.0.0.1", 1883);
    setUiStateIdle();
}

MainWindow::~MainWindow()
{
    delete ui;
}


//----- MQTT -----
void MainWindow::setupMqtt(const QString& host, int port) {
    mqtt = new QMqttClient(this);
    mqtt->setHostname(host);
    mqtt->setPort(port);
    mqtt->setClientId("qt_gui");

    connect(mqtt, &QMqttClient::connected,    this, &MainWindow::onMqttConnected);
    connect(mqtt, &QMqttClient::disconnected, this, &MainWindow::onMqttDisconnected);
    connect(mqtt, &QMqttClient::messageReceived, this, &MainWindow::onMqttMessage);

    mqtt->connectToHost();
}

void MainWindow::onMqttConnected() {
    appendLog("[MQTT] connected");
    statusBar()->showMessage("MQTT connected");

    // 기존 구독 유지(너가 쓰던 것) + 명세 토픽 추가
    mqtt->subscribe(QMqttTopicFilter("rccar/state/#"), 0);

    // MQTT 명세 기반 (robot/*)
    mqtt->subscribe(QMqttTopicFilter("robot/motor_cmd"), 0);
    mqtt->subscribe(QMqttTopicFilter("robot/camera_shot"), 0);

    // map/pose는 이번엔 로그 과한듯해서 생략(요청대로)
    // mqtt->subscribe(QMqttTopicFilter("robot/map_state"), 0);
    // mqtt->subscribe(QMqttTopicFilter("robot/pose"), 0);
}

void MainWindow::onMqttDisconnected() {
    appendLog("[MQTT] disconnected");
    statusBar()->showMessage("MQTT disconnected");
}

void MainWindow::onMqttMessage(const QByteArray &msg, const QMqttTopicName &topic) {
    const QString t = topic.name();

    if (t == "robot/motor_cmd") {
        handleMotorCmdJson(msg);
        return;
    }
    if (t == "robot/camera_shot") {
        handleCameraShotJson(msg);
        return;
    }

    // 나머지는 기존처럼 raw 로그로 남김(구조 유지)
    const QString line = QString("[%1] %2 : %3")
                             .arg(nowHMS())
                             .arg(t)
                             .arg(QString::fromUtf8(msg));
    appendLog(line);
}

void MainWindow::appendLog(const QString& line) {
    ui->rccarLogs->appendPlainText(line);
}


//----- MQTT: JSON handlers -----
void MainWindow::handleMotorCmdJson(const QByteArray &msg)
{
    QJsonParseError err {};
    QJsonDocument doc = QJsonDocument::fromJson(msg, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        appendLog(QString("[%1] [motor_cmd] JSON parse error: %2")
                      .arg(nowHMS())
                      .arg(err.errorString()));
        return;
    }

    const QJsonObject o = doc.object();
    const int speed = o.value("speed").toInt(lastSpeed);
    const int steer = o.value("steer").toInt(lastSteer);
    const bool enable = o.value("enable").toBool(true);
    const bool estop  = o.value("emergency_stop").toBool(false);
    const QString mode = o.value("mode").toString("unknown");
    const QString source = o.value("source").toString("unknown");

    lastSpeed = speed;
    lastSteer = steer;

    // 계기판 업데이트
    if (speedGauge)
        speedGauge->setValue(speed);

    appendLog(QString("[%1] [motor_cmd] src=%2 mode=%3 speed=%4 steer=%5 enable=%6 estop=%7")
                  .arg(nowHMS())
                  .arg(source)
                  .arg(mode)
                  .arg(speed)
                  .arg(steer)
                  .arg(enable ? "true" : "false")
                  .arg(estop ? "true" : "false"));
}

void MainWindow::handleCameraShotJson(const QByteArray &msg)
{
    QJsonParseError err {};
    QJsonDocument doc = QJsonDocument::fromJson(msg, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        appendLog(QString("[%1] [camera_shot] JSON parse error: %2")
                      .arg(nowHMS())
                      .arg(err.errorString()));
        return;
    }

    const QJsonObject o = doc.object();
    const QString shotId = o.value("shot_id").toString("unknown");
    const double x = o.value("x").toDouble(0.0);
    const double y = o.value("y").toDouble(0.0);

    // 명세는 image_url로 오는데, 네 요청은 "사진 파일 이름" 출력
    const QString imageUrl = o.value("image_url").toString("");
    const QString fileName = fileNameFromUrlOrPath(imageUrl);

    appendLog(QString("[%1] [camera_shot] shot_id=%2 file=%3 point=(%4,%5)")
                  .arg(nowHMS())
                  .arg(shotId)
                  .arg(fileName.isEmpty() ? "-" : fileName)
                  .arg(x, 0, 'f', 2)
                  .arg(y, 0, 'f', 2));
}


//----- GUI -> MQTT publish -----
void MainWindow::publishMotorCmd(int speed, int steer, bool enable, bool emergencyStop, const QString& mode)
{
    if (!mqtt || mqtt->state() != QMqttClient::Connected) {
        appendLog(QString("[%1] [PUB] mqtt not connected").arg(nowHMS()));
        return;
    }

    const int sp = qBound(0, speed, 100);
    const int st = qBound(0, steer, 180);

    const int flags = (enable ? 0x01 : 0x00) | (emergencyStop ? 0x02 : 0x00);

    QJsonObject raw;
    raw["speed_byte"] = sp;
    raw["steer_byte"] = st;
    raw["flags_byte"] = flags;

    QJsonObject o;
    o["source"] = "qt_gui";
    o["speed"] = sp;
    o["steer"] = st;
    o["enable"] = enable;
    o["emergency_stop"] = emergencyStop;
    o["raw"] = raw;
    o["mode"] = mode;
    o["timestamp"] = isoUtcNowZ();

    const QByteArray payload = QJsonDocument(o).toJson(QJsonDocument::Compact);
    mqtt->publish(QMqttTopicName("robot/motor_cmd"), payload, 0, false);

    appendLog(QString("[%1] [PUB] robot/motor_cmd speed=%2 steer=%3 enable=%4 estop=%5")
                  .arg(nowHMS()).arg(sp).arg(st)
                  .arg(enable ? "true" : "false")
                  .arg(emergencyStop ? "true" : "false"));
}


//----- Button -----
void MainWindow::onStartClicked() {
    appendLog("[UI] START clicked");
    statusBar()->showMessage("START", 2000);
    setUiStateRunning();

    // 기본: enable on + estop 해제 + 현재(last) speed/steer로 “수동 명령”
    publishMotorCmd(lastSpeed, lastSteer, true, false, "remote");

    // (기존 pub도 유지하고 싶으면 남겨도 됨)
    // mqtt->publish(QMqttTopicName("gui/cmd"), QByteArray("START"), 0, false);
}

void MainWindow::onStopClicked() {
    appendLog("[UI] STOP clicked");
    statusBar()->showMessage("STOP", 2000);
    setUiStateStopped();

    // STOP은 “emergency_stop=true”로 강제 정지
    publishMotorCmd(0, lastSteer, true, true, "remote");

    // mqtt->publish(QMqttTopicName("gui/cmd"), QByteArray("STOP"), 0, false);
}

void MainWindow::onFinishClicked() {
    appendLog("[UI] FINISH clicked");
    statusBar()->showMessage("FINISH", 2000);

    // FINISH는 enable=false로 종료 느낌
    publishMotorCmd(0, lastSteer, false, false, "remote");

    // mqtt->publish(QMqttTopicName("gui/cmd"), QByteArray("FINISH"), 0, false);
}

void MainWindow::setUiStateIdle() {
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
    ui->finishBtn->setEnabled(true);
}

void MainWindow::setUiStateRunning() {
    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);
    ui->finishBtn->setEnabled(true);
}

void MainWindow::setUiStateStopped() {
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
    ui->finishBtn->setEnabled(true);
}
