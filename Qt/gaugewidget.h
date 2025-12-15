#ifndef GAUGEWIDGET_H
#define GAUGEWIDGET_H

#include <QWidget>
#include <QPropertyAnimation>

class GaugeWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int displayValue READ displayValue WRITE setDisplayValue NOTIFY displayValueChanged)

public:
    explicit GaugeWidget(QWidget *parent = nullptr);

    int value() const { return m_targetValue; }          // “목표값”
    int displayValue() const { return m_displayValue; }  // “화면에 그리는 값”

    void setRange(int minV, int maxV);
    void setTitle(const QString& t) { m_title = t; update(); }
    void setUnit(const QString& u) { m_unit = u; update(); }
    void setAnimated(bool on) { m_animated = on; }

public slots:
    void setValue(int v);          // 외부에서 부르는 함수 (목표값 세팅)
    void setDisplayValue(int v);   // 애니메이션이 건드리는 setter (절대 애니메이션 재시작 X)

signals:
    void displayValueChanged(int);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    int m_min = 0;
    int m_max = 100;

    int m_displayValue = 0;  // 화면 표시용
    int m_targetValue  = 0;  // 목표값

    QString m_title = "GAUGE";
    QString m_unit  = "";

    bool m_animated = true;
    QPropertyAnimation *m_anim = nullptr;

    int clamp(int v) const;
    double ratio() const;
};

#endif
