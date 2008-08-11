#include "filetransdlg.h"

#include <QFileDialog>
#include <qlabel.h>
#include <qlineedit.h>
#include <QMessageBox>
#include <qpushbutton.h>
#include <qtimer.h>
#include <qfile.h>
#include <q3progressbar.h>
#include <qdir.h>
#include <q3listview.h>
#include <qlayout.h>
#include <q3hbox.h>
#include <qdatetime.h>
#include <qpainter.h>
#include <q3header.h>
#include <q3popupmenu.h>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <Q3PtrList>
#include <QHBoxLayout>
#include <QPixmap>

#include "psicon.h"
#include "psiaccount.h"
#include "userlist.h"
#include "iconwidget.h"
#include "s5b.h"
#include "busywidget.h"
#include "filetransfer.h"
#include "accountscombobox.h"
#include "psiiconset.h"
#include "msgmle.h"
#include "jidutil.h"
#include "psitooltip.h"
#include "psicontactlist.h"
#include "accountlabel.h"
#include "psioptions.h"
#include "fileutil.h"

typedef Q_UINT64 LARGE_TYPE;

#define CSMAX (sizeof(LARGE_TYPE)*8)
#define CSMIN 16
static int calcShift(qlonglong big)
{
	LARGE_TYPE val = 1;
	val <<= CSMAX - 1;
	for(int n = CSMAX - CSMIN; n > 0; --n) {
		if(big & val)
			return n;
		val >>= 1;
	}
	return 0;
}

static int calcComplement(qlonglong big, int shift)
{
	int block = 1 << shift;
	qlonglong rem = big % block;
	if(rem == 0)
		return 0;
	else
		return (block - (int)rem);
}

static int calcTotalSteps(qlonglong big, int shift)
{
	if(big < 1)
		return 0;
	return ((big - 1) >> shift) + 1;
}

static int calcProgressStep(qlonglong big, int complement, int shift)
{
	return ((big + complement) >> shift);
}

static QStringList *activeFiles = 0;

static void active_file_add(const QString &s)
{
	if(!activeFiles)
		activeFiles = new QStringList;
	activeFiles->append(s);
	//printf("added: [%s]\n", s.latin1());
}

static void active_file_remove(const QString &s)
{
	if(!activeFiles)
		return;
	activeFiles->remove(s);
	//printf("removed: [%s]\n", s.latin1());
}

static bool active_file_check(const QString &s)
{
	if(!activeFiles)
		return false;
	return activeFiles->contains(s);
}

static QString clean_filename(const QString &s)
{
//#ifdef Q_OS_WIN
	QString badchars = "\\/|?*:\"<>";
	QString str;
	for(int n = 0; n < s.length(); ++n) {
		bool found = false;
		for(int b = 0; b < badchars.length(); ++b) {
			if(s.at(n) == badchars.at(b)) {
				found = true;
				break;
			}
		}
		if(!found)
			str += s;
	}
	if(str.isEmpty())
		str = "unnamed";
	return str;
//#else
//	return s;
//#endif
}

//----------------------------------------------------------------------------
// FileTransferHandler
//----------------------------------------------------------------------------
class FileTransferHandler::Private
{
public:
	PsiAccount *pa;
	FileTransfer *ft;
	S5BConnection *c;
	Jid peer;
	QString fileName, saveName;
	qlonglong fileSize, sent, offset, length;
	QString desc;
	bool sending;
	QFile f;
	int shift;
	int complement;
	QString activeFile;
};

FileTransferHandler::FileTransferHandler(PsiAccount *pa, FileTransfer *ft)
{
	d = new Private;
	d->pa = pa;
	d->c = 0;

	if(ft) {
		d->sending = false;
		d->peer = ft->peer();
		d->fileName = clean_filename(ft->fileName());
		d->fileSize = ft->fileSize();
		d->desc = ft->description();
		d->shift = calcShift(d->fileSize);
		d->complement = calcComplement(d->fileSize, d->shift);
		d->ft = ft;
		Jid proxy = d->pa->userAccount().dtProxy;
		if(proxy.isValid())
			d->ft->setProxy(proxy);
		mapSignals();
	}
	else {
		d->sending = true;
		d->ft = 0;
	}
}

FileTransferHandler::~FileTransferHandler()
{
	if(!d->activeFile.isEmpty())
		active_file_remove(d->activeFile);

	if(d->ft) {
		d->ft->close();
		delete d->ft;
	}
	delete d;
}

void FileTransferHandler::send(const XMPP::Jid &to, const QString &fname, const QString &desc)
{
	if(!d->sending)
		return;

	d->peer = to;
	QFileInfo fi(fname);
	d->fileName = fi.fileName();
	d->fileSize = fi.size(); // TODO: large file support
	d->desc = desc;
	d->shift = calcShift(d->fileSize);
	d->complement = calcComplement(d->fileSize, d->shift);

	d->ft = d->pa->client()->fileTransferManager()->createTransfer();
	Jid proxy = d->pa->userAccount().dtProxy;
	if(proxy.isValid())
		d->ft->setProxy(proxy);
	mapSignals();

	d->f.setFileName(fname);
	d->ft->sendFile(d->peer, d->fileName, d->fileSize, desc);
}

PsiAccount *FileTransferHandler::account() const
{
	return d->pa;
}

int FileTransferHandler::mode() const
{
	return (d->sending ? Sending : Receiving);
}

Jid FileTransferHandler::peer() const
{
	return d->peer;
}

QString FileTransferHandler::fileName() const
{
	return d->fileName;
}

qlonglong FileTransferHandler::fileSize() const
{
	return d->fileSize;
}

QString FileTransferHandler::description() const
{
	return d->desc;
}

qlonglong FileTransferHandler::offset() const
{
	return d->offset;
}

int FileTransferHandler::totalSteps() const
{
	return calcTotalSteps(d->fileSize, d->shift);
}

bool FileTransferHandler::resumeSupported() const
{
	if(d->ft)
		return d->ft->rangeSupported();
	else
		return false;
}

QString FileTransferHandler::saveName() const
{
	return d->saveName;
}

void FileTransferHandler::accept(const QString &saveName, const QString &fileName, qlonglong offset)
{
	if(d->sending)
		return;
	d->fileName = fileName;
	d->saveName = saveName;
	d->offset = offset;
	d->length = d->fileSize;
	d->f.setFileName(saveName);
	d->ft->accept(offset);
}

