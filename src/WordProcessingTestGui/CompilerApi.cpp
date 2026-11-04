#include "CompilerApi.h"

#include "DocxFactory/WordProcessingCompiler/dfwc.h"

#include <QByteArray>

bool compilerApiCompile( const QString& p_srcFile, const QString& p_dstFile,
						 QString* p_what, QString* p_where )
{
	const QByteArray l_src = p_srcFile.toUtf8();
	const QByteArray l_dst = p_dstFile.toUtf8();

	dfwc_compile( l_src.constData(), l_dst.constData() );

	*p_what		= QString::fromUtf8( dfwc_getErrorWhat() );
	*p_where	= QString::fromUtf8( dfwc_getErrorWhere() );

	return !dfwc_getErrorFlag();
}

QString compilerApiTempDir()
{
	return QString::fromUtf8( dfwc_getTempDir() );
}
