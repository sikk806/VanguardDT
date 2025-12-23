#include "workers.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QSslError>
#include <QEventLoop>

#include <QPainter>
#include <QPen>

#include <cmath>

static QString nowHMS() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

// ---------------- LogWorker ----------------
LogWorker::LogWorker(QObject *parent)
    : QObject(parent)
{
}

void LogWorker::setLogFile(const QString& filePath)
{
    m_path = filePath;

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    if (m_path.isEmpty()) {
        emit logLine(QString("[%1] [log] empty path, file logging disabled").arg(nowHMS()));
        return;
    }

    QDir().mkpath(QFileInfo(m_path).absolutePath());

    m_file = new QFile(m_path);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        emit logLine(QString("[%1] [log] open fail: %2").arg(nowHMS(), m_path));
        delete m_file;
        m_file = nullptr;
        return;
    }

    emit logLine(QString("[%1] [log] file=%2").arg(nowHMS(), m_path));
}

void LogWorker::appendLine(const QString& line)
{
    if (!m_file) return;

    QTextStream ts(m_file);
    ts << line << "\n";
    // 너무 잦은 flush는 느릴 수 있으니 필요할 때만 flush하도록(여긴 기본 off)
}

void LogWorker::flush()
{
    if (!m_file) return;
    m_file->flush();
}

// ---------------- MotorWorker ----------------
MotorWorker::MotorWorker(QObject *parent)
    : QObject(parent)
{
}

void MotorWorker::onMotorCmd(const MotorCmd& cmd)
{
    if (cmd.speed != m_lastSpeed) {
        m_lastSpeed = cmd.speed;
        emit speedChanged(cmd.speed);
    }
    if (cmd.steer != m_lastSteer) {
        m_lastSteer = cmd.steer;
        emit steerChanged(cmd.steer);
    }
}

// ---------------- CameraWorker ----------------
CameraWorker::CameraWorker(QObject *parent)
    : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);

    m_timer = new QTimer(this);
    m_timer->setInterval(80);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &CameraWorker::processPrefetchQueue);
}

void CameraWorker::prefetchShotThumb(const Shot& shot)
{
    const QString url = shot.thumbUrl;
    if (url.isEmpty()) return;
    if (m_cache.contains(url)) return;

    // queue 폭주 방지
    if (m_queue.size() > 50) return;

    m_queue.enqueue(url);
    if (!m_timer->isActive()) m_timer->start();
}

void CameraWorker::onShotClicked(const Shot& shot)
{
    const QString url = !shot.imageUrl.isEmpty() ? shot.imageUrl : shot.thumbUrl;
    if (url.isEmpty()) {
        emit logLine(QString("[%1] [camera] empty url").arg(nowHMS()));
        return;
    }

    emit logLine(QString("[%1] [camera] load: %2").arg(nowHMS(), url));

    const QImage img = fetchImageSync(url, 12000);
    if (img.isNull()) {
        emit logLine(QString("[%1] [camera] load fail: %2").arg(nowHMS(), url));
        return;
    }

    emit cameraReady(img);
}

void CameraWorker::processPrefetchQueue()
{
    if (m_queue.isEmpty()) {
        m_timer->stop();
        return;
    }

    const QString url = m_queue.dequeue();
    if (url.isEmpty() || m_cache.contains(url)) return;

    const QImage img = fetchImageSync(url, 8000);
    if (!img.isNull()) {
        m_cache.insert(url, img);
    }

    if (m_queue.isEmpty()) m_timer->stop();
}

QImage CameraWorker::fetchImageSync(const QString& url, int timeoutMs)
{
    if (url.isEmpty()) return QImage();

    if (m_cache.contains(url)) return m_cache.value(url);

    QUrl qurl(url);

    // 로컬 파일 경로 지원(예: yaml 로드에서 imageUrl에 로컬 경로 넣는 케이스)
    if (!qurl.isValid() || qurl.scheme().isEmpty() || qurl.isLocalFile()) {
        const QString path = qurl.isLocalFile() ? qurl.toLocalFile() : url;
        QImage img;
        img.load(path);
        if (!img.isNull()) m_cache.insert(url, img);
        return img;
    }

    QNetworkRequest req(qurl);
    req.setHeader(QNetworkRequest::UserAgentHeader, "qt_gui");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_net->get(req);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    connect(&timeout, &QTimer::timeout, &loop, [&](){
        if (reply) reply->abort();
        loop.quit();
    });

    connect(reply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError>& errs){
        for (const auto& e : errs) emit logLine(QString("[%1] [http][ssl] %2").arg(nowHMS(), e.errorString()));
    });

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timeout.start(timeoutMs);
    loop.exec();

    const auto err = reply->error();
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (err != QNetworkReply::NoError) return QImage();

    QImage img;
    if (!img.loadFromData(data)) return QImage();

    m_cache.insert(url, img);
    return img;
}

// ---------------- MapRenderWorker ----------------
MapRenderWorker::MapRenderWorker(QObject *parent)
    : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(20);
    connect(m_renderTimer, &QTimer::timeout, this, &MapRenderWorker::renderNow);
}

