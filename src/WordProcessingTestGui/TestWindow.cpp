#include "TestWindow.h"

#include "DfWorker.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QThread>
#include <QTime>
#include <QVBoxLayout>

namespace
{

QPushButton*	makeButton( const QString& p_text, QWidget* p_parent )
{
	QPushButton* l_button = new QPushButton( p_text, p_parent );
	l_button ->setAutoDefault( false );
	return l_button;
}

} // namespace

TestWindow::TestWindow( QWidget* p_parent )
	: QMainWindow( p_parent )
	, m_thread( nullptr )
	, m_worker( nullptr )
	, m_busy( false )
	, m_loaded( false )
	, m_instantiated( false )
	, m_compileTargetAuto( true )
	, m_saveFileAuto( true )
{
	createUi();
	startWorker();
	updateActions();

	log( "就绪。Compiler 页把 docx 编译为 dfw；Merger 页加载 dfw 后可合并数据、设置剪贴板值、粘贴和保存。" );
	log( "注意: Merger 的任何一次调用失败都会丢弃已加载的模板，需要重新加载。" );
} // c'tor

TestWindow::~TestWindow()
{
	if ( m_thread )
	{
		m_thread ->quit();
		m_thread ->wait();
	}
} // d'tor



void TestWindow::createUi()
{
	setWindowTitle( "DocxFactory 测试 GUI" );
	resize( 960, 720 );

	m_tabs = new QTabWidget( this );
	m_tabs ->addTab( createCompilerTab(), "Compiler" );
	m_tabs ->addTab( createMergerTab(), "Merger" );

	m_log = new QPlainTextEdit( this );
	m_log ->setReadOnly( true );
	m_log ->setMaximumBlockCount( 5000 );
	m_log ->setPlaceholderText( "操作日志" );

	QSplitter* l_splitter = new QSplitter( Qt::Vertical, this );
	l_splitter ->addWidget( m_tabs );
	l_splitter ->addWidget( m_log );
	l_splitter ->setStretchFactor( 0, 1 );
	l_splitter ->setStretchFactor( 1, 0 );

	setCentralWidget( l_splitter );
	statusBar() ->showMessage( "就绪" );
} // createUi

QWidget* TestWindow::createCompilerTab()
{
	QWidget* l_tab = new QWidget( this );

	m_srcFile = new QLineEdit( l_tab );
	m_srcFile ->setPlaceholderText( "输入或选择 .docx 文件" );

	QPushButton* l_srcBrowse = makeButton( "浏览...", l_tab );
	connect( l_srcBrowse, &QPushButton::clicked, this, &TestWindow::onBrowseSource );

	QHBoxLayout* l_srcRow = new QHBoxLayout;
	l_srcRow ->setContentsMargins( 0, 0, 0, 0 );
	l_srcRow ->addWidget( m_srcFile, 1 );
	l_srcRow ->addWidget( l_srcBrowse );

	m_compileTarget = new QLineEdit( l_tab );
	m_compileTarget ->setPlaceholderText( "输出 .dfw 文件（默认与源文件同名）" );

	QPushButton* l_targetBrowse = makeButton( "浏览...", l_tab );
	connect( l_targetBrowse, &QPushButton::clicked, this, &TestWindow::onBrowseCompileTarget );

	QHBoxLayout* l_targetRow = new QHBoxLayout;
	l_targetRow ->setContentsMargins( 0, 0, 0, 0 );
	l_targetRow ->addWidget( m_compileTarget, 1 );
	l_targetRow ->addWidget( l_targetBrowse );

	m_tempDir = new QLineEdit( l_tab );
	m_tempDir ->setReadOnly( true );
	m_tempDir ->setPlaceholderText( "由 DocxFactory 决定" );

	m_compileButton = makeButton( "编译", l_tab );

	QFormLayout* l_form = new QFormLayout;
	l_form ->addRow( "源文件", l_srcRow );
	l_form ->addRow( "输出文件", l_targetRow );
	l_form ->addRow( "临时目录", m_tempDir );

	QLabel* l_hint = new QLabel(
		"编译生成 .dfw 模板，可在 Merger 页签加载。", l_tab );
	l_hint ->setWordWrap( true );

	QVBoxLayout* l_layout = new QVBoxLayout( l_tab );
	l_layout ->addLayout( l_form );
	l_layout ->addWidget( m_compileButton, 0, Qt::AlignLeft );
	l_layout ->addWidget( l_hint );
	l_layout ->addStretch( 1 );

	connect( m_srcFile, &QLineEdit::textChanged, this,
		[ this ]()
		{
			deriveCompileTarget();
			updateActions();
		} );

	connect( m_compileTarget, &QLineEdit::textEdited, this,
		[ this ]()
		{
			m_compileTargetAuto = false;
			updateActions();
		} );

	connect( m_compileButton, &QPushButton::clicked, this, &TestWindow::onCompile );

	return l_tab;
} // createCompilerTab

