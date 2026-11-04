#ifndef DOCXFACTORY_WORD_PROCESSING_TEST_GUI_COMPILERAPI_H
#define DOCXFACTORY_WORD_PROCESSING_TEST_GUI_COMPILERAPI_H

#include <QString>

// dfwc.h and dfw.h use the same include guard, so the compiler entry points
// live in their own translation unit to keep both headers usable.
bool	compilerApiCompile( const QString& p_srcFile, const QString& p_dstFile,
							QString* p_what, QString* p_where );
QString	compilerApiTempDir();

#endif
