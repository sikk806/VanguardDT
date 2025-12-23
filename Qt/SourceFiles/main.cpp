#include "mainwindow.h"

#include <QApplication>
#include <QMetaType>
#include <QFont>
#include <QStyleFactory>

#include "app_types.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setStyle(QStyleFactory::create("Fusion"));

    QFont f("Inter");
    f.setStyleHint(QFont::SansSerif);
    f.setPointSize(10);
    a.setFont(f);
    a.setStyleSheet(R"(
        QMainWindow, QWidget#centralwidget {
            background: #212121;
            color: #e6e6e6;
            font-family: "Inter","Segoe UI","Noto Sans","Arial";
            font-size: 12px;
        }

        QLabel { color: #e6e6e6;
            font-weight: 600;
        }

        QLabel#mapImage, QLabel#cameraImage {
            background: #212121;
            border: 1px solid #20242c;
            border-radius: 10px;
            padding: 6px;
        }

        QWidget#veloGraph {
            background: #212121;
            border: 1px solid #20242c;
            border-radius: 10px;
        }

        QPlainTextEdit {
            background: #292929;
            color: #e6e6e6;
            border: 1px solid #20242c;
            border-radius: 10px;
            selection-background-color: #292929;
        }

        QPushButton {
            background: #474747;
            color: #e6e6e6;
            border: 1px solid #242a33;
            border-radius: 8px;
            padding: 6px 14px;
            font-weight: 600;
        }
        QPushButton:hover { background: #7A7A7A; }
        QPushButton:pressed { background: #2B2B2B; }
        QPushButton:disabled {
            color: #6b7280;
            background: #0e1116;
            border-color: #1a1f27;
        }

        QStatusBar {
            background: #0b0d10;
            color: #9aa3af;
            border-top: 1px solid #1c212a;
        }
    )");

    // QueuedConnection에서 struct를 쓰기 위한 등록
    qRegisterMetaType<Shot>("Shot");
    qRegisterMetaType<MapMeta>("MapMeta");
    qRegisterMetaType<Pose>("Pose");
    qRegisterMetaType<MotorCmd>("MotorCmd");
    qRegisterMetaType<HitBox>("HitBox");
    qRegisterMetaType<QVector<HitBox>>("QVector<HitBox>");

    MainWindow w;
    w.show();

    return a.exec();
}