QWidget* TestWindow::createMergerTab()
{
	QWidget* l_tab = new QWidget( this );

	// template
	QGroupBox* l_templateGroup = new QGroupBox( "模板", l_tab );

	m_dfwFile = new QLineEdit( l_templateGroup );
	m_dfwFile ->setPlaceholderText( "输入或选择 .dfw 模板" );

	QPushButton* l_dfwBrowse = makeButton( "浏览...", l_templateGroup );
	connect( l_dfwBrowse, &QPushButton::clicked, this, &TestWindow::onBrowseDfw );

	m_loadButton	= makeButton( "加载", l_templateGroup );
	m_closeButton	= makeButton( "卸载", l_templateGroup );

	connect( m_loadButton,	&QPushButton::clicked, this, &TestWindow::onLoad );
	connect( m_closeButton,	&QPushButton::clicked, this, &TestWindow::onCloseTemplate );

	QHBoxLayout* l_fileRow = new QHBoxLayout;
	l_fileRow ->addWidget( m_dfwFile, 1 );
	l_fileRow ->addWidget( l_dfwBrowse );
	l_fileRow ->addWidget( m_loadButton );
	l_fileRow ->addWidget( m_closeButton );

	m_sessionLabel = new QLabel( "未加载模板", l_templateGroup );
	m_sessionLabel ->setWordWrap( true );

	QVBoxLayout* l_templateLayout = new QVBoxLayout( l_templateGroup );
	l_templateLayout ->addLayout( l_fileRow );
	l_templateLayout ->addWidget( m_sessionLabel );

	// data
	QGroupBox* l_dataGroup = new QGroupBox( "合并数据（可选）", l_tab );

	m_dataFile = new QLineEdit( l_dataGroup );
	m_dataFile ->setPlaceholderText( "JSON / XML 数据文件，留空则按空数据合并" );

	QPushButton* l_dataBrowse = makeButton( "浏览...", l_dataGroup );
	connect( l_dataBrowse, &QPushButton::clicked, this, &TestWindow::onBrowseData );

	m_dataButton = makeButton( "合并数据", l_dataGroup );
	connect( m_dataButton, &QPushButton::clicked, this, &TestWindow::onMerge );

	QHBoxLayout* l_dataRow = new QHBoxLayout;
	l_dataRow ->addWidget( m_dataFile, 1 );
	l_dataRow ->addWidget( l_dataBrowse );
	l_dataRow ->addWidget( m_dataButton );

	QVBoxLayout* l_dataLayout = new QVBoxLayout( l_dataGroup );
	l_dataLayout ->addLayout( l_dataRow );

	// clipboard values and paste
	QGroupBox* l_valueGroup = new QGroupBox( "剪贴板值与粘贴", l_tab );

	m_itemCombo		= new QComboBox( l_valueGroup );
	m_fieldCombo	= new QComboBox( l_valueGroup );

	m_itemCombo	->setEditable( true );
	m_fieldCombo->setEditable( true );
	m_itemCombo	->setInsertPolicy( QComboBox::NoInsert );
	m_fieldCombo->setInsertPolicy( QComboBox::NoInsert );

	connect( m_itemCombo, &QComboBox::currentTextChanged, this, &TestWindow::onItemChanged );

	QHBoxLayout* l_fieldRow = new QHBoxLayout;
	l_fieldRow ->addWidget( new QLabel( "Item", l_valueGroup ) );
	l_fieldRow ->addWidget( m_itemCombo, 1 );
	l_fieldRow ->addWidget( new QLabel( "字段", l_valueGroup ) );
	l_fieldRow ->addWidget( m_fieldCombo, 1 );

	m_valueEdit = new QLineEdit( l_valueGroup );
	m_valueEdit ->setPlaceholderText( "值，或图片/文件路径" );

	m_browseValueButton = makeButton( "选图片...", l_valueGroup );
	m_addRowButton		= makeButton( "加入表格", l_valueGroup );

	connect( m_browseValueButton,&QPushButton::clicked,	this, &TestWindow::onBrowseValue );
	connect( m_addRowButton,	&QPushButton::clicked,	this, &TestWindow::onAddRow );
	connect( m_valueEdit,		&QLineEdit::returnPressed, this, &TestWindow::onAddRow );

	QHBoxLayout* l_valueRow = new QHBoxLayout;
	l_valueRow ->addWidget( m_valueEdit, 1 );
	l_valueRow ->addWidget( m_browseValueButton );
	l_valueRow ->addWidget( m_addRowButton );

	m_valueTable = new QTableWidget( 0, 3, l_valueGroup );

	QStringList l_headers;
	l_headers << "Item" << "字段" << "值";

	m_valueTable ->setHorizontalHeaderLabels( l_headers );
	m_valueTable ->horizontalHeader() ->setStretchLastSection( true );
	m_valueTable ->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_valueTable ->setEditTriggers(
		QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed );
	m_valueTable ->verticalHeader() ->setVisible( false );

	connect( m_valueTable ->model(), &QAbstractItemModel::rowsInserted,
		this, [ this ]() { updateActions(); } );
	connect( m_valueTable ->model(), &QAbstractItemModel::rowsRemoved,
		this, [ this ]() { updateActions(); } );

	m_applyButton		= makeButton( "应用全部", l_valueGroup );
	m_removeRowButton	= makeButton( "删除选中行", l_valueGroup );
	m_clearRowsButton	= makeButton( "清空表格", l_valueGroup );
	m_pasteButton		= makeButton( "粘贴（生成项）", l_valueGroup );

	connect( m_applyButton,		&QPushButton::clicked, this, &TestWindow::onApplyRows );
	connect( m_removeRowButton,	&QPushButton::clicked, this, &TestWindow::onRemoveRow );
	connect( m_clearRowsButton,	&QPushButton::clicked, this, &TestWindow::onClearRows );
	connect( m_pasteButton,		&QPushButton::clicked, this, &TestWindow::onPaste );

	QHBoxLayout* l_valueButtons = new QHBoxLayout;
	l_valueButtons ->addWidget( m_applyButton );
	l_valueButtons ->addWidget( m_removeRowButton );
	l_valueButtons ->addWidget( m_clearRowsButton );
	l_valueButtons ->addStretch( 1 );
	l_valueButtons ->addWidget( m_pasteButton );

	QVBoxLayout* l_valueLayout = new QVBoxLayout( l_valueGroup );
	l_valueLayout ->addLayout( l_fieldRow );
	l_valueLayout ->addLayout( l_valueRow );
	l_valueLayout ->addWidget( m_valueTable );
	l_valueLayout ->addLayout( l_valueButtons );

	QLabel* l_valueHint = new QLabel(
		"提示: 「应用全部」只把值放进待粘贴内容；"
		"需再点「粘贴（生成项）」才会写入文档，然后保存。\n"
		"图片字段（如 pic4_2_1）的值填图片文件路径，可用「选图片...」；"
		"路径按系统编码传给库，中文路径可用。", l_valueGroup );
	l_valueHint ->setWordWrap( true );
	l_valueLayout ->addWidget( l_valueHint );

	// save
	QGroupBox* l_saveGroup = new QGroupBox( "保存", l_tab );

	m_saveFile = new QLineEdit( l_saveGroup );
	m_saveFile ->setPlaceholderText( "输出 .docx 文件（默认 <模板名>_out.docx）" );

	QPushButton* l_saveBrowse = makeButton( "浏览...", l_saveGroup );
	connect( l_saveBrowse, &QPushButton::clicked, this, &TestWindow::onBrowseSave );

	m_saveButton = makeButton( "保存 docx", l_saveGroup );
	connect( m_saveButton, &QPushButton::clicked, this, &TestWindow::onSave );

	QHBoxLayout* l_saveRow = new QHBoxLayout;
	l_saveRow ->addWidget( m_saveFile, 1 );
	l_saveRow ->addWidget( l_saveBrowse );
	l_saveRow ->addWidget( m_saveButton );

	QVBoxLayout* l_saveLayout = new QVBoxLayout( l_saveGroup );
	l_saveLayout ->addLayout( l_saveRow );

	QVBoxLayout* l_layout = new QVBoxLayout( l_tab );
	l_layout ->addWidget( l_templateGroup );
	l_layout ->addWidget( l_dataGroup );
	l_layout ->addWidget( l_valueGroup, 1 );
	l_layout ->addWidget( l_saveGroup );

	connect( m_dfwFile, &QLineEdit::textChanged, this,
		[ this ]()
		{
			deriveSaveFile();
			updateActions();
		} );

	connect( m_saveFile, &QLineEdit::textEdited, this,
		[ this ]()
		{
			m_saveFileAuto = false;
			updateActions();
		} );

	return l_tab;
} // createMergerTab

