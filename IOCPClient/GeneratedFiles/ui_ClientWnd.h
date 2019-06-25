/********************************************************************************
** Form generated from reading UI file 'ClientWnd.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CLIENTWND_H
#define UI_CLIENTWND_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ClientWndClass
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *ClientWndClass)
    {
        if (ClientWndClass->objectName().isEmpty())
            ClientWndClass->setObjectName(QString::fromUtf8("ClientWndClass"));
        ClientWndClass->resize(600, 400);
        menuBar = new QMenuBar(ClientWndClass);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        ClientWndClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(ClientWndClass);
        mainToolBar->setObjectName(QString::fromUtf8("mainToolBar"));
        ClientWndClass->addToolBar(mainToolBar);
        centralWidget = new QWidget(ClientWndClass);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        ClientWndClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(ClientWndClass);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        ClientWndClass->setStatusBar(statusBar);

        retranslateUi(ClientWndClass);

        QMetaObject::connectSlotsByName(ClientWndClass);
    } // setupUi

    void retranslateUi(QMainWindow *ClientWndClass)
    {
        ClientWndClass->setWindowTitle(QApplication::translate("ClientWndClass", "ClientWnd", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ClientWndClass: public Ui_ClientWndClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CLIENTWND_H
