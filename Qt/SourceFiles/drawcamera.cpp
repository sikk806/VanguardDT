#include "drawcamera.h"

#include <QLabel>
#include <QPixmap>

DrawCamera::DrawCamera(QLabel *cameraLabel, QObject *parent)
    : QObject(parent), m_label(cameraLabel)
{
    if (m_label) {
        m_label->setAlignment(Qt::AlignCenter);
    }
}

void DrawCamera::setImage(const QImage& img)
{
    if (!m_label) return;
    if (img.isNull()) return;

    m_label->setPixmap(QPixmap::fromImage(img).scaled(
        m_label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void DrawCamera::clear()
{
    if (!m_label) return;
    m_label->clear();
}