void TestWindow::startWorker()
{
	m_thread	= new QThread( this );
	m_worker	= new DfWorker;

	m_worker ->moveToThread( m_thread );

	connect( m_thread, &QThread::finished, m_worker, &QObject::deleteLater );

	connect( this, &TestWindow::compileRequested,
		m_worker, &DfWorker::compile, Qt::QueuedConnection );
	connect( this, &TestWindow::tempDirRequested,
		m_worker, &DfWorker::queryTempDir, Qt::QueuedConnection );
	connect( this, &TestWindow::loadRequested,
		m_worker, &DfWorker::loadTemplate, Qt::QueuedConnection );
	connect( this, &TestWindow::closeRequested,
		m_worker, &DfWorker::closeTemplate, Qt::QueuedConnection );
	connect( this, &TestWindow::mergeRequested,
		m_worker, &DfWorker::mergeData, Qt::QueuedConnection );
	connect( this, &TestWindow::clipboardValueRequested,
		m_worker, &DfWorker::setClipboardValue, Qt::QueuedConnection );
	connect( this, &TestWindow::pasteRequested,
		m_worker, &DfWorker::pasteItem, Qt::QueuedConnection );
	connect( this, &TestWindow::saveRequested,
		m_worker, &DfWorker::saveDocument, Qt::QueuedConnection );
	connect( this, &TestWindow::fieldsRequested,
		m_worker, &DfWorker::queryItemFields, Qt::QueuedConnection );

	connect( m_worker, &DfWorker::compiled,			this, &TestWindow::onCompiled );
	connect( m_worker, &DfWorker::tempDirQueried,	this, &TestWindow::onTempDirQueried );
	connect( m_worker, &DfWorker::loaded,			this, &TestWindow::onLoaded );
	connect( m_worker, &DfWorker::closed,			this, &TestWindow::onClosed );
	connect( m_worker, &DfWorker::merged,			this, &TestWindow::onMerged );
	connect( m_worker, &DfWorker::clipboardSet,		this, &TestWindow::onClipboardSet );
	connect( m_worker, &DfWorker::pasted,			this, &TestWindow::onPasted );
	connect( m_worker, &DfWorker::saved,			this, &TestWindow::onSaved );
	connect( m_worker, &DfWorker::itemFieldsQueried,	this, &TestWindow::onItemFieldsQueried );

	m_thread ->start();

	emit tempDirRequested();
} // startWorker



