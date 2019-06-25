#include <QtWidgets/QApplication>
#include "ServerWnd.h"
#include "IOCPServer.h"


int main(int argc, char *argv[])
{


	QApplication a(argc, argv);
	ServerWnd w;
	IOCPServer server;
	server.BingMainWnd(&w);
	w.BindIOCPServer(&server);
	w.show();
	
	return a.exec();
	//IOCPServer server;
	//server.StartServer();
	//server.StopServer();
//	_CrtDumpMemoryLeaks();
	return 0;
}
