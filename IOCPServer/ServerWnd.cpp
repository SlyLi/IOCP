#include "ServerWnd.h"
#include "IOCPServer.h"
#pragma execution_character_set("utf-8")

ServerWnd::ServerWnd(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	ui.stopServer->setDisabled(true);
	//list=new QListWidget(this);
}

void ServerWnd::ShowStatusMessage(std::wstring str)
{
	//ui.listWidget->addItem(QString::fromStdWString(str));
	//ui.listWidget->scrollToBottom();
	ui.statusBar->showMessage(QString::fromStdWString(str));
}

void ServerWnd::ShowStatusMessage(std::wstring str, int time_out)
{
	ui.statusBar->showMessage(QString::fromStdWString(str),time_out);
}

void ServerWnd::BindIOCPServer(IOCPServer * s)
{
	
	server = s;
}

void ServerWnd::slotStartServer()
{
	ui.startServer->setEnabled(false);
	ui.stopServer->setEnabled(true);
	server->StartServer();
	/*for (int i = 0; i < 20; i++)
	{
		server->StartServer();
		server->StopServer();
	}
	*/
}
void ServerWnd::slotStopServer()
{
	ui.startServer->setEnabled(true);
	ui.stopServer->setEnabled(false);
	server->StopServer();

	/*for (int i = 0; i < 20; i++)
	{
		server->StartServer();
		server->StopServer();
	}
*/

}