// state

void TestWindow::setBusy( bool p_busy )
{
	m_busy = p_busy;
	updateActions();
} // setBusy

void TestWindow::updateActions()
{
	const bool l_idle	= !m_busy;
	const bool l_loaded	= l_idle && m_loaded;
	const int	l_rows	= m_valueTable ->rowCount();

	m_srcFile			->setEnabled( l_idle );
	m_compileTarget		->setEnabled( l_idle );
	m_compileButton		->setEnabled( l_idle && !m_srcFile ->text().trimmed().isEmpty() );

	m_dfwFile			->setEnabled( l_idle );
	m_loadButton		->setEnabled( l_idle );
	m_closeButton		->setEnabled( l_loaded );
	m_dataFile			->setEnabled( l_idle );
	m_dataButton		->setEnabled( l_loaded );
	m_itemCombo			->setEnabled( l_loaded );
	m_fieldCombo		->setEnabled( l_loaded );
	m_valueEdit			->setEnabled( l_loaded );
	m_addRowButton		->setEnabled( l_loaded );
	m_valueTable		->setEnabled( l_loaded );
	m_applyButton		->setEnabled( l_loaded && l_rows > 0 );
	m_removeRowButton	->setEnabled( l_loaded && l_rows > 0 );
	m_clearRowsButton	->setEnabled( l_loaded && l_rows > 0 );
	m_pasteButton		->setEnabled( l_loaded && isPasteable( m_itemCombo ->currentText() ) );
	m_saveFile			->setEnabled( l_idle );
	m_saveButton		->setEnabled( l_loaded && !m_saveFile ->text().trimmed().isEmpty() );
} // updateActions

