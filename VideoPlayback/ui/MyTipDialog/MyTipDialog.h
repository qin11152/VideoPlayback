#pragma once

#include <QObject>

class MyTipDialog  : public QObject
{
	Q_OBJECT

public:
	static void show(const QString& title, const QString& text, bool modal = true, int timeout = 0);
	~MyTipDialog();

private:
	MyTipDialog(QObject* parent = nullptr); // ˽�й��캯������ֹʵ����

	static void showModal(const QString& title, const QString& text);
	static void showNonModal(const QString& title, const QString& text, int timeout);
};
