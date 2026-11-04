#ifndef DOCXFACTORY_WORD_PROCESSING_TEST_GUI_DFWORKER_H
#define DOCXFACTORY_WORD_PROCESSING_TEST_GUI_DFWORKER_H

#include <QObject>
#include <QString>
#include <QStringList>

// Owns every dfw_* call. The merger API is not documented as thread safe and
// keeps session state in globals, so all calls are serialized on one thread.
class DfWorker : public QObject
{
	Q_OBJECT

public:
	explicit DfWorker( QObject* p_parent = nullptr );

public slots:
	void compile( const QString& p_srcFile, const QString& p_dstFile );
	void queryTempDir();

	void loadTemplate( const QString& p_file );
	void closeTemplate();
	void mergeData( const QByteArray& p_data );
	void setClipboardValue( const QString& p_item, const QString& p_field,
							const QString& p_value, const QString& p_context );
	void pasteItem( const QString& p_item );
	void saveDocument( const QString& p_file );
	void queryItemFields( const QString& p_item );

signals:
	void compiled( bool p_ok, const QString& p_what, const QString& p_where );
	void tempDirQueried( const QString& p_dir );
	void loaded( bool p_ok, const QString& p_what, const QString& p_where,
				 const QStringList& p_items );
	void closed( bool p_ok, const QString& p_what, const QString& p_where );
	void merged( bool p_ok, const QString& p_what, const QString& p_where );
	void clipboardSet( bool p_ok, const QString& p_what, const QString& p_where,
					   const QString& p_context );
	void pasted( bool p_ok, const QString& p_what, const QString& p_where );
	void saved( bool p_ok, const QString& p_what, const QString& p_where );
	void itemFieldsQueried( const QString& p_item, const QStringList& p_fields );
};

#endif
