#include "MyThread.h"
#include <QThread>

MyThread::MyThread(QObject *parent)
    : QThread(parent)
{
}

void MyThread::run()
{
    m_running = true;

    int value = 0;
    while (m_running && value <= 100) {
        emit progressChanged(value);
        ++value;
        QThread::msleep(50);
    }

    m_running = false;
}

void MyThread::stop()
{
    m_running = false;
}
