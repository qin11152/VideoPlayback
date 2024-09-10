/********************************************************************************
** Form generated from reading UI file 'VideoPlayback.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VIDEOPLAYBACK_H
#define UI_VIDEOPLAYBACK_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "ui/OpenGLPreviewWidget/OpenGLPreviewWidget.h"

QT_BEGIN_NAMESPACE

class Ui_VideoPlaybackClass
{
public:
    QVBoxLayout *verticalLayout;
    OpenGLPreviewWidget *openGLWidget;
    QHBoxLayout *horizontalLayout_3;
    QSlider *videoTImeSlider;
    QLabel *timeCodeLabel;
    QHBoxLayout *horizontalLayout_4;
    QHBoxLayout *horizontalLayout;
    QPushButton *startPushButton;
    QPushButton *pausePushButton;
    QPushButton *continuePushButton;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *choosePushButton;
    QLabel *fileNameLabel;

    void setupUi(QWidget *VideoPlaybackClass)
    {
        if (VideoPlaybackClass->objectName().isEmpty())
            VideoPlaybackClass->setObjectName(QString::fromUtf8("VideoPlaybackClass"));
        VideoPlaybackClass->resize(924, 878);
        verticalLayout = new QVBoxLayout(VideoPlaybackClass);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        openGLWidget = new OpenGLPreviewWidget(VideoPlaybackClass);
        openGLWidget->setObjectName(QString::fromUtf8("openGLWidget"));

        verticalLayout->addWidget(openGLWidget);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setSpacing(6);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        videoTImeSlider = new QSlider(VideoPlaybackClass);
        videoTImeSlider->setObjectName(QString::fromUtf8("videoTImeSlider"));
        videoTImeSlider->setOrientation(Qt::Horizontal);

        horizontalLayout_3->addWidget(videoTImeSlider);

        timeCodeLabel = new QLabel(VideoPlaybackClass);
        timeCodeLabel->setObjectName(QString::fromUtf8("timeCodeLabel"));

        horizontalLayout_3->addWidget(timeCodeLabel);


        verticalLayout->addLayout(horizontalLayout_3);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setSpacing(6);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        startPushButton = new QPushButton(VideoPlaybackClass);
        startPushButton->setObjectName(QString::fromUtf8("startPushButton"));

        horizontalLayout->addWidget(startPushButton);

        pausePushButton = new QPushButton(VideoPlaybackClass);
        pausePushButton->setObjectName(QString::fromUtf8("pausePushButton"));

        horizontalLayout->addWidget(pausePushButton);

        continuePushButton = new QPushButton(VideoPlaybackClass);
        continuePushButton->setObjectName(QString::fromUtf8("continuePushButton"));

        horizontalLayout->addWidget(continuePushButton);


        horizontalLayout_4->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        choosePushButton = new QPushButton(VideoPlaybackClass);
        choosePushButton->setObjectName(QString::fromUtf8("choosePushButton"));

        horizontalLayout_2->addWidget(choosePushButton);

        fileNameLabel = new QLabel(VideoPlaybackClass);
        fileNameLabel->setObjectName(QString::fromUtf8("fileNameLabel"));

        horizontalLayout_2->addWidget(fileNameLabel);


        horizontalLayout_4->addLayout(horizontalLayout_2);


        verticalLayout->addLayout(horizontalLayout_4);

        verticalLayout->setStretch(0, 8);
        verticalLayout->setStretch(1, 1);
        verticalLayout->setStretch(2, 1);

        retranslateUi(VideoPlaybackClass);

        QMetaObject::connectSlotsByName(VideoPlaybackClass);
    } // setupUi

    void retranslateUi(QWidget *VideoPlaybackClass)
    {
        VideoPlaybackClass->setWindowTitle(QCoreApplication::translate("VideoPlaybackClass", "VideoPlayback", nullptr));
        timeCodeLabel->setText(QCoreApplication::translate("VideoPlaybackClass", "TextLabel", nullptr));
        startPushButton->setText(QCoreApplication::translate("VideoPlaybackClass", "\345\274\200\345\247\213", nullptr));
        pausePushButton->setText(QCoreApplication::translate("VideoPlaybackClass", "\346\232\202\345\201\234", nullptr));
        continuePushButton->setText(QCoreApplication::translate("VideoPlaybackClass", "\347\273\247\347\273\255", nullptr));
        choosePushButton->setText(QCoreApplication::translate("VideoPlaybackClass", "\351\200\211\346\213\251\346\226\207\344\273\266", nullptr));
        fileNameLabel->setText(QCoreApplication::translate("VideoPlaybackClass", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VideoPlaybackClass: public Ui_VideoPlaybackClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VIDEOPLAYBACK_H
