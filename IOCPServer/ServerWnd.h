#pragma once
#include <QtWidgets/QMainWindow>
#include "ui_ServerWnd.h"
#include <iostream>
#include <vector>
#include <QStandardItemModel>

class IOCPServer;

class ServerWnd : public QMainWindow
{
	Q_OBJECT

public:
	ServerWnd(QWidget *parent = Q_NULLPTR);
	
	void ShowStatusMessage(std::wstring str);	//显示信息到statusbar
	void ShowStatusMessage(std::wstring str,int time_out);
	void BindIOCPServer(IOCPServer* server);
public slots:
	void slotStartServer();
	void slotStopServer();
	
private:
	IOCPServer* server;
	Ui::ServerWndClass ui;
	QListWidget *list;
	
};
