#include <QApplication>

#include "TestWindow.h"

int main( int p_argc, char* p_argv[] )
{
	QCoreApplication::setAttribute( Qt::AA_EnableHighDpiScaling );

	QApplication	l_app( p_argc, p_argv );
	TestWindow		l_window;

	l_window.show();

	return l_app.exec();
}