void FileTransferHandler::s5b_proxyQuery()
{
	statusMessage(tr("Quering proxy..."));
}

void FileTransferHandler::s5b_proxyResult(bool b)
{
	if(b)
		statusMessage(tr("Proxy query successful."));
	else
		statusMessage(tr("Proxy query failed!"));
}

void FileTransferHandler::s5b_requesting()
{
	statusMessage(tr("Requesting data transfer channel..."));
}

void FileTransferHandler::s5b_accepted()
{
	statusMessage(tr("Peer accepted request."));
}

void FileTransferHandler::s5b_tryingHosts(const StreamHostList &)
{
	statusMessage(tr("Connecting to peer..."));
}

void FileTransferHandler::s5b_proxyConnect()
{
	statusMessage(tr("Connecting to proxy..."));
}

void FileTransferHandler::s5b_waitingForActivation()
{
	statusMessage(tr("Waiting for peer activation..."));
}

void FileTransferHandler::ft_accepted()
{
	d->offset = d->ft->offset();
	d->length = d->ft->length();

	d->c = d->ft->s5bConnection();
	connect(d->c, SIGNAL(proxyQuery()), SLOT(s5b_proxyQuery()));
	connect(d->c, SIGNAL(proxyResult(bool)), SLOT(s5b_proxyResult(bool)));
	connect(d->c, SIGNAL(requesting()), SLOT(s5b_requesting()));
	connect(d->c, SIGNAL(accepted()), SLOT(s5b_accepted()));
	connect(d->c, SIGNAL(tryingHosts(const StreamHostList &)), SLOT(s5b_tryingHosts(const StreamHostList &)));
	connect(d->c, SIGNAL(proxyConnect()), SLOT(s5b_proxyConnect()));
	connect(d->c, SIGNAL(waitingForActivation()), SLOT(s5b_waitingForActivation()));

	if(d->sending)
		accepted();
	else
		statusMessage(QString());
}

void FileTransferHandler::ft_connected()
{
	d->sent = d->offset;

	if(d->sending) {
		// open the file, and set the correct offset
		bool ok = false;
		if(d->f.open(QIODevice::ReadOnly)) {
			if(d->offset == 0) {
				ok = true;
			}
			else {
				if(d->f.at(d->offset))
					ok = true;
			}
		}
		if(!ok) {
			delete d->ft;
			d->ft = 0;
			error(ErrFile, 0, d->f.errorString());
			return;
		}

		if(d->sent == d->fileSize)
			QTimer::singleShot(0, this, SLOT(doFinish()));
		else
			QTimer::singleShot(0, this, SLOT(trySend()));
	}
	else {
		// open the file, truncating if offset is zero, otherwise set the correct offset
		QIODevice::OpenMode m = QIODevice::ReadWrite;
		if(d->offset == 0)
			m |= QIODevice::Truncate;
		bool ok = false;
		if(d->f.open(m)) {
			if(d->offset == 0) {
				ok = true;
			}
			else {
				if(d->f.at(d->offset))
					ok = true;
			}
		}
		if(!ok) {
			delete d->ft;
			d->ft = 0;
			error(ErrFile, 0, d->f.errorString());
			return;
		}

		d->activeFile = d->f.name();
		active_file_add(d->activeFile);

		// done already?  this means a file size of zero
		if(d->sent == d->fileSize)
			QTimer::singleShot(0, this, SLOT(doFinish()));
	}

	connected();
}

void FileTransferHandler::ft_readyRead(const QByteArray &a)
{
	if(!d->sending) {
		//printf("%d bytes read\n", a.size());
		int r = d->f.writeBlock(a.data(), a.size());
		if(r < 0) {
			d->f.close();
			delete d->ft;
			d->ft = 0;
			error(ErrFile, 0, d->f.errorString());
			return;
		}
		d->sent += a.size();
		doFinish();
	}
}

void FileTransferHandler::ft_bytesWritten(int x)
{
	if(d->sending) {
		//printf("%d bytes written\n", x);
		d->sent += x;
		if(d->sent == d->fileSize) {
			d->f.close();
			delete d->ft;
			d->ft = 0;
		}
		else
			QTimer::singleShot(0, this, SLOT(trySend()));
		progress(calcProgressStep(d->sent, d->complement, d->shift), d->sent);
	}
}

void FileTransferHandler::ft_error(int x)
{
	if(d->f.isOpen())
		d->f.close();
	delete d->ft;
	d->ft = 0;

	if(x == FileTransfer::ErrReject)
		error(ErrReject, x, "");
	else if(x == FileTransfer::ErrNeg)
		error(ErrTransfer, x, tr("Unable to negotiate transfer."));
	else if(x == FileTransfer::ErrConnect)
		error(ErrTransfer, x, tr("Unable to connect to peer for data transfer."));
	else if(x == FileTransfer::ErrProxy)
		error(ErrTransfer, x, tr("Unable to connect to proxy for data transfer."));
	else if(x == FileTransfer::ErrStream)
		error(ErrTransfer, x, tr("Lost connection / Cancelled."));
}

void FileTransferHandler::trySend()
{
	// Since trySend comes from singleShot which is an "uncancelable"
	//   action, we should protect that d->ft is valid, for good measure
	if(!d->ft)
		return;

	// When FileTransfer emits error, you are not allowed to call
	//   dataSizeNeeded() afterwards.  Simetime ago, we changed to using
	//   QueuedConnection for error() signal delivery (see mapSignals()).
	//   This made it possible to call dataSizeNeeded by accident between
	//   the error() signal emit and the ft_error() slot invocation.  To
	//   work around this problem, we'll check to see if the FileTransfer
	//   is internally active by checking if s5bConnection() is null.
	//   FIXME: this probably breaks other file transfer methods, whenever
	//   we get those.  Probably we need a real fix in Iris..
	if(!d->ft->s5bConnection())
		return;

	int blockSize = d->ft->dataSizeNeeded();
	QByteArray a(blockSize, 0);
	int r = 0;
	if(blockSize > 0)
		r = d->f.read(a.data(), a.size());
	if(r < 0) {
		d->f.close();
		delete d->ft;
		d->ft = 0;
		error(ErrFile, 0, d->f.errorString());
		return;
	}
	if(r < (int)a.size())
		a.resize(r);
	d->ft->writeFileData(a);
}

