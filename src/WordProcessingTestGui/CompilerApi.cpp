#include "CompilerApi.h"

#include "LibStr.h"

#include "DocxFactory/WordProcessingCompiler/dfwc.h"

#include <QByteArray>

bool compilerApiCompile( const QString& p_srcFile, const QString& p_dstFile,
						 QString* p_what, QString* p_where )
{
	const QByteArray l_src = toLibPath( p_srcFile );
	const QByteArray l_dst = toLibPath( p_dstFile );

	dfwc_compile( l_src.constData(), l_dst.constData() );

	*p_what		= fromLibStr( dfwc_getErrorWhat() );
	*p_where	= fromLibStr( dfwc_getErrorWhere() );

	return !dfwc_getErrorFlag();
}

QString compilerApiTempDir()
{
	return fromLibStr( dfwc_getTempDir() );
}
