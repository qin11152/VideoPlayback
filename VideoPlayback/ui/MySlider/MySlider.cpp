#include "MySlider.h"
#include <QDebug>
#include <QMouseEvent>

MySlider::MySlider(QWidget *parent)
{
}

MySlider::~MySlider()
{
}

void MySlider::mousePressEvent(QMouseEvent *event)
{
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    qDebug()<<"mousePressEvent";
    m_bPressed = true;
    int currentX = event->pos().x();

    //获取当前点击的位置占整个Slider的百分比
    double per = currentX *1.0 /this->width();

    //利用算得的百分比得到具体数字
    double value = per*(this->maximum() - this->minimum()) + this->minimum();
    
    //value转为int，四舍五入
    value = qRound(value);

    //设定滑动条位置
    qDebug()<<"set value"<<value;
    this->setValue(value);

    emit signalSliderValueChanged(per*100);
}

void MySlider::mouseReleaseEvent(QMouseEvent *event)
{
    m_bPressed=false;
}

void MySlider::mouseMoveEvent(QMouseEvent *event)
{
    //移动的时候计算下当前的值，设置到滑动条上,不要太频繁的触发
    if(m_bPressed)
    {
        int currentX = event->pos().x();
        m_iLastMoveValue = currentX;
        double per = currentX *1.0 /this->width();
        int value = per*(this->maximum() - this->minimum()) + this->minimum();
        this->setValue(value);
        qDebug()<<"set value"<<value;
        emit signalSliderValueChanged(per*100);
    }
}
