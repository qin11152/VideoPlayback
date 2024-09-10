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
    m_bPressed = true;
    int currentX = event->pos().x();

    //获取当前点击的位置占整个Slider的百分比
    double per = currentX *1.0 /this->width();

    //利用算得的百分比得到具体数字
    int value = per*(this->maximum() - this->minimum()) + this->minimum();
    //设定滑动条位置
    qDebug()<<"set value"<<value;
    this->setValue(value);

    emit signalSliderValueChanged(per*100);

    //滑动条移动事件等事件也用到了mousePressEvent,加这句话是为了不对其产生影响，是的Slider能正常相应其他鼠标事件
    QSlider::mousePressEvent(event);

}

void MySlider::mouseReleaseEvent(QMouseEvent *event)
{
    m_bPressed=false;
}

void MySlider::mouseMoveEvent(QMouseEvent *event)
{
}
