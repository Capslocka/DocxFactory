#ifndef DOCXFACTORY_WORD_PROCESSING_TEST_GUI_LIBSTR_H
#define DOCXFACTORY_WORD_PROCESSING_TEST_GUI_LIBSTR_H

#include <QByteArray>
#include <QString>

// DocxFactory opens files through the narrow C runtime and the *A Win32 APIs,
// so file paths must be in the ANSI code page on Windows. Item names, field
// names and values are document text and stay UTF-8.
inline QByteArray toLibPath( const QString& p_path )
{
#if defined( Q_OS_WIN )
	return p_path.toLocal8Bit();
#else
	return p_path.toUtf8();
#endif
}

// Engine strings are a mix of paths (ANSI on Windows) and document text
// (UTF-8), so fall back to the local code page only when the bytes are not
// valid UTF-8.
inline QString fromLibStr( const char* p_str )
{
	if ( !p_str )
		return QString();

	const QByteArray	l_bytes( p_str );
	const QString		l_utf8 = QString::fromUtf8( l_bytes );

	if ( l_utf8.toUtf8() == l_bytes )
		return l_utf8;

	return QString::fromLocal8Bit( l_bytes );
}

#endif