void FileTransferHandler::doFinish()
{
	if(d->sent == d->fileSize) {
		d->f.close();
		delete d->ft;
		d->ft = 0;
	}
	progress(calcProgressStep(d->sent, d->complement, d->shift), d->sent);
}

void FileTransferHandler::mapSignals()
{
	connect(d->ft, SIGNAL(accepted()), SLOT(ft_accepted()));
	connect(d->ft, SIGNAL(connected()), SLOT(ft_connected()));
	connect(d->ft, SIGNAL(readyRead(const QByteArray &)), SLOT(ft_readyRead(const QByteArray &)));
	connect(d->ft, SIGNAL(bytesWritten(int)), SLOT(ft_bytesWritten(int)));
	connect(d->ft, SIGNAL(error(int)), SLOT(ft_error(int)),Qt::QueuedConnection);
}

//----------------------------------------------------------------------------
// FileRequestDlg
//----------------------------------------------------------------------------

class FileRequestDlg::Private
{
public:
	PsiCon *psi;
	PsiAccount *pa;
	AccountsComboBox *cb_ident;
	QLabel* lb_identity;
	AccountLabel* lb_ident;
	QLabel* lb_time;
	ChatView *te;
	Jid jid;
	FileTransferHandler *ft;
	QString fileName;
	qlonglong fileSize;
	bool sending;
	QTimer t;
};


FileRequestDlg::FileRequestDlg(const Jid &j, PsiCon *psi, PsiAccount *pa) 
	: QDialog(0, psi_dialog_flags)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setModal(false);
	setupUi(this);
	QStringList l;
	FileRequestDlg(j, psi, pa, l);
}


FileRequestDlg::FileRequestDlg(const Jid &jid, PsiCon *psi, PsiAccount *pa, const QStringList& files, bool direct)
	: QDialog(0, psi_dialog_flags)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	setModal(false);
	setupUi(this);
	d->psi = psi;
	d->pa = 0;
	d->jid = jid;
	d->ft = 0;
	d->sending = true;
	d->lb_ident = 0;
	updateIdentity(pa);

	Q3HBox *hb = new Q3HBox(this);
	d->lb_identity = new QLabel(tr("Identity: "), hb);
	d->cb_ident = d->psi->accountsComboBox(hb);
	connect(d->cb_ident, SIGNAL(activated(PsiAccount *)), SLOT(updateIdentity(PsiAccount *)));
	d->cb_ident->setAccount(pa);
	replaceWidget(lb_accountlabel, hb);
	setTabOrder(d->cb_ident, le_to);
	connect(d->psi, SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	d->te = new ChatView(this);
	d->te->setReadOnly(false);
	d->te->setTextFormat(Qt::PlainText);
	replaceWidget(te_desc, d->te);
	setTabOrder(le_fname, d->te);
	setTabOrder(d->te, pb_stop);

	setWindowTitle(tr("Send File"));
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/upload").icon());
#endif

	le_to->setText(d->jid.full());
	le_to->setReadOnly(false);
	pb_start->setText(tr("&Send"));
	pb_stop->setText(tr("&Close"));

	connect(tb_browse, SIGNAL(clicked()), SLOT(chooseFile()));
	connect(pb_start, SIGNAL(clicked()), SLOT(doStart()));
	connect(pb_stop, SIGNAL(clicked()), SLOT(close()));

	lb_status->setText(tr("Ready"));

	d->te->setFocus();
	d->psi->dialogRegister(this);

	if (files.isEmpty()) {
		QTimer::singleShot(0, this, SLOT(chooseFile()));
	}
	else {
		// TODO: Once sending of multiple files is supported, change this
		QFileInfo fi(files.first());

		// Check if the file is legal
		if(!fi.exists()) {
			QMessageBox::critical(this, tr("Error"), QString("The file '%1' does not exist.").arg(files.first()));
			QTimer::singleShot(0, this, SLOT(reject()));
			return;
		}
		if(fi.isDir()) {
			QMessageBox::critical(this, tr("Error"), tr("Sending folders is not supported."));
			QTimer::singleShot(0, this, SLOT(reject()));
			return;
		}

		FileUtil::setLastUsedSavePath(fi.dirPath());
		le_fname->setText(QDir::convertSeparators(fi.filePath()));
		lb_size->setText(tr("%1 byte(s)").arg(fi.size())); // TODO: large file support
	}

	if (direct) {
		doStart();
	}
}

FileRequestDlg::FileRequestDlg(const QDateTime &ts, FileTransfer *ft, PsiAccount *pa)
	: QDialog(0, psi_dialog_flags)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	setModal(false);
	setupUi(this);
	d->psi = 0;
	d->pa = 0;
	d->jid = ft->peer();
	d->ft = new FileTransferHandler(pa, ft);
	d->sending = false;
	updateIdentity(pa);

	d->fileName = ft->fileName();
	d->fileSize = ft->fileSize();

	d->cb_ident = 0;
	Q3HBox *hb = new Q3HBox(this);
	d->lb_identity = new QLabel(tr("Identity: "), hb);
	d->lb_ident = new AccountLabel(hb);
	d->lb_ident->setAccount(d->pa);
	d->lb_ident->setSizePolicy(QSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed ));
	new QLabel(tr("Time:"), hb);
	d->lb_time = new QLabel(ts.time().toString(Qt::LocalDate), hb);
	d->lb_time->setFrameStyle( QFrame::Panel | QFrame::Sunken );
	connect(d->pa->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	replaceWidget(lb_accountlabel, hb);

	d->te = new ChatView(this);
	d->te->setTextFormat(Qt::PlainText);
	replaceWidget(te_desc, d->te);
	setTabOrder(le_fname, d->te);
	setTabOrder(d->te, pb_stop);

	lb_to->setText(tr("From:"));
	setWindowTitle(tr("Receive File"));
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/download").icon());
#endif

	le_to->setText(d->jid.full());
	le_fname->setText(d->fileName);
	lb_size->setText(tr("%1 byte(s)").arg(d->fileSize));
	d->te->setReadOnly(true);
	d->te->setText(ft->description());
	pb_start->setText(tr("&Accept"));
	pb_stop->setText(tr("&Reject"));

	tb_browse->hide();

	connect(pb_start, SIGNAL(clicked()), SLOT(doStart()));
	connect(pb_stop, SIGNAL(clicked()), SLOT(close()));

	connect(&d->t, SIGNAL(timeout()), SLOT(t_timeout()));

	lb_status->setText(tr("Ready"));

	pb_start->setFocus();
	d->pa->dialogRegister(this);
}

