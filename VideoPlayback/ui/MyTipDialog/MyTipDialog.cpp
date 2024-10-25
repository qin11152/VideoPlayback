#include "MyTipDialog.h"

#include <QTimer>
#include <QMessageBox>
#include <QApplication>

void MyTipDialog::show(const QString& title, const QString& text, bool modal /*= true*/, int timeout /*= 0*/)
{
	if (modal) 
	{
		showModal(title, text);
	}
	else
	{
		showNonModal(title, text, timeout);
	}
}

MyTipDialog::MyTipDialog(QObject* parent)
	: QObject(parent)
{}

void MyTipDialog::showModal(const QString& title, const QString& text)
{
	QMessageBox msgBox(QMessageBox::Information, title, text, QMessageBox::Ok, QApplication::activeWindow());
	msgBox.setWindowModality(Qt::WindowModal);
	msgBox.exec();
}

void MyTipDialog::showNonModal(const QString& title, const QString& text, int timeout)
{
	QMessageBox* msgBox = new QMessageBox(QMessageBox::Information, title, text, QMessageBox::Ok, QApplication::activeWindow());
	msgBox->setAttribute(Qt::WA_DeleteOnClose); // ȷ���ڹر�ʱɾ��

	QTimer::singleShot(timeout, msgBox, &QMessageBox::accept); // ���ó�ʱ

	msgBox->show();
}

MyTipDialog::~MyTipDialog()
{}