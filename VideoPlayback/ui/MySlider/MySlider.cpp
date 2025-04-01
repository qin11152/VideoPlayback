#include "MySlider.h"
#include <QDebug>
#include <QMouseEvent>

MySlider::MySlider(QWidget *parent)
	: QSlider(parent)
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

    //��ȡ��ǰ�����λ��ռ����Slider�İٷֱ�
    double per = currentX *1.0 /this->width();

    //������õİٷֱȵõ���������
    double value = per*(this->maximum() - this->minimum()) + this->minimum();
    
    //valueתΪint����������
    value = qRound(value);
    qDebug() << "set value" << value;
    //�趨������λ��
    this->setValue(value);

    //emit signalSliderValueChanged(value);
}

void MySlider::mouseReleaseEvent(QMouseEvent *event)
{
    m_bPressed=false;
	if (event->button() != Qt::LeftButton)
	{
		return;
	}
	qDebug() << "mousePressEvent";
	m_bPressed = true;
	int currentX = event->pos().x();

	//��ȡ��ǰ�����λ��ռ����Slider�İٷֱ�
	double per = currentX * 1.0 / this->width();

	//������õİٷֱȵõ���������
	double value = per * (this->maximum() - this->minimum()) + this->minimum();

	//valueתΪint����������
	value = qRound(value);
	qDebug() << "set value" << value;
	//�趨������λ��
	this->setValue(value);

	emit signalSliderValueChanged(value);
}

void MySlider::mouseMoveEvent(QMouseEvent *event)
{
    //�ƶ���ʱ������µ�ǰ��ֵ�����õ���������,��Ҫ̫Ƶ���Ĵ���
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