FileRequestDlg::~FileRequestDlg()
{
	delete d->ft;
	if(d->psi)
		d->psi->dialogUnregister(this);
	else
		d->pa->dialogUnregister(this);
	delete d;
}

void FileRequestDlg::done(int r)
{
	if(busy->isActive()) {
		int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to cancel the transfer?"), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;

		// close/reject FT if there is one
		if(d->ft) {
			delete d->ft;
			d->ft = 0;
		}
	}

	QDialog::done(r);
}

void FileRequestDlg::keyPressEvent(QKeyEvent *e)
{
	if(e->key() == Qt::Key_Return && ((e->modifiers() & Qt::ControlModifier) || (e->modifiers() & Qt::AltModifier)) ) {
		if(pb_start->isEnabled())
			doStart();
	}
	else
		QDialog::keyPressEvent(e);
}

void FileRequestDlg::updateIdentity(PsiAccount *pa)
{
	if(d->pa)
		disconnect(d->pa, SIGNAL(disconnected()), this, SLOT(pa_disconnected()));

	if(!pa) {
		close();
		return;
	}

	d->pa = pa;
	connect(d->pa, SIGNAL(disconnected()), this, SLOT(pa_disconnected()));
}

void FileRequestDlg::updateIdentityVisibility()
{
	bool visible = d->pa->psi()->contactList()->enabledAccounts().count() > 1;
	if (d->cb_ident) 
		d->cb_ident->setVisible(visible);
	if (d->lb_ident)
		d->lb_ident->setVisible(visible);
	d->lb_identity->setVisible(visible);
}

void FileRequestDlg::pa_disconnected()
{
	//if(busy->isActive()) {
	//	busy->stop();
	//	close();
	//}
}

void FileRequestDlg::blockWidgets()
{
	if(d->cb_ident)
		d->cb_ident->setEnabled(false);
	le_to->setEnabled(false);
	le_fname->setEnabled(false);
	tb_browse->setEnabled(false);
	d->te->setEnabled(false);
	pb_start->setEnabled(false);
}

void FileRequestDlg::unblockWidgets()
{
	if(d->cb_ident)
		d->cb_ident->setEnabled(true);
	le_to->setEnabled(true);
	le_fname->setEnabled(true);
	tb_browse->setEnabled(true);
	d->te->setEnabled(true);
	pb_start->setEnabled(true);
}

void FileRequestDlg::chooseFile()
{
	QString str = FileUtil::getOpenFileName(this,
	                                        tr("Choose a file"),
	                                        tr("All files (*)"));
	if (!str.isEmpty()) {
		QFileInfo fi(str);
		le_fname->setText(QDir::convertSeparators(fi.filePath()));
		lb_size->setText(tr("%1 byte(s)").arg(fi.size())); // TODO: large file support
	}
}

void FileRequestDlg::doStart()
{
	if(!d->pa->checkConnected(this))
		return;

	if(d->sending) {
		Jid to = le_to->text();
		if(!to.isValid()) {
			QMessageBox::information(this, tr("Error"), tr("The Jabber ID specified is not valid.  Correct this and try again."));
			return;
		}

		QFileInfo fi(le_fname->text());
		if(!fi.exists()) {
			QMessageBox::information(this, tr("Error"), tr("The file specified does not exist.  Choose a correct file name before sending."));
			return;
		}

		blockWidgets();

		pb_stop->setText(tr("&Cancel"));
		pb_stop->setFocus();
		busy->start();
		lb_status->setText(tr("Requesting..."));

		d->fileName = fi.fileName();
		d->fileSize = fi.size(); // TODO: large file support

		d->ft = new FileTransferHandler(d->pa);
		connect(d->ft, SIGNAL(accepted()), SLOT(ft_accepted()));
		connect(d->ft, SIGNAL(statusMessage(const QString &)), SLOT(ft_statusMessage(const QString &)));
		connect(d->ft, SIGNAL(connected()), SLOT(ft_connected()));
		connect(d->ft, SIGNAL(error(int, int, const QString &)), SLOT(ft_error(int, int, const QString &)));
		d->ft->send(le_to->text(), le_fname->text(), d->te->text());
	}
	else {
		QString fname, savename;
		fname = FileUtil::getSaveFileName(this,
		                                  tr("Save As"),
		                                  d->fileName,
		                                  tr("All files (*)"));
		if(fname.isEmpty())
			return;
		QFileInfo fi(fname);
		savename = fname + ".part";
		fname = fi.fileName();

		if(active_file_check(savename)) {
			QMessageBox::information(this, tr("Error"), tr("This file is being transferred already!"));
			return;
		}

		qlonglong resume_offset = 0;
		if(!fi.exists()) {
			// supports resume?  check for a .part
			if(d->ft->resumeSupported()) {
				QFileInfo fi(savename);
				if(fi.exists())
					resume_offset = fi.size();
			}
		}

		pb_start->setEnabled(false);

		le_fname->setText(fname);
		pb_stop->setText(tr("&Cancel"));
		pb_stop->setFocus();
		busy->start();
		lb_status->setText(tr("Accepting..."));

		d->t.start(30000, true);

		connect(d->ft, SIGNAL(accepted()), SLOT(ft_accepted()));
		connect(d->ft, SIGNAL(statusMessage(const QString &)), SLOT(ft_statusMessage(const QString &)));
		connect(d->ft, SIGNAL(connected()), SLOT(ft_connected()));
		connect(d->ft, SIGNAL(error(int, int, const QString &)), SLOT(ft_error(int, int, const QString &)));
		d->ft->accept(savename, fname, resume_offset);
	}
}

void FileRequestDlg::ft_accepted()
{
	lb_status->setText(tr("Accepted!"));
}

void FileRequestDlg::ft_statusMessage(const QString &s)
{
	if(!s.isEmpty()) {
		lb_status->setText(s);
	}

	// stop the timer at first activity
	if(d->t.isActive())
		d->t.stop();
}

void FileRequestDlg::ft_connected()
{
	d->t.stop();
	busy->stop();
	FileTransDlg *w = d->pa->psi()->ftdlg();
	FileTransferHandler *h = d->ft;
	d->ft = 0;
	closeDialogs(this);
	close();
	w->showWithoutActivation();

	w->takeTransfer(h, 0, 0);
}