void MapRenderWorker::onMapState(const MapMeta& meta)
{
    const bool mapChanged = (m_meta.mapId != meta.mapId);

    m_meta = meta;
    if (mapChanged) {
        m_shots.clear();
        m_hasPose = false;
    }

    if (meta.imageUrl.isEmpty()) {
        emit logLine(QString("[%1] [map] empty image_url (map_id=%2)").arg(nowHMS(), meta.mapId));
        m_base = QImage();
        scheduleRender();
        return;
    }

    // base map image 로드(캐시)
    m_base = fetchImageSync(meta.imageUrl, 15000);
    if (m_base.isNull()) {
        emit logLine(QString("[%1] [map] image load fail: %2").arg(nowHMS(), meta.imageUrl));
    } else {
        emit logLine(QString("[%1] [map] image ok: %2 (%3x%4)")
                     .arg(nowHMS(), meta.imageUrl)
                     .arg(m_base.width()).arg(m_base.height()));
    }

    scheduleRender();
}

void MapRenderWorker::onCameraShot(const Shot& shot)
{
    if (!m_meta.mapId.isEmpty() && !shot.mapId.isEmpty() && m_meta.mapId != shot.mapId) {
        emit logLine(QString("[%1] [map] ignore shot(map mismatch) shot.map_id=%2 current=%3")
                     .arg(nowHMS(), shot.mapId, m_meta.mapId));
        return;
    }

    m_shots.push_back(shot);

    // 폭주 방지 (원하면 더 크게)
    if (m_shots.size() > 5000) {
        m_shots.remove(0);
    }

    scheduleRender();
}

void MapRenderWorker::onPose(const Pose& pose)
{
    if (!m_meta.mapId.isEmpty() && !pose.mapId.isEmpty() && m_meta.mapId != pose.mapId) {
        return;
    }

    m_pose = pose;
    m_hasPose = true;
    scheduleRender();
}

void MapRenderWorker::scheduleRender()
{
    if (!m_renderTimer) return;
    if (!m_renderTimer->isActive()) m_renderTimer->start();
}

QPoint MapRenderWorker::worldToPixel(const MapMeta& meta, const QImage& img, double x, double y) const
{
    if (meta.resolution <= 0.0) return QPoint(-1, -1);
    if (img.isNull()) return QPoint(-1, -1);

    const double cx = (x - meta.originX) / meta.resolution;
    const double cy = (y - meta.originY) / meta.resolution;

    const int imgW = img.width();
    const int imgH = img.height();

    const double gridW = (meta.widthCells  > 0) ? meta.widthCells  : imgW;
    const double gridH = (meta.heightCells > 0) ? meta.heightCells : imgH;

    const double scaleX = imgW / gridW;
    const double scaleY = imgH / gridH;

    const int px = int(cx * scaleX);
    const int py = int((gridH - 1.0 - cy) * scaleY);

    return QPoint(px, py);
}

void MapRenderWorker::renderNow()
{
    if (m_base.isNull()) return;

    QImage draw = m_base.convertToFormat(QImage::Format_ARGB32);
    QPainter p(&draw);
    p.fillRect(draw.rect(), QColor(0, 0, 0, 55));

    p.setRenderHint(QPainter::Antialiasing, true);

    QVector<HitBox> hitboxes;
    hitboxes.reserve(m_shots.size());

    const int r = 4;

    // shots
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 0, 0, 255));

    for (int i = 0; i < m_shots.size(); ++i) {
        const Shot& s = m_shots[i];

        QPoint pt = worldToPixel(m_meta, draw, s.x, s.y);
        if (pt.x() < 0 || pt.y() < 0 || pt.x() >= draw.width() || pt.y() >= draw.height())
            continue;

        QRect rc(pt.x() - r, pt.y() - r, 2*r, 2*r);
        p.drawEllipse(rc);

        hitboxes.push_back(HitBox{rc, i});
    }

    // robot pose
    if (m_hasPose) {
        QPoint pt = worldToPixel(m_meta, draw, m_pose.x, m_pose.y);
        if (pt.x() >= 0 && pt.y() >= 0 && pt.x() < draw.width() && pt.y() < draw.height()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 200, 255, 220));
            p.drawEllipse(QRect(pt.x()-5, pt.y()-5, 10, 10));

            p.setPen(QPen(QColor(0, 200, 255, 220), 2));
            const double len = 18.0;
            const double dx = len * std::cos(-m_pose.theta);
            const double dy = len * std::sin(-m_pose.theta);
            p.drawLine(QPointF(pt), QPointF(pt.x() + dx, pt.y() + dy));
        }
    }

    emit mapImageRendered(draw, hitboxes);
}

QImage MapRenderWorker::loadLocalImage(const QString& path)
{
    QImage img;
    img.load(path);
    return img;
}

QImage MapRenderWorker::fetchImageSync(const QString& url, int timeoutMs)
{
    if (url.isEmpty()) return QImage();

    if (m_cache.contains(url)) return m_cache.value(url);

    QUrl qurl(url);
    if (!qurl.isValid() || qurl.scheme().isEmpty() || qurl.isLocalFile()) {
        const QString path = qurl.isLocalFile() ? qurl.toLocalFile() : url;
        const QImage img = loadLocalImage(path);
        if (!img.isNull()) m_cache.insert(url, img);
        return img;
    }

    QNetworkRequest req(qurl);
    req.setHeader(QNetworkRequest::UserAgentHeader, "qt_gui");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_net->get(req);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    connect(&timeout, &QTimer::timeout, &loop, [&](){
        if (reply) reply->abort();
        loop.quit();
    });

    connect(reply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError>& errs){
        for (const auto& e : errs) emit logLine(QString("[%1] [http][ssl] %2").arg(nowHMS(), e.errorString()));
    });

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timeout.start(timeoutMs);
    loop.exec();

    const auto err = reply->error();
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (err != QNetworkReply::NoError) return QImage();

    QImage img;
    if (!img.loadFromData(data)) return QImage();

    m_cache.insert(url, img);
    return img;
}
