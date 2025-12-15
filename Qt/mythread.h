#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <QThread>

class MyThread : public QThread
{
    Q_OBJECT

public:
    explicit MyThread(QObject *parent = nullptr);
    void stop();

signals:
    void progressChanged(int value);

protected:
    void run() override;

private:
    bool m_running = false;
};

#endif // MYTHREAD_H