void FileRequestDlg::ft_error(int x, int fx, const QString &)
{
	d->t.stop();
	busy->stop();

	delete d->ft;
	d->ft = 0;

	closeDialogs(this);

	if(d->sending) {
		unblockWidgets();
		pb_stop->setText(tr("&Close"));
		lb_status->setText(tr("Ready"));
	}

	QString str;
	if(x == FileTransferHandler::ErrReject)
		str = tr("File was rejected by remote user.");
	else if(x == FileTransferHandler::ErrTransfer) {
		if(fx == FileTransfer::ErrNeg)
			str = tr(
				"Unable to negotiate transfer.\n\n"
				"This can happen if the contact did not understand our request, or if the\n"
				"contact is offline."
				);
		else if(fx == FileTransfer::ErrConnect)
			str = tr(
				"Unable to connect to peer for data transfer.\n\n"
				"Ensure that your Data Transfer settings are proper.  If you are behind\n"
				"a NAT router or firewall then you'll need to open the proper TCP port\n"
				"or specify a Data Transfer Proxy in your account settings."
				);
		else if(fx == FileTransfer::ErrProxy)
			str = tr(
				"Failure to either connect to, or activate, the Data Transfer Proxy.\n\n"
				"This means that the Proxy service is either not functioning or it is\n"
				"unreachable.  If you are behind a firewall, then you'll need to ensure\n"
				"that outgoing TCP connections are allowed."
				);
	}
	else
		str = tr("File I/O error");
	QMessageBox::information(this, tr("Error"), str);

	if(!d->sending || x == FileTransferHandler::ErrReject)
		close();
}

void FileRequestDlg::t_timeout()
{
	delete d->ft;
	d->ft = 0;

	busy->stop();
	closeDialogs(this);

	QString str = tr("Unable to accept the file.  Perhaps the sender has cancelled the request.");
	QMessageBox::information(this, tr("Error"), str);
	close();
}

//----------------------------------------------------------------------------
// FileTransDlg
//----------------------------------------------------------------------------
class FileTransItem : public Q3ListViewItem
{
public:
	QPixmap icon;
	bool sending;
	QString name;
	qlonglong size;
	QString peer;
	QString rate;
	int progress;
	qlonglong sent;
	int bps;
	int timeRemaining;
	int id;
	int dist;
	bool done;
	QString error;

	FileTransItem(Q3ListView *parent, const QString &_name, qlonglong _size, const QString &_peer, bool _sending)
	:Q3ListViewItem(parent)
	{
		done = false;
		sending = _sending;
		name = _name;
		size = _size;
		peer = _peer;
		rate = FileTransDlg::tr("N/A");
		sent = 0;
		progress = 0;
		dist = -1;
	}

	void niceUnit(qlonglong n, qlonglong *div, QString *unit)
	{
		qlonglong gb = 1024 * 1024 * 1024;
		qlonglong mb = 1024 * 1024;
		qlonglong kb = 1024;
		if(n >= gb) {
			*div = gb;
			*unit = QString("GB");
		}
		else if(n >= mb) {
			*div = mb;
			*unit = QString("MB");
		}
		else if(n >= kb) {
			*div = kb;
			*unit = QString("KB");
		}
		else {
			*div = 1;
			*unit = QString("B");
		}
	}

	QString roundedNumber(qlonglong n, qlonglong div)
	{
		bool decimal = false;
		if(div >= 1024) {
			div /= 10;
			decimal = true;
		}
		qlonglong x_long = n / div;
		int x = (int)x_long;
		if(decimal) {
			double f = (double)x;
			f /= 10;
			return QString::number(f, 'f', 1);
		}
		else
			return QString::number(x);
	}

	bool setProgress(int _progress, qlonglong _sent, int _bps)
	{
		progress = _progress;
		sent = _sent;
		bps = _bps;

		if(bps > 0) {
			qlonglong rest_long = size - sent;
			rest_long /= bps;
			int maxtime = (23 * 60 * 60) + (59 * 60) + (59); // 23h59m59s
			if(rest_long > maxtime)
				rest_long = maxtime;
			timeRemaining = (int)rest_long;
		}

		int lastDist = dist;
		dist = progressBarDist(progressBarWidth());
		if(dist != lastDist)
			return true;
		else
			return false;
	}

	void updateRate()
	{
		QString s;
		{
			qlonglong div;
			QString unit;
			niceUnit(size, &div, &unit);

			s = roundedNumber(sent, div) + '/' + roundedNumber(size, div) + unit;

			if(done) {
				if(error.isEmpty())
					s += QString(" ") + FileTransDlg::tr("[Done]");
				else
					s += QString(" ") + FileTransDlg::tr("[Error: %1]").arg(error);
			}
			else if(bps == -1)
				s += "";
			else if(bps == 0)
				s += QString(" ") + FileTransDlg::tr("[Stalled]");
			else {
				niceUnit(bps, &div, &unit);
				s += QString(" @ ") + FileTransDlg::tr("%1%2/s").arg(roundedNumber(bps, div)).arg(unit);

				s += ", ";
				QTime t = QTime().addSecs(timeRemaining);
				s += FileTransDlg::tr("%1h%2m%3s remaining").arg(t.hour()).arg(t.minute()).arg(t.second());
			}
		}
		rate = s;
	}

	int progressBarWidth() const
	{
		int m = 4;
		int w = listView()->columnWidth(0);
		//int pw = (w - (3 * m)) / 2;
		int pw = (w - (3 * m)) * 2 / 3;
		return pw;
	}

	int progressBarDist(int width) const
	{
		int xsize = width - 2;
		return (progress * xsize / 8192);
	}

	void drawProgressBar(QPainter *p, const QColorGroup &cg, int x, int y, int width, int height) const
	{
		p->save();
		if(isSelected())
			p->setPen(cg.highlightedText());
		else
			p->setPen(cg.text());
		p->drawRect(x, y, width, height);
		int xoff = x + 1;
		int yoff = y + 1;
		int xsize = width - 2;
		int ysize = height - 2;

		int dist = progressBarDist(width);
		p->fillRect(xoff, yoff, dist, ysize, cg.brush(QColorGroup::Highlight));
		p->fillRect(xoff + dist, yoff, width - 2 - dist, ysize, cg.brush(QColorGroup::Base));

		int percent = progress * 100 / 8192;
		QString s = QString::number(percent) + '%';

		QFontMetrics fm(p->font());
		int ty = ((height - fm.height()) / 2) + fm.ascent() + y;
		int textwidth = fm.width(s);
		int center = xoff + (xsize / 2);

		p->save();
		p->setPen(cg.highlightedText());
		p->setClipRect(xoff, yoff, dist, ysize);
		p->drawText(center - (textwidth / 2), ty, s);
		p->restore();

		p->save();
		p->setPen(cg.text());
		p->setClipRect(xoff + dist, yoff, width - 2 - dist, ysize);
		p->drawText(center - (textwidth / 2), ty, s);
		p->restore();
		p->restore();
	}

