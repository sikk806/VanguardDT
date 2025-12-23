#pragma once
/**
 * @file mainwindow.h
 * @brief 앱 "조립" 담당: UI + MyMqtt + DrawMap + Worker Threads
 */

#include <QMainWindow>
#include <QImage>

#include "app_types.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class GaugeWidget;
class MyMqtt;
class DrawMap;
class DrawCamera;

class QThread;
class LogWorker;
class MotorWorker;
class CameraWorker;
class MapRenderWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool loadRosMapFromYaml(const QString& yamlPath);

signals:
    void logToFile(const QString& line);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onFinishClicked();
    void onCameraStartClicked();
    void onCameraStopClicked();

private:
    Ui::MainWindow *ui = nullptr;

    GaugeWidget *m_speedGauge = nullptr;

    MyMqtt *m_mqtt = nullptr;
    DrawMap *m_map = nullptr;
    DrawCamera *m_camera = nullptr;

    // 마지막으로 받은(또는 보낸) 조작값
    int m_lastSpeed = 0;   // 0~100
    int m_lastSteer = 90;  // 0~180

    MapMeta m_currentMap;

    // --- worker threads ---
    QThread *m_logThread = nullptr;
    QThread *m_motorThread = nullptr;
    QThread *m_cameraThread = nullptr;
    QThread *m_mapThread = nullptr;

    LogWorker *m_logWorker = nullptr;
    MotorWorker *m_motorWorker = nullptr;
    CameraWorker *m_cameraWorker = nullptr;
    MapRenderWorker *m_mapWorker = nullptr;

    void setupUiWidgets();
    void setupMqtt();
    void setupThreads();
    void setupConnections();

    void appendLog(const QString& line);

    void setUiStateIdle();
    void setUiStateRunning();
    void setUiStateStopped();
    void setUiStateCameraRunning();
    void setUiStateCameraStopped();

    bool loadRosMapYamlInternal(const QString& yamlPath, MapMeta& outMeta, QImage& outImg);
};