void TestWindow::setSessionLoaded( bool p_loaded )
{
	m_loaded		= p_loaded;
	m_instantiated	= false;

	if ( !p_loaded )
	{
		m_loadedFile.clear();

		m_itemCombo	->clear();
		m_fieldCombo->clear();
		m_sessionLabel ->setText( "未加载模板" );
	}

	updateActions();
} // setSessionLoaded

void TestWindow::invalidateSession()
{
	setSessionLoaded( false );

	m_sessionLabel ->setText( "模板未加载（操作失败后需重新加载）" );
} // invalidateSession



// log

void TestWindow::log( const QString& p_text )
{
	m_log ->appendPlainText( QTime::currentTime() .toString( "HH:mm:ss" ) + "  " + p_text );
} // log

void TestWindow::logError( const QString& p_what, const QString& p_where )
{
	QString l_text = p_what;

	if ( !p_where.isEmpty() )
		l_text += "  [" + p_where + "]";

	log( "失败: " + l_text );
	setStatus( "失败" );
} // logError

void TestWindow::logResult( bool p_ok, const QString& p_successText,
							const QString& p_what, const QString& p_where )
{
	if ( p_ok )
	{
		log( p_successText );
		setStatus( p_successText );
	}

	else
		logError( p_what, p_where );
} // logResult

void TestWindow::setStatus( const QString& p_text )
{
	statusBar() ->showMessage( p_text, 8000 );
} // setStatus



// paths

void TestWindow::deriveCompileTarget()
{
	if ( !m_compileTargetAuto && !m_compileTarget ->text().trimmed().isEmpty() )
		return;

	const QString l_src = m_srcFile ->text().trimmed();

	if ( l_src.isEmpty() )
	{
		m_compileTarget ->clear();
		return;
	}

	const QFileInfo	l_info( l_src );
	const QString	l_target = l_info.absolutePath() + "/" + l_info.completeBaseName() + ".dfw";

	m_compileTargetAuto = true;
	m_compileTarget ->setText( QDir::toNativeSeparators( l_target ) );
} // deriveCompileTarget

void TestWindow::deriveSaveFile()
{
	if ( !m_saveFileAuto && !m_saveFile ->text().trimmed().isEmpty() )
		return;

	const QString l_dfw = m_dfwFile ->text().trimmed();

	if ( l_dfw.isEmpty() )
	{
		m_saveFile ->clear();
		return;
	}

	const QFileInfo	l_info( l_dfw );
	const QString	l_save = l_info.absolutePath() + "/" + l_info.completeBaseName() + "_out.docx";

	m_saveFileAuto = true;
	m_saveFile ->setText( QDir::toNativeSeparators( l_save ) );
} // deriveSaveFile

QStringList TestWindow::visibleItems( const QStringList& p_items ) const
{
	QStringList l_items;

	for ( const QString& l_item : p_items )
	{
		// dfw_setClipboardValueByStr / dfw_paste reject the internal items
		if ( l_item.startsWith( '_' ) && l_item != "_header" && l_item != "_footer" )
			continue;

		l_items << l_item;
	}

	return l_items;
} // visibleItems

bool TestWindow::isPasteable( const QString& p_item ) const
{
	const QString l_item = p_item.trimmed();

	return !l_item.isEmpty() && !l_item.startsWith( '_' );
} // isPasteable

bool TestWindow::confirmOverwrite( const QString& p_file )
{
	if ( !QFileInfo::exists( p_file ) )
		return true;

	return QMessageBox::question( this, "覆盖确认",
		"文件已存在，是否覆盖？\n" + QDir::toNativeSeparators( p_file ),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) == QMessageBox::Yes;
} // confirmOverwrite

QString TestWindow::cellText( int p_row, int p_column ) const
{
	QTableWidgetItem* l_item = m_valueTable ->item( p_row, p_column );

	return l_item ? l_item ->text() : QString();
} // cellText



// compiler tab

void TestWindow::onBrowseSource()
{
	const QString l_file = QFileDialog::getOpenFileName( this,
		"选择 docx 文件", m_srcFile ->text(), "Word 文档 (*.docx)" );

	if ( l_file.isEmpty() )
		return;

	m_srcFile ->setText( QDir::toNativeSeparators( l_file ) );
} // onBrowseSource

