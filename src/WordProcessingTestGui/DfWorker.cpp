#include "DfWorker.h"

#include "CompilerApi.h"

#include "DocxFactory/WordProcessingMerger/dfw.h"

#include <QByteArray>
#include <QStringList>

namespace
{

QString			lastWhat()	{ return QString::fromUtf8( dfw_getErrorWhat() ); }
QString			lastWhere()	{ return QString::fromUtf8( dfw_getErrorWhere() ); }

QStringList		splitNames( const QString& p_names )
{
	if ( p_names.isEmpty() )
		return QStringList();

	return p_names.split( ',', Qt::SkipEmptyParts );
}

QStringList		itemNames()
{
	// the API returns a static buffer that the next call overwrites
	return splitNames( QString::fromUtf8( dfw_getItems() ) );
}

} // namespace

DfWorker::DfWorker( QObject* p_parent )
	: QObject( p_parent )
{
} // c'tor



// compiler

void DfWorker::compile( const QString& p_srcFile, const QString& p_dstFile )
{
	QString l_what;
	QString l_where;

	const bool l_ok = compilerApiCompile( p_srcFile, p_dstFile, &l_what, &l_where );

	emit compiled( l_ok, l_what, l_where );
} // compile

void DfWorker::queryTempDir()
{
	emit tempDirQueried( compilerApiTempDir() );
} // queryTempDir



// merger

void DfWorker::loadTemplate( const QString& p_file )
{
	const QByteArray l_file = p_file.toUtf8();

	dfw_load( l_file.constData() );

	if ( dfw_getErrorFlag() )
	{
		emit loaded( false, lastWhat(), lastWhere(), QStringList() );
		return;
	}

	// values set through dfw_setClipboardValueByStr are converted from the
	// session code page, so the GUI can hand over UTF-8 strings directly
	dfw_setCodePage( "UTF-8" );

	if ( dfw_getErrorFlag() )
	{
		emit loaded( false, lastWhat(), lastWhere(), QStringList() );
		return;
	}

	const QStringList l_items = itemNames();

	if ( dfw_getErrorFlag() )
	{
		emit loaded( false, lastWhat(), lastWhere(), QStringList() );
		return;
	}

	emit loaded( true, QString(), QString(), l_items );
} // loadTemplate

void DfWorker::closeTemplate()
{
	dfw_close();

	emit closed( !dfw_getErrorFlag(), lastWhat(), lastWhere() );
} // closeTemplate

void DfWorker::mergeData( const QByteArray& p_data )
{
	dfw_merge( p_data.constData() );

	emit merged( !dfw_getErrorFlag(), lastWhat(), lastWhere() );
} // mergeData

void DfWorker::setClipboardValue( const QString& p_item, const QString& p_field,
								  const QString& p_value, const QString& p_context )
{
	const QByteArray l_item	= p_item.toUtf8();
	const QByteArray l_field	= p_field.toUtf8();
	const QByteArray l_value	= p_value.toUtf8();

	dfw_setClipboardValueByStr( l_item.constData(), l_field.constData(), l_value.constData() );

	emit clipboardSet( !dfw_getErrorFlag(), lastWhat(), lastWhere(), p_context );
} // setClipboardValue

void DfWorker::pasteItem( const QString& p_item )
{
	const QByteArray l_item = p_item.toUtf8();

	dfw_paste( l_item.constData() );

	emit pasted( !dfw_getErrorFlag(), lastWhat(), lastWhere() );
} // pasteItem

void DfWorker::saveDocument( const QString& p_file )
{
	const QByteArray l_file = p_file.toUtf8();

	dfw_save( l_file.constData() );

	emit saved( !dfw_getErrorFlag(), lastWhat(), lastWhere() );
} // saveDocument

void DfWorker::queryItemFields( const QString& p_item )
{
	const QByteArray	l_item = p_item.toUtf8();
	const QString		l_fields = QString::fromUtf8( dfw_getItemFields( l_item.constData() ) );

	emit itemFieldsQueried( p_item, splitNames( l_fields ) );
} // queryItemFields
