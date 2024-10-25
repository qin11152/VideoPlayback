#pragma once

#include <QObject>

class MyTipDialog  : public QObject
{
	Q_OBJECT

public:
	static void show(const QString& title, const QString& text, bool modal = true, int timeout = 0);
	~MyTipDialog();

private:
	MyTipDialog(QObject* parent = nullptr); // 私有构造函数，防止实例化

	static void showModal(const QString& title, const QString& text);
	static void showNonModal(const QString& title, const QString& text, int timeout);
};
