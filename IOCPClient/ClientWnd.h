#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ClientWnd.h"

class ClientWnd : public QMainWindow
{
	Q_OBJECT

public:
	ClientWnd(QWidget *parent = Q_NULLPTR);

private:
	Ui::ClientWndClass ui;
};
