#include "ClientWnd.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	ClientWnd w;
	w.show();
	return a.exec();
}