void TestWindow::onBrowseCompileTarget()
{
	const QString l_file = QFileDialog::getSaveFileName( this,
		"选择输出 dfw 文件", m_compileTarget ->text(), "模板 (*.dfw)" );

	if ( l_file.isEmpty() )
		return;

	m_compileTargetAuto = false;
	m_compileTarget ->setText( QDir::toNativeSeparators( l_file ) );
} // onBrowseCompileTarget

void TestWindow::onCompile()
{
	const QString l_src = m_srcFile ->text().trimmed();
	QString l_target = m_compileTarget ->text().trimmed();

	if ( l_src.isEmpty() )
	{
		log( "请先选择源 docx 文件" );
		return;
	}

	if ( l_target.isEmpty() )
	{
		log( "请先指定输出 dfw 文件" );
		return;
	}

	if ( !confirmOverwrite( l_target ) )
	{
		log( "已取消编译" );
		return;
	}

	setBusy( true );
	log( "编译: " + QDir::toNativeSeparators( l_src ) + " -> " + QDir::toNativeSeparators( l_target ) );

	emit compileRequested( l_src, l_target );
} // onCompile



// merger tab

void TestWindow::onBrowseDfw()
{
	const QString l_file = QFileDialog::getOpenFileName( this,
		"选择 dfw 模板", m_dfwFile ->text(), "模板 (*.dfw)" );

	if ( l_file.isEmpty() )
		return;

	m_dfwFile ->setText( QDir::toNativeSeparators( l_file ) );
} // onBrowseDfw

void TestWindow::onLoad()
{
	const QString l_file = m_dfwFile ->text().trimmed();

	if ( l_file.isEmpty() )
	{
		log( "请先选择 dfw 模板" );
		return;
	}

	setBusy( true );
	log( "加载模板: " + QDir::toNativeSeparators( l_file ) );

	emit loadRequested( l_file );
} // onLoad

void TestWindow::onCloseTemplate()
{
	if ( !m_loaded )
		return;

	setBusy( true );
	emit closeRequested();
} // onCloseTemplate

void TestWindow::onBrowseData()
{
	const QString l_file = QFileDialog::getOpenFileName( this,
		"选择数据文件", m_dataFile ->text(),
		"数据文件 (*.json *.xml);;所有文件 (*.*)" );

	if ( l_file.isEmpty() )
		return;

	m_dataFile ->setText( QDir::toNativeSeparators( l_file ) );
} // onBrowseData

void TestWindow::onMerge()
{
	if ( !m_loaded )
		return;

	QByteArray	l_data;
	const QString l_file = m_dataFile ->text().trimmed();

	if ( !l_file.isEmpty() )
	{
		QFile l_in( l_file );

		if ( !l_in.open( QIODevice::ReadOnly ) )
		{
			log( "无法读取数据文件: " + l_in .errorString() );
			return;
		}

		l_data = l_in .readAll();
	}

	setBusy( true );
	log( l_file.isEmpty()
		? QString( "按空数据合并" )
		: "合并数据: " + QDir::toNativeSeparators( l_file ) );

	emit mergeRequested( l_data );
} // onMerge

void TestWindow::onItemChanged( const QString& p_item )
{
	updateActions();

	if ( !m_loaded || m_busy )
		return;

	m_fieldCombo ->clear();

	emit fieldsRequested( p_item.trimmed() );
} // onItemChanged

void TestWindow::onBrowseValue()
{
	const QString l_file = QFileDialog::getOpenFileName( this,
		"选择图片文件", QString(),
		"图片 (*.png *.jpg *.jpeg *.gif *.bmp *.tif *.tiff *.emf *.wmf);;所有文件 (*)" );

	if ( l_file.isEmpty() )
		return;

	m_valueEdit ->setText( QDir::toNativeSeparators( l_file ) );
} // onBrowseValue

