#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "gaugewidget.h"
#include "mymqtt.h"
#include "drawmap.h"
#include "drawcamera.h"
#include "workers.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QThread>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QImage>

static QString nowHMS() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->rccarLogs->setMaximumBlockCount(500);
    setWindowTitle("VANGUARD-DT");

    setupUiWidgets();
    setupMqtt();
    setupThreads();
    setupConnections();

    // 로컬 YAML로 맵 테스트
    loadRosMapFromYaml("C:/Users/SSAFY/Downloads/my_map.yaml");

    setUiStateIdle();
}

MainWindow::~MainWindow()
{
    // 로그 flush
    if (m_logWorker) {
        QMetaObject::invokeMethod(m_logWorker, "flush", Qt::BlockingQueuedConnection);
    }

    auto stopThread = [](QThread* t){
        if (!t) return;
        t->quit();
        t->wait(2000);
    };

    stopThread(m_mapThread);
    stopThread(m_cameraThread);
    stopThread(m_motorThread);
    stopThread(m_logThread);

    delete ui;
}

void MainWindow::setupUiWidgets()
{
    // --- speed gauge ---
    m_speedGauge = new GaugeWidget(ui->veloGraph);
    m_speedGauge->setTitle("SPEED");
    m_speedGauge->setUnit("%");
    m_speedGauge->setRange(0, 100);
    m_speedGauge->setAnimated(true);

    auto *l = new QHBoxLayout(ui->veloGraph);
    l->setContentsMargins(8,8,8,8);
    l->addWidget(m_speedGauge);

    // --- map/camera helpers ---
    m_map = new DrawMap(ui->mapImage, this);
    m_camera = new DrawCamera(ui->cameraImage, this);

    ui->mapImage->setAlignment(Qt::AlignCenter);
    ui->cameraImage->setAlignment(Qt::AlignCenter);

    // 버튼 연결
    connect(ui->startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->cameraStartBtn, &QPushButton::clicked, this, &MainWindow::onCameraStartClicked);
    connect(ui->cameraStopBtn, &QPushButton::clicked, this, &MainWindow::onCameraStopClicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(ui->finishBtn, &QPushButton::clicked, this, &MainWindow::onFinishClicked);
}

void MainWindow::setupMqtt()
{
    m_mqtt = new MyMqtt(this);
}

void MainWindow::setupThreads()
{
    // threads
    m_logThread = new QThread(this);
    m_motorThread = new QThread(this);
    m_cameraThread = new QThread(this);
    m_mapThread = new QThread(this);

    // workers (no parent; will be deleted when thread finishes)
    m_logWorker = new LogWorker();
    m_motorWorker = new MotorWorker();
    m_cameraWorker = new CameraWorker();
    m_mapWorker = new MapRenderWorker();

    m_logWorker->moveToThread(m_logThread);
    m_motorWorker->moveToThread(m_motorThread);
    m_cameraWorker->moveToThread(m_cameraThread);
    m_mapWorker->moveToThread(m_mapThread);

    connect(m_logThread, &QThread::finished, m_logWorker, &QObject::deleteLater);
    connect(m_motorThread, &QThread::finished, m_motorWorker, &QObject::deleteLater);
    connect(m_cameraThread, &QThread::finished, m_cameraWorker, &QObject::deleteLater);
    connect(m_mapThread, &QThread::finished, m_mapWorker, &QObject::deleteLater);

    m_logThread->start();
    m_motorThread->start();
    m_cameraThread->start();
    m_mapThread->start();

    // 로그 파일 경로 설정
    const QString logDir = QCoreApplication::applicationDirPath() + "/logs";
    const QString logPath = logDir + "/qt_gui_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";

    QMetaObject::invokeMethod(m_logWorker, "setLogFile", Qt::QueuedConnection, Q_ARG(QString, logPath));
}

void MainWindow::setupConnections()
{
    // --- log: UI + file ---
    connect(this, &MainWindow::logToFile, m_logWorker, &LogWorker::appendLine, Qt::QueuedConnection);
    connect(m_logWorker, &LogWorker::logLine, this, &MainWindow::appendLog);

    // mqtt log
    connect(m_mqtt, &MyMqtt::logLine, this, &MainWindow::appendLog);

    // map internal log
    connect(m_map, &DrawMap::logLine, this, &MainWindow::appendLog);

    // worker logs
    connect(m_cameraWorker, &CameraWorker::logLine, this, &MainWindow::appendLog);
    connect(m_mapWorker, &MapRenderWorker::logLine, this, &MainWindow::appendLog);

    // --- motor thread ---
    connect(m_mqtt, &MyMqtt::motorCmdReceived, m_motorWorker, &MotorWorker::onMotorCmd, Qt::QueuedConnection);
    connect(m_motorWorker, &MotorWorker::speedChanged, this, [this](int sp){
        m_lastSpeed = sp;
        if (m_speedGauge) m_speedGauge->setValue(sp);
    });
    connect(m_motorWorker, &MotorWorker::steerChanged, this, [this](int st){
        m_lastSteer = st;
    });

    // --- map thread ---
    connect(m_mqtt, &MyMqtt::mapStateReceived, this, [this](const MapMeta& meta){
        m_currentMap = meta;
        m_map->setMapMeta(meta);
        m_map->clearShots();
        m_camera->setImage(QImage());

        QMetaObject::invokeMethod(m_mapWorker, "onMapState", Qt::QueuedConnection, Q_ARG(MapMeta, meta));
    });

    connect(m_mqtt, &MyMqtt::cameraShotReceived, this, [this](const Shot& s){
        // UI도 동일 순서로 shot을 저장해야 click idx가 맞음
        m_map->addShot(s);

        QMetaObject::invokeMethod(m_mapWorker, "onCameraShot", Qt::QueuedConnection, Q_ARG(Shot, s));
        QMetaObject::invokeMethod(m_cameraWorker, "prefetchShotThumb", Qt::QueuedConnection, Q_ARG(Shot, s));
    });

    connect(m_mqtt, &MyMqtt::poseReceived, this, [this](const Pose& p){
        // mqtt pose도 mapWorker 렌더에 사용
        QMetaObject::invokeMethod(m_mapWorker, "onPose", Qt::QueuedConnection, Q_ARG(Pose, p));

        // 로컬 yaml 렌더(기본 모드)일 때도 보이도록(선택)
        m_map->setPose(p);
    });

    connect(m_mapWorker, &MapRenderWorker::mapImageRendered, this,
            [this](const QImage& rendered, const QVector<HitBox>& hitboxes){
        m_map->setRenderedImage(rendered, hitboxes);
    }, Qt::QueuedConnection);

    // map 클릭 -> camera worker
    connect(m_map, &DrawMap::shotClicked, this, [this](const Shot& s){
        QMetaObject::invokeMethod(m_cameraWorker, "onShotClicked", Qt::QueuedConnection, Q_ARG(Shot, s));
    });

    connect(m_cameraWorker, &CameraWorker::cameraReady, this, [this](const QImage& img){
        m_camera->setImage(img);
    }, Qt::QueuedConnection);

    connect(m_mqtt, &MyMqtt::disconnected, this, [this](){
        setUiStateStopped();        // cameraStart/Stop 둘 다 disable 포함된 상태
    });

}

void MainWindow::appendLog(const QString& line)
{
    if (ui->rccarLogs) ui->rccarLogs->appendPlainText(line);
    emit logToFile(line);
}

void MainWindow::onStartClicked()
{
    statusBar()->showMessage("MQTT START", 1500);
    setUiStateRunning();

    if (!m_mqtt)
        return;

    if (m_mqtt->isConnected()) {
        m_mqtt->publishMotorCmd(m_lastSpeed, m_lastSteer, true, false, "remote");
        return;
    }

    connect(m_mqtt, &MyMqtt::connected, this, [this]() {
        appendLog(QString("[%1] [UI] MQTT connected -> send enable").arg(nowHMS()));
        m_mqtt->publishMotorCmd(m_lastSpeed, m_lastSteer, true, false, "remote");
    }, Qt::SingleShotConnection);

    // 연결
    m_mqtt->connectToBroker("10.60.106.137", 1883, "qt_gui");
}

void MainWindow::onStopClicked()
{
    statusBar()->showMessage("MQTT STOP", 1500);

    if (m_mqtt && m_mqtt->isConnected()) {
        m_mqtt->publishCameraCmd(false, 1000, "qt_gui");
    }

    setUiStateStopped();

    if (!m_mqtt) return;
    if (m_mqtt->isConnected()) {
        m_mqtt->publishMotorCmd(0, m_lastSteer, true, true, "remote");
    }
    m_mqtt->disconnectFromBroker();
}


void MainWindow::onFinishClicked()
{
    statusBar()->showMessage("MQTT FINISH", 1500);

    if (m_mqtt) {
        m_mqtt->publishMotorCmd(0, m_lastSteer, false, false, "remote");
    }
}

void MainWindow::onCameraStartClicked()
{
    statusBar()->showMessage("CAMERA START", 1500);
    if (!m_mqtt) return;

    setUiStateCameraRunning();

    auto doPub = [this]() {
        m_mqtt->publishCameraCmd(true, 1000, "qt_gui");
        appendLog(QString("[%1] [UI] send camera start").arg(nowHMS()));
    };

    // 연결돼있으면 바로 publish
    if (m_mqtt->isConnected()) {
        doPub();
        return;
    }

    // 아직 연결 전이면: 연결되자마자 1회 publish
    connect(m_mqtt, &MyMqtt::connected, this, [doPub]() { doPub(); }, Qt::SingleShotConnection);
    m_mqtt->connectToBroker("10.252.52.29", 1883, "qt_gui");
}

void MainWindow::onCameraStopClicked()
{
    statusBar()->showMessage("CAMERA STOP", 1500);
    if (!m_mqtt) return;

    setUiStateCameraStopped();

    // 연결돼있을 때만 publish (안돼있으면 UI만 바뀌고, 실제 stop 명령은 못 감)
    if (!m_mqtt->isConnected()) {
        appendLog(QString("[%1] [UI] camera stop requested but mqtt not connected").arg(nowHMS()));
        return;
    }

    m_mqtt->publishCameraCmd(false, 1000, "qt_gui");
    appendLog(QString("[%1] [UI] send camera stop").arg(nowHMS()));
}

void MainWindow::setUiStateIdle()
{
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
    ui->finishBtn->setEnabled(true);

    ui->cameraStartBtn->setEnabled(false);
    ui->cameraStopBtn->setEnabled(false);
}

void MainWindow::setUiStateRunning()
{
    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);
    ui->finishBtn->setEnabled(true);

    ui->cameraStartBtn->setEnabled(true);
    ui->cameraStopBtn->setEnabled(false);
}