	void setup()
	{
		widthChanged();
		Q3ListView *lv = listView();

		QFontMetrics fm = lv->fontMetrics();
		int m = 4;
		int pm = 2;
		int ph = fm.height() + 2 + (pm * 2);
		int h = (ph * 2) + (m * 3);

		h += lv->itemMargin() * 2;

		// ensure an even number
		if(h & 1)
			++h;

		setHeight(h);
	}

	QString chopString(const QString &s, const QFontMetrics &fm, int len) const
	{
		if(fm.width(s) <= len)
			return s;

		QString str;
		uint n = s.length();
		do {
			str = s.mid(0, --n) + "...";
		} while(n > 0 && fm.width(str) > len);
		return str;
	}

	void paintCell(QPainter *mp, const QColorGroup &_cg, int, int width, int)
	{
		QColorGroup cg = _cg;
		int w = width;
		int h = height();

		// tint the background
		/*//QColor base = Qt::black; //cg.base();
		QColor base = Qt::white;
		int red = base.red();
		int green = base.green();
		int blue = base.blue();
		bool light = false;//true;
		if(sending) {
			if(light) {
				green = green * 15 / 16;
				blue = blue * 15 / 16;
			}
			else {
				red = 255 - red;
				red = red * 15 / 16;
				red = 255 - red;
			}
		}
		else {
			if(light) {
				red = red * 15 / 16;
				blue = blue * 15 / 16;
			}
			else {
				green = 255 - green;
				green = green * 15 / 16;
				green = 255 - green;
			}
		}
		base.setRgb(red, green, blue);
		cg.setColor(QColorGroup::Base, base);*/

		QPixmap pix(w, h);
		QPainter *p = new QPainter(&pix);
		QFont font = mp->font();
		QFont boldFont = font;
		boldFont.setBold(true);
		QFontMetrics fm(font);
		QFontMetrics fmbold(boldFont);
		QBrush br;

		// m = margin, pm = progress margin, ph = progress height, yoff = text y offset,
		// tt = text top, tb = text bottom, pw = progress width, px = progress x coord
		int m = 4;
		int pm = 2;
		int ph = fm.height() + 2 + (pm * 2);
		int yoff = 1 + pm;
		int tt = m + yoff + fm.ascent();
		int tb = (m * 2) + ph + yoff + fm.ascent();
		//int pw = (w - (3 * m)) / 2;
		int pw = (w - (3 * m)) * 2 / 3;
		int tw = (w - (3 * m)) - pw;
		int px = (m * 2) + tw;

		// clear out the background
		if(isSelected())
			br = cg.brush(QColorGroup::Highlight);
		else
			br = cg.brush(QColorGroup::Base);
		p->fillRect(0, 0, width, h, br);

		// icon
		p->drawPixmap(m, m + yoff, icon);
		int tm = m + icon.width() + 4;
		tw = tw - (icon.width() + 4);

		// filename / peer
		if(isSelected())
			p->setPen(cg.highlightedText());
		else
			p->setPen(cg.text());
		p->setFont(boldFont);
		QString s1 = FileTransDlg::tr("File") + ": ";
		QString s2 = FileTransDlg::tr("To") + ": ";
		QString s3 = FileTransDlg::tr("From") + ": ";

		int lw = QMAX(QMAX(fmbold.width(s1), fmbold.width(s2)), fmbold.width(s3));
		int left = tw - lw;
		p->drawText(tm, tt, s1);
		p->drawText(tm, tb, sending ? s2 : s3);
		p->setFont(font);
		p->drawText(tm + lw, tt, chopString(name, fm, left));
		p->drawText(tm + lw, tb, chopString(peer, fm, left));

		// rate
		p->setFont(boldFont);
		s1 = FileTransDlg::tr("Status") + ": ";
		lw = fmbold.width(s1);
		left = pw - lw;
		p->drawText(px, tb, s1);
		p->setFont(font);
		p->drawText(px + lw, tb, chopString(rate, fm, left));

		// progress bar
		drawProgressBar(p, cg, px, m, pw, ph);

		delete p;
		mp->drawPixmap(0, 0, pix);
	}

	QString makeTip() const
	{
		QTime t = QTime().addSecs(timeRemaining);
		QString rem = FileTransDlg::tr("%1h%2m%3s").arg(t.hour()).arg(t.minute()).arg(t.second());

		QString s;
		s += FileTransDlg::tr("Filename") + QString(": %1").arg(name);
		s += QString("\n") + FileTransDlg::tr("Type") + QString(": %1").arg(sending ? FileTransDlg::tr("Upload") : FileTransDlg::tr("Download"));
		s += QString("\n") + FileTransDlg::tr("Peer") + QString(": %1").arg(peer);
		s += QString("\n") + FileTransDlg::tr("Size") + QString(": %1").arg(size);
		if(done) {
			s += QString("\n") + FileTransDlg::tr("[Done]");
		}
		else {
			s += QString("\n") + FileTransDlg::tr("Transferred") + QString(": %1").arg(sent);
			if(bps > 0)
				s += QString("\n") + FileTransDlg::tr("Time remaining") + QString(": %1").arg(rem);
		}

		return s;
	}
};

class FileTransView : public Q3ListView
{
	Q_OBJECT
public:
	FileTransView(QWidget *parent=0, const char *name=0)
	:Q3ListView(parent, name)
	{
		connect(this, SIGNAL(contextMenuRequested(Q3ListViewItem *, const QPoint &, int)), this, SLOT(qlv_contextMenuRequested(Q3ListViewItem *, const QPoint &, int)));
	}

	bool maybeTip(const QPoint &pos)
	{
		FileTransItem *i = static_cast<FileTransItem*>(itemAt(pos));
		if(!i)
			return false;
		QRect r(itemRect(i));
		PsiToolTip::showText(mapToGlobal(pos), i->makeTip(), this);
		return true;
	}