void TestWindow::onAddRow()
{
	const QString l_item	= m_itemCombo ->currentText().trimmed();
	const QString l_field	= m_fieldCombo ->currentText().trimmed();
	const QString l_value	= m_valueEdit ->text();

	if ( l_item.isEmpty() )
	{
		log( "请先选择或输入 item" );
		return;
	}

	if ( l_item.startsWith( '_' ) && l_item != "_header" && l_item != "_footer" )
	{
		log( "item 名称以 _ 开头，仅支持 _header / _footer" );
		return;
	}

	if ( l_field.isEmpty() )
	{
		log( "请先选择或输入字段" );
		return;
	}

	if ( l_value.isEmpty() )
	{
		log( "空值会被 DocxFactory 忽略，未加入表格" );
		return;
	}

	// picture fields silently drop a file that does not exist, so say so here
	const QFileInfo	l_valueInfo( l_value );
	static const QStringList l_imageExts =
		{ "png", "jpg", "jpeg", "gif", "bmp", "tif", "tiff", "emf", "wmf" };

	if ( l_imageExts.contains( l_valueInfo.suffix().toLower() ) && !l_valueInfo.isFile() )
		log( "警告: 图片文件不存在，库会静默忽略该图片字段: "
			+ QDir::toNativeSeparators( l_value ) );

	const int l_row = m_valueTable ->rowCount();

	m_valueTable ->insertRow( l_row );
	m_valueTable ->setItem( l_row, 0, new QTableWidgetItem( l_item ) );
	m_valueTable ->setItem( l_row, 1, new QTableWidgetItem( l_field ) );
	m_valueTable ->setItem( l_row, 2, new QTableWidgetItem( l_value ) );

	m_valueEdit ->clear();
	m_valueEdit ->setFocus();
} // onAddRow

void TestWindow::onApplyRows()
{
	if ( !m_loaded || m_busy )
		return;

	m_applyQueue.clear();

	for ( int l_row = 0; l_row < m_valueTable ->rowCount(); ++l_row )
	{
		ValueRow l_valueRow;

		l_valueRow.item		= cellText( l_row, 0 ).trimmed();
		l_valueRow.field	= cellText( l_row, 1 ).trimmed();
		l_valueRow.value	= cellText( l_row, 2 );

		if ( l_valueRow.item .isEmpty()
		  || l_valueRow.field .isEmpty()
		  || l_valueRow.value .isEmpty() )
			continue;

		m_applyQueue << l_valueRow;
	}

	if ( m_applyQueue.isEmpty() )
	{
		log( "表格中没有可应用的行" );
		return;
	}

	setBusy( true );
	applyNextRow();
} // onApplyRows

void TestWindow::applyNextRow()
{
	if ( m_applyQueue.isEmpty() )
	{
		setBusy( false );
		log( "剪贴板值应用完成" );
		setStatus( "剪贴板值应用完成" );
		return;
	}

	const ValueRow l_row = m_applyQueue .takeFirst();

	emit clipboardValueRequested( l_row.item, l_row.field, l_row.value,
		l_row.item + " / " + l_row.field );
} // applyNextRow

void TestWindow::onRemoveRow()
{
	const int l_row = m_valueTable ->currentRow();

	if ( l_row >= 0 )
		m_valueTable ->removeRow( l_row );
} // onRemoveRow

void TestWindow::onClearRows()
{
	m_valueTable ->setRowCount( 0 );
} // onClearRows

void TestWindow::onPaste()
{
	const QString l_item = m_itemCombo ->currentText().trimmed();

	if ( !isPasteable( l_item ) )
	{
		log( "请选择不以 _ 开头的 item 再粘贴" );
		return;
	}

	setBusy( true );
	log( "粘贴 item: " + l_item );

	emit pasteRequested( l_item );
} // onPaste

void TestWindow::onBrowseSave()
{
	const QString l_file = QFileDialog::getSaveFileName( this,
		"选择输出 docx 文件", m_saveFile ->text(), "Word 文档 (*.docx)" );

	if ( l_file.isEmpty() )
		return;

	m_saveFileAuto = false;
	m_saveFile ->setText( QDir::toNativeSeparators( l_file ) );
} // onBrowseSave

void TestWindow::onSave()
{
	if ( !m_loaded )
		return;

	const QString l_file = m_saveFile ->text().trimmed();

	if ( l_file.isEmpty() )
	{
		log( "请先指定输出 docx 文件" );
		return;
	}

	if ( !m_instantiated )
	{
		const QMessageBox::StandardButton l_answer = QMessageBox::warning( this, "保存",
			"尚未合并数据，也未粘贴任何 item。\n"
			"剪贴板值只放进待粘贴内容，必须点「粘贴（生成项）」才会写入文档。\n"
			"现在保存的文档几乎是空的。仍要保存吗？",
			QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel );

		if ( l_answer != QMessageBox::Save )
		{
			log( "已取消保存（未合并数据也未粘贴）" );
			return;
		}
	}

	if ( !confirmOverwrite( l_file ) )
	{
		log( "已取消保存" );
		return;
	}

	setBusy( true );
	log( "保存: " + QDir::toNativeSeparators( l_file ) );

	emit saveRequested( l_file );
} // onSave



