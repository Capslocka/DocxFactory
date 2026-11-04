#ifndef DOCXFACTORY_WORD_PROCESSING_TEST_GUI_TESTWINDOW_H
#define DOCXFACTORY_WORD_PROCESSING_TEST_GUI_TESTWINDOW_H

#include <QByteArray>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QThread;
class DfWorker;

class TestWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit TestWindow( QWidget* p_parent = nullptr );
	~TestWindow() override;

signals:
	// The GUI thread only emits these; DfWorker executes them on its own thread.
	void compileRequested( const QString& p_srcFile, const QString& p_dstFile );
	void tempDirRequested();
	void loadRequested( const QString& p_file );
	void closeRequested();
	void mergeRequested( const QByteArray& p_data );
	void clipboardValueRequested( const QString& p_item, const QString& p_field,
								  const QString& p_value, const QString& p_context );
	void pasteRequested( const QString& p_item );
	void saveRequested( const QString& p_file );
	void fieldsRequested( const QString& p_item );

private slots:
	// compiler tab
	void onBrowseSource();
	void onBrowseCompileTarget();
	void onCompile();

	// merger tab
	void onBrowseDfw();
	void onLoad();
	void onCloseTemplate();
	void onBrowseData();
	void onMerge();
	void onItemChanged( const QString& p_item );
	void onAddRow();
	void onApplyRows();
	void onRemoveRow();
	void onClearRows();
	void onPaste();
	void onBrowseSave();
	void onSave();

	// worker results
	void onCompiled( bool p_ok, const QString& p_what, const QString& p_where );
	void onTempDirQueried( const QString& p_dir );
	void onLoaded( bool p_ok, const QString& p_what, const QString& p_where,
				   const QStringList& p_items );
	void onClosed( bool p_ok, const QString& p_what, const QString& p_where );
	void onMerged( bool p_ok, const QString& p_what, const QString& p_where );
	void onClipboardSet( bool p_ok, const QString& p_what, const QString& p_where,
						 const QString& p_context );
	void onPasted( bool p_ok, const QString& p_what, const QString& p_where );
	void onSaved( bool p_ok, const QString& p_what, const QString& p_where );
	void onItemFieldsQueried( const QString& p_item, const QStringList& p_fields );

private:
	struct ValueRow
	{
		QString item;
		QString field;
		QString value;
	};

	void		createUi();
	QWidget*	createCompilerTab();
	QWidget*	createMergerTab();
	void		startWorker();

	void		setBusy( bool p_busy );
	void		updateActions();
	void		setSessionLoaded( bool p_loaded );
	void		invalidateSession();

	void		log( const QString& p_text );
	void		logError( const QString& p_what, const QString& p_where );
	void		logResult( bool p_ok, const QString& p_successText,
						   const QString& p_what, const QString& p_where );
	void		setStatus( const QString& p_text );

	void		deriveCompileTarget();
	void		deriveSaveFile();
	QStringList	visibleItems( const QStringList& p_items ) const;
	bool		isPasteable( const QString& p_item ) const;
	bool		confirmOverwrite( const QString& p_file );
	void		applyNextRow();
	QString		cellText( int p_row, int p_column ) const;

	QThread*			m_thread;
	DfWorker*			m_worker;
	bool				m_busy;
	bool				m_loaded;
	bool				m_compileTargetAuto;
	bool				m_saveFileAuto;
	QString				m_loadedFile;
	QVector<ValueRow>	m_applyQueue;

	QTabWidget*			m_tabs;
	QPlainTextEdit*		m_log;
	QLabel*				m_sessionLabel;

	// compiler tab
	QLineEdit*			m_srcFile;
	QLineEdit*			m_compileTarget;
	QLineEdit*			m_tempDir;
	QPushButton*		m_compileButton;

	// merger tab
	QLineEdit*			m_dfwFile;
	QPushButton*		m_loadButton;
	QPushButton*		m_closeButton;
	QLineEdit*			m_dataFile;
	QPushButton*		m_dataButton;
	QComboBox*			m_itemCombo;
	QComboBox*			m_fieldCombo;
	QLineEdit*			m_valueEdit;
	QPushButton*		m_addRowButton;
	QTableWidget*		m_valueTable;
	QPushButton*		m_applyButton;
	QPushButton*		m_removeRowButton;
	QPushButton*		m_clearRowsButton;
	QPushButton*		m_pasteButton;
	QLineEdit*			m_saveFile;
	QPushButton*		m_saveButton;
};

#endif
