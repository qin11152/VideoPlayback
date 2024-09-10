#pragma once

#include <QSlider>

class MySlider : public QSlider
{
    Q_OBJECT
public:

    MySlider(QWidget *parent = nullptr);

    ~MySlider();

    bool getPressed() const
    {
        return m_bPressed;
    }

protected:
    void mousePressEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;
signals:
    void signalSliderMoved(int value);

    void signalSliderPressed();

    void signalSliderReleased();

    void signalSliderValueChanged(double value);

private:
    bool m_bPressed = false;
    bool m_bMoved = false;
    int m_iValue = 0;
};