	void resizeEvent(QResizeEvent *e)
	{
		Q3ListView::resizeEvent(e);

		if(e->oldSize().width() != e->size().width())
			doResize();
	}
	
	bool event(QEvent* e)
	{
		if (e->type() == QEvent::ToolTip) {
			QPoint pos = ((QHelpEvent*) e)->pos();
			e->setAccepted(maybeTip(pos));
			return true;
		}
		return Q3ListView::event(e);
	}

signals:
	void itemCancel(int id);
	void itemOpenDest(int id);
	void itemClear(int id);

private slots:
	void qlv_contextMenuRequested(Q3ListViewItem *item, const QPoint &pos, int)
	{
		if(!item)
			return;

		FileTransItem *i = static_cast<FileTransItem*>(item);

		Q3PopupMenu p;
		p.insertItem(tr("&Cancel"), 0);
		p.insertSeparator();
		//p.insertItem(tr("&Open Destination Folder"), 1);
		p.insertItem(tr("Cl&ear"), 2);

		if(i->done) {
			p.setItemEnabled(0, false);
		}
		else {
			//p.setItemEnabled(1, false);
			p.setItemEnabled(2, false);
		}

		int x = p.exec(pos);

		// TODO: what if item is deleted during exec?

		if(x == 0) {
			if(!i->done)
				itemCancel(i->id);
		}
		else if(x == 1)
			itemOpenDest(i->id);
		else if(x == 2)
			itemClear(i->id);
	}

private:
	void doResize()
	{
		Q3ListViewItemIterator it(this);
		for(Q3ListViewItem *i; (i = it.current()); ++it)
			i->setup();
	}
};

class TransferMapping
{
public:
	FileTransferHandler *h;
	int id;
	int p;
	qlonglong sent;

	int at;
	qlonglong last[10];

	TransferMapping()
	{
		h = 0;
		at = 0;
	}

	~TransferMapping()
	{
		delete h;
	}

	void logSent()
	{
		// if we're at the end, shift down to make room
		if(at == 10) {
			for(int n = 0; n < at - 1; ++n)
				last[n] = last[n + 1];
			--at;
		}
		last[at++] = sent;
	}
};

class FileTransDlg::Private
{
public:
	FileTransDlg *parent;
	PsiCon *psi;
	FileTransView *lv;
	Q3PtrList<TransferMapping> transferList;
	QTimer t;

	Private(FileTransDlg *_parent)
	{
		parent = _parent;
		transferList.setAutoDelete(true);
	}

	int findFreeId()
	{
		int n = 0;
		while(1) {
			bool found = false;
			Q3ListViewItemIterator it(lv);
			for(Q3ListViewItem *i; (i = it.current()); ++it) {
				FileTransItem *fi = static_cast<FileTransItem*>(i);
				if(fi->id == n) {
					found = true;
					break;
				}
			}
			if(!found)
				break;
			++n;
		}
		return n;
	}

	FileTransItem *findItem(int id)
	{
		Q3ListViewItemIterator it(lv);
		for(Q3ListViewItem *i; (i = it.current()); ++it) {
			FileTransItem *fi = static_cast<FileTransItem*>(i);
			if(fi->id == id)
				return fi;
		}
		return 0;
	}

	Q3PtrList<FileTransItem> getFinished()
	{
		Q3PtrList<FileTransItem> list;
		Q3ListViewItemIterator it(lv);
		for(Q3ListViewItem *i; (i = it.current()); ++it) {
			FileTransItem *fi = static_cast<FileTransItem*>(i);
			if(fi->done)
				list.append(fi);
		}
		return list;
	}

	TransferMapping *findMapping(FileTransferHandler *h)
	{
		Q3PtrListIterator<TransferMapping> it(transferList);
		for(TransferMapping *i; (i = it.current()); ++it) {
			if(i->h == h)
				return i;
		}
		return 0;
	}

	TransferMapping *findMapping(int id)
	{
		Q3PtrListIterator<TransferMapping> it(transferList);
		for(TransferMapping *i; (i = it.current()); ++it) {
			if(i->id == id)
				return i;
		}
		return 0;
	}

	void updateProgress(TransferMapping *i, bool updateAll=true)
	{
		bool done = (i->p == i->h->totalSteps());

		// calculate bps
		int bps = -1;
		if(!done && i->at >= 2) {
			int seconds = i->at - 1;
			qlonglong average = i->last[i->at-1] - i->last[0];
			bps = ((int)average) / seconds;
		}

		if(done) {
			FileTransItem *fi = findItem(i->id);
			fi->done = true;
		}

		parent->setProgress(i->id, i->p, i->h->totalSteps(), i->sent, bps, updateAll);

		if(done) {
			bool recv = (i->h->mode() == FileTransferHandler::Receiving);
			QString fname, savename;
			if(recv) {
				fname = i->h->fileName();
				savename = i->h->saveName();
			}

			PsiAccount *pa = i->h->account();
			transferList.removeRef(i);

			if(recv) {
				//printf("fname: [%s], savename: [%s]\n", fname.latin1(), savename.latin1());

				// rename .part to original filename
				QFileInfo fi(savename);
				QDir dir = fi.dir();
				if(dir.exists(fname))
					dir.remove(fname);
				if(!dir.rename(fi.fileName(), fname)) {
					// TODO: display some error about renaming
				}
			}

			pa->playSound(PsiOptions::instance()->getOption("options.ui.notifications.sounds.completed-file-transfer").toString());
		}
	}
};

FileTransDlg::FileTransDlg(PsiCon *psi)
:AdvancedWidget(0, psi_dialog_flags)
{
	d = new Private(this);
	d->psi = psi;
	//d->psi->dialogRegister(this);

	connect(&d->t, SIGNAL(timeout()), SLOT(updateItems()));

	setWindowTitle(tr("Transfer Manager"));
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/filemanager").icon());
#endif

	QVBoxLayout *vb = new QVBoxLayout(this, 11, 6);
	d->lv = new FileTransView(this);
	connect(d->lv, SIGNAL(itemCancel(int)), SLOT(itemCancel(int)));
	connect(d->lv, SIGNAL(itemOpenDest(int)), SLOT(itemOpenDest(int)));
	connect(d->lv, SIGNAL(itemClear(int)), SLOT(itemClear(int)));
	vb->addWidget(d->lv);
	QHBoxLayout *hb = new QHBoxLayout(vb);
	hb->addStretch(1);
	QPushButton *pb_clear = new QPushButton(tr("Clear &Finished"), this);
	connect(pb_clear, SIGNAL(clicked()), SLOT(clearFinished()));
	hb->addWidget(pb_clear);
	QPushButton *pb_close = new QPushButton(tr("&Hide"), this);
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	hb->addWidget(pb_close);

	pb_close->setDefault(true);
	pb_close->setFocus();

	d->lv->addColumn("");
	d->lv->header()->hide();

	d->lv->setResizeMode(Q3ListView::LastColumn);
	d->lv->setAllColumnsShowFocus(true);
	d->lv->setSorting(-1);

	resize(560, 240);
}

