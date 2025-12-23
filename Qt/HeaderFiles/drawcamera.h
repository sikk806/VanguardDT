#pragma once
/**
 * @file drawcamera.h
 * @brief camera QLabel에 QImage를 띄우는 작은 헬퍼 (UI 의존 최소화)
 */

#include <QObject>
#include <QImage>

class QLabel;

class DrawCamera final : public QObject
{
    Q_OBJECT
public:
    explicit DrawCamera(QLabel *cameraLabel, QObject *parent = nullptr);

    void setImage(const QImage& img);
    void clear();

private:
    QLabel *m_label = nullptr;
};