void MainWindow::setUiStateCameraRunning()
{
    ui->cameraStartBtn->setEnabled(false);
    ui->cameraStopBtn->setEnabled(true);
}

void MainWindow::setUiStateCameraStopped()
{
    ui->cameraStartBtn->setEnabled(true);
    ui->cameraStopBtn->setEnabled(false);
}

void MainWindow::setUiStateStopped()
{
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
    ui->finishBtn->setEnabled(true);

    ui->cameraStartBtn->setEnabled(false);
    ui->cameraStopBtn->setEnabled(false);
}


// ---------------- YAML map loader (로컬 테스트용) ----------------
static QString stripQuotes(QString s)
{
    s = s.trimmed();
    if (s.size() >= 2) {
        if ((s.front() == '"'  && s.back() == '"') ||
            (s.front() == '\'' && s.back() == '\'')) {
            s = s.mid(1, s.size() - 2);
        }
    }
    return s.trimmed();
}

static bool parseBracketList3(const QString& s, double& a, double& b, double& c)
{
    // origin: [x, y, yaw]
    QString t = s.trimmed();
    if (!t.startsWith('[') || !t.endsWith(']')) return false;
    t = t.mid(1, t.size() - 2);

    const auto parts = t.split(',', Qt::SkipEmptyParts);
    if (parts.size() < 3) return false;

    bool ok1=false, ok2=false, ok3=false;
    a = parts[0].trimmed().toDouble(&ok1);
    b = parts[1].trimmed().toDouble(&ok2);
    c = parts[2].trimmed().toDouble(&ok3);
    return ok1 && ok2 && ok3;
}