FileTransDlg::~FileTransDlg()
{
	//d->psi->dialogUnregister(this);
	delete d;
}

int FileTransDlg::addItem(const QString &filename, qlonglong size, const QString &peer, bool sending)
{
	int id = d->findFreeId();
	FileTransItem *i = new FileTransItem(d->lv, filename, size, peer, sending);
	if(sending)
		i->icon = IconsetFactory::icon("psi/upload").impix().pixmap();
	else
		i->icon = IconsetFactory::icon("psi/download").impix().pixmap();
	i->id = id;
	d->t.start(1000);
	return id;
}

void FileTransDlg::setProgress(int id, int step, int total, qlonglong sent, int bytesPerSecond, bool updateAll)
{
	FileTransItem *i = d->findItem(id);
	if(i) {
		// convert steps/total into a word
		int progress;
		if(total > 0)
			progress = step * 8192 / total;
		else
			progress = 8192;

		bool do_repaint = i->setProgress(progress, sent, bytesPerSecond);
		if(updateAll) {
			i->updateRate();
			do_repaint = true;
		}
		if(do_repaint)
			i->repaint();
	}
}

void FileTransDlg::removeItem(int id)
{
	FileTransItem *i = d->findItem(id);
	if(i)
		delete i;
	if(d->lv->childCount() == 0)
		d->t.stop();
}

void FileTransDlg::setError(int id, const QString &reason)
{
	FileTransItem *i = d->findItem(id);
	if(i) {
		i->done = true;
		i->error = reason;
		i->updateRate();
		i->repaint();
		show();
		d->lv->ensureItemVisible(i);
		QMessageBox::information(this, tr("Transfer Error"), tr("Transfer of %1 with %2 failed.\nReason: %3").arg(i->name).arg(i->peer).arg(reason));
	}
}

void FileTransDlg::takeTransfer(FileTransferHandler *h, int p, qlonglong sent)
{
	QString peer;
	UserListItem *u = h->account()->findFirstRelevant(h->peer());
	if(u)
		peer = JIDUtil::nickOrJid(u->name(),u->jid().full());
	else
		peer = h->peer().full();

	TransferMapping *i = new TransferMapping;
	i->h = h;
	i->id = addItem(h->fileName(), h->fileSize(), peer, (h->mode() == FileTransferHandler::Sending));
	i->p = p;
	i->sent = sent;
	d->transferList.append(i);

	FileTransItem *fi = d->findItem(i->id);
	d->lv->ensureItemVisible(fi);

	if(p == i->h->totalSteps()) {
		d->updateProgress(i, true);
	}
	else {
		connect(h, SIGNAL(progress(int, qlonglong)), SLOT(ft_progress(int, qlonglong)));
		connect(h, SIGNAL(error(int, int, const QString &)), SLOT(ft_error(int, int, const QString &)));
	}
}

void FileTransDlg::clearFinished()
{
	Q3PtrList<FileTransItem> list = d->getFinished();
	{
		// remove related transfer mappings
		Q3PtrListIterator<FileTransItem> it(list);
		for(FileTransItem *fi; (fi = it.current()); ++it) {
			TransferMapping *i = d->findMapping(fi->id);
			d->transferList.removeRef(i);
		}
	}
	list.setAutoDelete(true);
	list.clear();
}

void FileTransDlg::ft_progress(int p, qlonglong sent)
{
	TransferMapping *i = d->findMapping((FileTransferHandler *)sender());
	i->p = p;
	i->sent = sent;
	if(p == i->h->totalSteps())
		d->updateProgress(i, true);
	else
		d->updateProgress(i, false);
}

void FileTransDlg::ft_error(int x, int, const QString &s)
{
	TransferMapping *i = d->findMapping((FileTransferHandler *)sender());
	int id = i->id;
	d->transferList.removeRef(i);

	QString str;
	//if(x == FileTransferHandler::ErrReject)
	//	str = tr("File was rejected by remote user.");
	if(x == FileTransferHandler::ErrTransfer)
		str = s;
	else
		str = tr("File I/O error (%1)").arg(s);
	setError(id, str);
}

void FileTransDlg::updateItems()
{
	// operate on a copy so that we can delete items in updateProgress
	Q3PtrList<TransferMapping> list = d->transferList;
	Q3PtrListIterator<TransferMapping> it(list);
	for(TransferMapping *i; (i = it.current()); ++it) {
		if(i->h) {
			i->logSent();
			d->updateProgress(i);
		}
	}
}

void FileTransDlg::itemCancel(int id)
{
	FileTransItem *fi = d->findItem(id);
	TransferMapping *i = d->findMapping(id);
	d->transferList.removeRef(i);
	delete fi;
}

void FileTransDlg::itemOpenDest(int id)
{
	TransferMapping *i = d->findMapping(id);

	QString path;
	bool recv = (i->h->mode() == FileTransferHandler::Receiving);
	if(recv)
		path = QFileInfo(i->h->saveName()).dirPath();
	else
		path = QFileInfo(i->h->fileName()).dirPath();

	//printf("item open dest: [%s]\n", path.latin1());
}

void FileTransDlg::itemClear(int id)
{
	FileTransItem *fi = d->findItem(id);
	TransferMapping *i = d->findMapping(id);
	d->transferList.removeRef(i);
	delete fi;
}

void FileTransDlg::killTransfers(PsiAccount *pa)
{
	Q3PtrList<TransferMapping> list = d->transferList;
	Q3PtrListIterator<TransferMapping> it(list);
	for(TransferMapping *i; (i = it.current()); ++it) {
		// this account?
		if(i->h->account() == pa) {
			FileTransItem *fi = d->findItem(i->id);
			d->transferList.removeRef(i);
			delete fi;
		}
	}
}

#include "filetransdlg.moc"
