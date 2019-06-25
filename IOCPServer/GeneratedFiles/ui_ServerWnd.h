/********************************************************************************
** Form generated from reading UI file 'ServerWnd.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SERVERWND_H
#define UI_SERVERWND_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ServerWndClass
{
public:
    QWidget *centralWidget;
    QPushButton *startServer;
    QPushButton *stopServer;
    QListWidget *listWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *ServerWndClass)
    {
        if (ServerWndClass->objectName().isEmpty())
            ServerWndClass->setObjectName(QString::fromUtf8("ServerWndClass"));
        ServerWndClass->resize(624, 551);
        centralWidget = new QWidget(ServerWndClass);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        startServer = new QPushButton(centralWidget);
        startServer->setObjectName(QString::fromUtf8("startServer"));
        startServer->setGeometry(QRect(110, 480, 111, 41));
        stopServer = new QPushButton(centralWidget);
        stopServer->setObjectName(QString::fromUtf8("stopServer"));
        stopServer->setGeometry(QRect(330, 480, 121, 41));
        listWidget = new QListWidget(centralWidget);
        listWidget->setObjectName(QString::fromUtf8("listWidget"));
        listWidget->setGeometry(QRect(10, 10, 601, 451));
        ServerWndClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(ServerWndClass);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        statusBar->setSizeGripEnabled(true);
        ServerWndClass->setStatusBar(statusBar);

        retranslateUi(ServerWndClass);
        QObject::connect(startServer, SIGNAL(clicked()), ServerWndClass, SLOT(slotStartServer()));
        QObject::connect(stopServer, SIGNAL(clicked()), ServerWndClass, SLOT(slotStopServer()));

        QMetaObject::connectSlotsByName(ServerWndClass);
    } // setupUi

    void retranslateUi(QMainWindow *ServerWndClass)
    {
        ServerWndClass->setWindowTitle(QApplication::translate("ServerWndClass", "ServerWnd", nullptr));
        startServer->setText(QApplication::translate("ServerWndClass", "\345\274\200\345\220\257\346\234\215\345\212\241\345\231\250", nullptr));
        stopServer->setText(QApplication::translate("ServerWndClass", "\345\205\263\351\227\255\346\234\215\345\212\241\345\231\250", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ServerWndClass: public Ui_ServerWndClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SERVERWND_H