bool MainWindow::loadRosMapYamlInternal(const QString& yamlPath, MapMeta& outMeta, QImage& outImg)
{
    QFile f(yamlPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog(QString("[%1] [map] open fail: %2").arg(nowHMS(), yamlPath));
        return false;
    }

    QFileInfo yfi(yamlPath);
    const QString yamlDir = yfi.absolutePath();

    // 기본값
    QString imageRel;
    double resolution = 0.05;
    double ox=0.0, oy=0.0, oyaw=0.0;
    int negate = 0;

    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString line = ts.readLine();

        const int hash = line.indexOf('#');
        if (hash >= 0) line = line.left(hash);

        line = line.trimmed();
        if (line.isEmpty()) continue;

        const int colon = line.indexOf(':');
        if (colon < 0) continue;

        const QString key = line.left(colon).trimmed();
        QString val = line.mid(colon + 1).trimmed();
        val = stripQuotes(val);

        if (key == "image") {
            imageRel = val;
        } else if (key == "resolution") {
            bool ok=false;
            const double r = val.toDouble(&ok);
            if (ok) resolution = r;
        } else if (key == "origin") {
            double a,b,c;
            if (parseBracketList3(val, a,b,c)) {
                ox=a; oy=b; oyaw=c;
            }
        } else if (key == "negate") {
            bool ok=false;
            const int n = val.toInt(&ok);
            if (ok) negate = n;
        }
    }

    if (imageRel.isEmpty()) {
        appendLog(QString("[%1] [map] yaml has no image field").arg(nowHMS()));
        return false;
    }

    const QString imagePath = QDir(yamlDir).absoluteFilePath(imageRel);

    QImage img;
    if (!img.load(imagePath)) {
        appendLog(QString("[%1] [map] image load fail: %2").arg(nowHMS(), imagePath));
        return false;
    }

    img = img.convertToFormat(QImage::Format_Grayscale8);
    if (negate == 1) {
        img.invertPixels();
    }

    MapMeta meta;
    meta.mapId = QFileInfo(yamlPath).baseName();
    meta.widthCells = img.width();
    meta.heightCells = img.height();
    meta.resolution = resolution;
    meta.originX = ox;
    meta.originY = oy;
    meta.originYaw = oyaw;
    meta.imageUrl = imagePath;
    meta.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + "Z";

    outMeta = meta;
    outImg = img;
    return true;
}

bool MainWindow::loadRosMapFromYaml(const QString& yamlPath)
{
    MapMeta meta;
    QImage img;

    if (!loadRosMapYamlInternal(yamlPath, meta, img)) {
        return false;
    }

    m_currentMap = meta;
    m_map->setMapMeta(meta);
    m_map->setBaseMapImage(img);
    m_map->render();

    appendLog(QString("[%1] [map] loaded yaml=%2 img=%3 (%4x%5) res=%6 origin=(%7,%8,%9)")
                  .arg(nowHMS(), yamlPath, meta.imageUrl)
                  .arg(img.width()).arg(img.height())
                  .arg(meta.resolution, 0, 'f', 3)
                  .arg(meta.originX, 0, 'f', 3)
                  .arg(meta.originY, 0, 'f', 3)
                  .arg(meta.originYaw, 0, 'f', 3));
    return true;
}