// worker results

void TestWindow::onCompiled( bool p_ok, const QString& p_what, const QString& p_where )
{
	setBusy( false );

	if ( p_ok )
	{
		const QString l_target = m_compileTarget ->text().trimmed();

		logResult( true, "编译完成: " + QDir::toNativeSeparators( l_target ),
			p_what, p_where );

		// convenient hand over to the merger tab
		m_dfwFile ->setText( l_target );
	}

	else
		logResult( false, QString(), p_what, p_where );
} // onCompiled

void TestWindow::onTempDirQueried( const QString& p_dir )
{
	m_tempDir ->setText( QDir::toNativeSeparators( p_dir ) );
} // onTempDirQueried

void TestWindow::onLoaded( bool p_ok, const QString& p_what, const QString& p_where,
						   const QStringList& p_items )
{
	setBusy( false );

	if ( !p_ok )
	{
		logError( p_what, p_where );
		invalidateSession();
		return;
	}

	m_loadedFile = m_dfwFile ->text().trimmed();

	const QStringList l_items = visibleItems( p_items );

	m_itemCombo ->blockSignals( true );
	m_itemCombo ->clear();
	m_itemCombo ->addItems( l_items );
	m_itemCombo ->blockSignals( false );

	m_fieldCombo	->clear();
	m_valueTable	->setRowCount( 0 );

	setSessionLoaded( true );
	m_sessionLabel ->setText(
		QString( "已加载: %1（%2 项）" )
			.arg( QDir::toNativeSeparators( m_loadedFile ) )
			.arg( l_items.size() ) );

	deriveSaveFile();
	updateActions();

	logResult( true, QString( "模板已加载: %1（可选 %2 项）" )
		.arg( QDir::toNativeSeparators( m_loadedFile ) )
		.arg( l_items.size() ), p_what, p_where );

	if ( !l_items.isEmpty() )
		onItemChanged( m_itemCombo ->currentText() );
} // onLoaded

void TestWindow::onClosed( bool p_ok, const QString& p_what, const QString& p_where )
{
	setBusy( false );

	if ( p_ok )
	{
		setSessionLoaded( false );
		logResult( true, "模板已卸载", p_what, p_where );
	}

	else
	{
		logError( p_what, p_where );
		invalidateSession();
	}
} // onClosed

void TestWindow::onMerged( bool p_ok, const QString& p_what, const QString& p_where )
{
	setBusy( false );

	if ( p_ok )
	{
		m_instantiated = true;
		logResult( true, "合并完成（模板仍在会话中，可继续粘贴或保存）", p_what, p_where );
	}

	else
	{
		logError( p_what, p_where );
		invalidateSession();
	}
} // onMerged

void TestWindow::onClipboardSet( bool p_ok, const QString& p_what, const QString& p_where,
								 const QString& p_context )
{
	if ( !p_ok )
	{
		m_applyQueue.clear();
		logError( p_what, p_where );
		invalidateSession();
		setBusy( false );
		return;
	}

	log( "已设置剪贴板值: " + p_context );

	applyNextRow();
} // onClipboardSet

void TestWindow::onPasted( bool p_ok, const QString& p_what, const QString& p_where )
{
	setBusy( false );

	if ( p_ok )
	{
		m_instantiated = true;
		logResult( true, "粘贴完成", p_what, p_where );
	}

	else
	{
		logError( p_what, p_where );
		invalidateSession();
	}
} // onPasted

void TestWindow::onSaved( bool p_ok, const QString& p_what, const QString& p_where )
{
	setBusy( false );

	if ( p_ok )
		logResult( true, "保存完成: " + QDir::toNativeSeparators( m_saveFile ->text().trimmed() ),
			p_what, p_where );

	else
	{
		logError( p_what, p_where );
		invalidateSession();
	}
} // onSaved

void TestWindow::onItemFieldsQueried( const QString& p_item, const QStringList& p_fields )
{
	if ( !m_loaded
	  || p_item != m_itemCombo ->currentText().trimmed() )
		return;

	const QString l_current = m_fieldCombo ->currentText();

	m_fieldCombo ->blockSignals( true );
	m_fieldCombo ->clear();
	m_fieldCombo ->addItems( p_fields );

	if ( p_fields.contains( l_current ) )
		m_fieldCombo ->setCurrentText( l_current );

	m_fieldCombo ->blockSignals( false );
} // onItemFieldsQueried
