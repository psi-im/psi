#include "filetransdlg.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QPainter>
#include <QDesktopServices>
#include <QFileIconProvider>
#include <QProcess>
#include <QMenu>
#include <QKeyEvent>
#include <QBuffer>

#include "psicon.h"
#include "psiaccount.h"
#include "userlist.h"
#include "s5b.h"
#include "busywidget.h"
#include "filetransfer.h"
#include "psiiconset.h"
#include "psitextview.h"
#include "jidutil.h"
#include "psitooltip.h"
#include "psicontactlist.h"
#include "accountscombobox.h"
#include "accountlabel.h"
#include "psioptions.h"
#include "fileutil.h"
#include "xmpp_tasks.h"

typedef quint64 LARGE_TYPE;

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
	activeFiles->removeAll(s);
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
	BSConnection *c;
	Jid peer;
	QString fileName, saveName;
	QString filePath;
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
	d->filePath = fname;
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

	// try to make thumbnail
	QImage img(fname);
	XMPP::FTThumbnail thumb;
	if (!img.isNull()) {
		QByteArray ba;
		QBuffer buffer(&ba);
		buffer.open(QIODevice::WriteOnly);
		img = img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		img.save(&buffer, "PNG");
		thumb = XMPP::FTThumbnail(ba, "image/png", img.width(), img.height());
	}

	d->ft->sendFile(d->peer, d->fileName, d->fileSize, desc, thumb);
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

QString FileTransferHandler::filePath() const
{
	return d->filePath;
}

qlonglong FileTransferHandler::fileSize() const
{
	return d->fileSize;
}

QPixmap FileTransferHandler::fileIcon() const
{
	QPixmap icon;
	QFileIconProvider provider;
	QFileInfo file(d->filePath);
	if (file.exists()){
		icon = provider.icon(file).pixmap(32, 32);
	}
	return icon;
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
	d->filePath = saveName;
	d->filePath.chop(5);
	d->offset = offset;
	d->length = d->fileSize;
	d->f.setFileName(saveName);
	d->ft->accept(offset);
}

void FileTransferHandler::s5b_proxyQuery()
{
	statusMessage(tr("Querying proxy..."));
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

	d->c = d->ft->bsConnection();

	if (dynamic_cast<S5BConnection*>(d->c)) {
		connect(d->c, SIGNAL(proxyQuery()), SLOT(s5b_proxyQuery()));
		connect(d->c, SIGNAL(proxyResult(bool)), SLOT(s5b_proxyResult(bool)));
		connect(d->c, SIGNAL(requesting()), SLOT(s5b_requesting()));
		connect(d->c, SIGNAL(accepted()), SLOT(s5b_accepted()));
		connect(d->c, SIGNAL(tryingHosts(const StreamHostList &)), SLOT(s5b_tryingHosts(const StreamHostList &)));
		connect(d->c, SIGNAL(proxyConnect()), SLOT(s5b_proxyConnect()));
		connect(d->c, SIGNAL(waitingForActivation()), SLOT(s5b_waitingForActivation()));
	}

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
				if(d->f.seek(d->offset))
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
				if(d->f.seek(d->offset))
					ok = true;
			}
		}
		if(!ok) {
			delete d->ft;
			d->ft = 0;
			error(ErrFile, 0, d->f.errorString());
			return;
		}

		d->activeFile = d->f.fileName();
		active_file_add(d->activeFile);

		// done already?  this means a file size of zero
		if(d->sent == d->fileSize)
			QTimer::singleShot(0, this, SLOT(doFinish()));
	}

	emit connected();
}

void FileTransferHandler::ft_readyRead(const QByteArray &a)
{
	if(!d->sending) {
		//printf("%d bytes read\n", a.size());
		int r = d->f.write(a.data(), a.size());
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

void FileTransferHandler::ft_bytesWritten(qint64 x)
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
	if(!d->ft->bsConnection())
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
	connect(d->ft, SIGNAL(bytesWritten(qint64)), SLOT(ft_bytesWritten(qint64)));
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
	PsiTextView *te;
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

	QFrame *hb = new QFrame(this);
	QHBoxLayout *hbLayout = new QHBoxLayout(hb);
	hbLayout->setMargin(0);
	d->lb_identity = new QLabel(tr("Identity: "), hb);
	hbLayout->addWidget(d->lb_identity);
	d->cb_ident = d->psi->accountsComboBox(hb);
	hbLayout->addWidget(d->cb_ident);
	connect(d->cb_ident, SIGNAL(activated(PsiAccount *)), SLOT(updateIdentity(PsiAccount *)));
	d->cb_ident->setAccount(pa);
	replaceWidget(lb_accountlabel, hb);
	setTabOrder(d->cb_ident, le_to);
	connect(d->psi, SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	d->te = new PsiTextView(this);
	d->te->setReadOnly(false);
	d->te->setAcceptRichText(false);
	replaceWidget(te_desc, d->te);
	setTabOrder(le_fname, d->te);
	setTabOrder(d->te, pb_stop);

	setWindowTitle(tr("Send File"));
#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/upload").icon());
#endif

	lb_thumbnail->hide();
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

		FileUtil::setLastUsedSavePath(fi.path());
		le_fname->setText(QDir::toNativeSeparators(fi.filePath()));
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
	QFrame *hb = new QFrame(this);
	QHBoxLayout *hbLayout = new QHBoxLayout(hb);
	hbLayout->setMargin(0);
	d->lb_identity = new QLabel(tr("Identity: "), hb);
	hbLayout->addWidget(d->lb_identity);
	d->lb_ident = new AccountLabel(hb);
	d->lb_ident->setAccount(d->pa);
	d->lb_ident->setSizePolicy(QSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed ));
	hbLayout->addWidget(d->lb_ident);
	hbLayout->addWidget(new QLabel(tr("Time:")));
	d->lb_time = new QLabel(ts.time().toString(Qt::LocalDate), hb);
	d->lb_time->setFrameStyle( QFrame::Panel | QFrame::Sunken );
	hbLayout->addWidget(d->lb_time);
	connect(d->pa->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	replaceWidget(lb_accountlabel, hb);

	d->te = new PsiTextView(this);
	d->te->setAcceptRichText(false);
	replaceWidget(te_desc, d->te);
	setTabOrder(le_fname, d->te);
	setTabOrder(d->te, pb_stop);

	lb_to->setText(tr("From:"));
	setWindowTitle(tr("Receive File"));
#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/download").icon());
#endif

	le_to->setText(d->jid.full());
	le_fname->setText(d->fileName);
	lb_size->setText(tr("%1 byte(s)").arg(d->fileSize));
	d->te->setReadOnly(true);
	d->te->setText(ft->description());
	pb_start->setText(tr("&Accept"));
	pb_stop->setText(tr("&Reject"));

	lb_thumbnail->hide();
	tb_browse->hide();

	if (!ft->thumbnail().isNull()) {
		lb_thumbnail->resize(ft->thumbnail().width, ft->thumbnail().height);
		JT_BitsOfBinary *task = new JT_BitsOfBinary(pa->client()->rootTask());
		connect(task, SIGNAL(finished()), SLOT(thumbnailReceived()));
		task->get(ft->peer(), QString(ft->thumbnail().data));
		task->go(true);
	}

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
		le_fname->setText(QDir::toNativeSeparators(fi.filePath()));
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
			QMessageBox::information(this, tr("Error"), tr("The XMPP address specified is not valid.  Correct this and try again."));
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
		d->ft->send(le_to->text(), le_fname->text(), d->te->getPlainText());
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

		d->t.setSingleShot(true);
		d->t.start(30000);

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

void FileRequestDlg::thumbnailReceived()
{
	JT_BitsOfBinary *task = static_cast<JT_BitsOfBinary *>(sender());
	BoBData data = task->data();
	if (!data.data().isNull()) {
		lb_thumbnail->show();
		QPixmap pix;
		pix.loadFromData(data.data());
		lb_thumbnail->setPixmap(pix);
	}
}

//----------------------------------------------------------------------------
// ColumnWidthManager
//----------------------------------------------------------------------------
class ColumnWidthManager
{
public:
	ColumnWidthManager()
	{
		reset();
	}

	void reset()
	{
		curItemWidth = 0;
		calcWidths.fill(0, 3);
		reqWidthsMax.fill(0, 3);
	}

	int getColumnWidth(int col, int itemWidth = -1)
	{
		if (col < 0 || col > 2)
			return 0;

		if (itemWidth != -1 && curItemWidth != itemWidth) {
			curItemWidth = itemWidth;
			calculateWidth();
		}
		return calcWidths[col];
	}

	void setRequiredWidth(int col, int w)
	{
		if (col < 0 || col > 2)
			return;

		if (w > reqWidthsMax[col]) {
			reqWidthsMax[col] = w;
			calculateWidth();
		}
	}

private:
	void calculateWidth()
	{
		calcWidths[0] = reqWidthsMax[0];
		int col1 = 0;
		int col2 = 0;
		int w = curItemWidth - calcWidths[0];
		if (w > 0) {
			col1 = w / 3;
			col2 = w - col1;
			int diff = w - reqWidthsMax[1] - reqWidthsMax[2];
			if (diff >= 0) {
				if (col2 > diff + reqWidthsMax[2]) {
					col2 = diff + reqWidthsMax[2];
				}
				else if (col1 < reqWidthsMax[1]) {
					col2 = w - reqWidthsMax[1];
				}
			}
			else if (col2 > reqWidthsMax[2]) {
				col2 = reqWidthsMax[2];
			}
			col1 = w - col2;
		}
		calcWidths[1] = col1;
		calcWidths[2] = col2;
	}

	int curItemWidth;
	QVector<int> calcWidths; // cache
	QVector<int> reqWidthsMax;
};

//----------------------------------------------------------------------------
// FileTransDlg
//----------------------------------------------------------------------------
class FileTransItem : public QListWidgetItem
{
public:
	QPixmap icon, fileicon;
	bool sending;
	QString name;
	QString path;
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
	int height;
	int margin;
	ColumnWidthManager *cm;

	FileTransItem(QListWidget *parent, const QString &_name, const QString &_path, qlonglong _size, const QString &_peer, bool _sending)
	:QListWidgetItem(parent)
	{
		done = false;
		sending = _sending;
		name = _name;
		path = _path;
		size = _size;
		peer = _peer;
		rate = FileTransDlg::tr("N/A");
		sent = 0;
		progress = 0;
		dist = -1;
		margin = 4;
		cm = 0;
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
		return cm->getColumnWidth(2);
	}

	int progressBarDist(int width) const
	{
		int xsize = width - 2;
		return (progress * xsize / 8192);
	}

	void drawProgressBar(QPainter *p, /*const QColorGroup &cg,*/ int x, int y, int width, int height) const
	{
		p->save();
		QPalette palette = listWidget()->palette();
		if(isSelected())
			p->setPen(palette.color(QPalette::HighlightedText));
		else
			p->setPen(palette.color(QPalette::Text));
		p->drawRect(x, y, width, height);
		int xoff = x + 1;
		int yoff = y + 1;
		int xsize = width - 1;
		int ysize = height - 1;

		int dist = progressBarDist(width) + 1;
		p->fillRect(xoff, yoff, dist, ysize, palette.brush(QPalette::Highlight));
		if (dist < xsize)
			p->fillRect(xoff + dist, yoff, xsize - dist, ysize, palette.brush(QPalette::Base));

		int percent = progress * 100 / 8192;
		QString s = QString::number(percent) + '%';

		QFontMetrics fm(p->font());
		int ty = ((height - fm.height()) / 2) + fm.ascent() + y;
		int textwidth = fm.width(s);
		int center = xoff + (xsize / 2);

		p->save();
		p->setPen(palette.color(QPalette::HighlightedText));
		p->setClipRect(xoff, yoff, dist, ysize);
		p->drawText(center - (textwidth / 2), ty, s);
		p->restore();

		p->save();
		p->setPen(palette.color(QPalette::Text));
		p->setClipRect(xoff + dist, yoff, width - 2 - dist, ysize);
		p->drawText(center - (textwidth / 2), ty, s);
		p->restore();
		p->restore();
	}

	void setup()
	{
		//widthChanged();
		QListWidget *lv = listWidget();

		QFontMetrics fm = lv->fontMetrics();
		int m = 4;
		int pm = 2;
		int ph = fm.height() + 2 + (pm * 2);
		int h = (ph * 2) + (m * 3);

		h += 2; //lv->itemMargin() * 2;

		// ensure an even number
		if(h & 1)
			++h;

		height = h;
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

	void paintCell(QPainter *mp, const QStyleOptionViewItem& option)
	{
		mp->save();
		QRect rect = option.rect;
		int w = rect.width();
		int h = rect.height();
		int _w = w - margin * 3;

		QPixmap pix(w, h);
		QPainter *p = new QPainter(&pix);
		QFont font = option.font;
		QFont boldFont = font;
		boldFont.setBold(true);
		QFontMetrics fm(font);
		QFontMetrics fmbold(boldFont);
		QBrush br;
		QPalette palette = option.palette;

		int col0 = cm->getColumnWidth(0, _w);
		int col1 = cm->getColumnWidth(1, _w);
		int col2 = cm->getColumnWidth(2, _w);

		// pm = progress margin, ph = progress height, yoff = text y offset,
		// tt = text top, tb = text bottom, pw = progress width, px = progress x coord
		int pm = 2;
		int ph = fm.height() + 2 + (pm * 2);
		int yoff = 1 + pm;
		int tt = margin + yoff + fm.ascent();
		int tb = (margin * 2) + ph + yoff + fm.ascent();
		int px = (margin * 2) + col0 + col1;
		int pw = col2;

		// clear out the background
		if(option.state & QStyle::State_Selected)
			br = palette.brush(QPalette::Highlight);
		else
			br = palette.brush(QPalette::Base);
		p->fillRect(0, 0, w, h, br);

		// icon
		if(!fileicon.isNull()) {
			int sz = col0 - 4;
			QPixmap fIcon = fileicon.scaled(sz, sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			p->drawPixmap(margin, margin + yoff, fIcon);
			p->drawPixmap(margin + sz + 1 - icon.width(), margin + yoff, icon);
		}
		else {
			p->drawPixmap(margin, margin + yoff, icon);
		}

		// filename / peer
		QString flabel = FileTransDlg::tr("File") + ": ";
		QString plabel = (sending ? FileTransDlg::tr("To") : FileTransDlg::tr("From")) + ": ";
		if(option.state & QStyle::State_Selected)
			p->setPen(palette.color(QPalette::HighlightedText));
		else
			p->setPen(palette.color(QPalette::Text));
		int tm = margin + col0;
		p->setFont(boldFont);
		p->drawText(tm, tt, flabel);
		p->drawText(tm, tb, plabel);
		_w = qMax(fmbold.width(flabel), fmbold.width(plabel));
		int left = col1 - _w;
		p->setFont(font);
		p->drawText(tm + _w, tt, chopString(name, fm, left));
		p->drawText(tm + _w, tb, chopString(peer, fm, left));

		// rate
		QString slabel = FileTransDlg::tr("Status") + ": ";
		_w = fmbold.width(slabel);
		p->setFont(boldFont);
		p->drawText(px, tb, slabel);
		left = pw - _w;
		p->setFont(font);
		p->drawText(px + _w, tb, chopString(rate, fm, left));

		// progress bar
		drawProgressBar(p, px, margin, pw, ph);

		delete p;
		mp->drawPixmap(rect, pix);
		mp->restore();
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

	int neededColumnWidth(int col) const
	{
		if (col == 0) {
			return (height - (margin + 1) + icon.width() / 2) + 4;
		}
		if (col == 1) {
			QFont fbold = font();
			fbold.setBold(true);
			QFontMetrics fm(fbold);
			QString fl = FileTransDlg::tr("File") + ": ";
			QString pl = (sending ? FileTransDlg::tr("To") : FileTransDlg::tr("From")) + ": ";
			int w = qMax(fm.width(fl), fm.width(pl));
			fm = QFontMetrics(font());
			w += qMax(fm.width(name), fm.width(peer));
			return w;
		}
		if (col == 2) {
			QFont fbold = font();
			fbold.setBold(true);
			QFontMetrics fm(fbold);
			int w = fm.width(FileTransDlg::tr("Status") + ": ");
			fm = QFontMetrics(font());
			w += fm.width(rate);
			return w;
		}
		return 0;
	}
};

class FileTransView : public QListWidget
{
	Q_OBJECT
public:
	ColumnWidthManager *cm;

	FileTransView(QWidget *parent=0, const char *name=0)
	:QListWidget(parent)
	{
		Q_UNUSED(name)
		cm = new ColumnWidthManager();
		//connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(qlv_contextMenuRequested(QPoint)));
		setItemDelegate(new FileTransDelegate(this));
	}

	~FileTransView()
	{
		delete cm;
	}

	bool maybeTip(const QPoint &pos)
	{
		FileTransItem *i = static_cast<FileTransItem*>(itemAt(pos));
		if(!i)
			return false;
//		QRect r(visualItemRect(i));
		PsiToolTip::showText(mapToGlobal(pos), i->makeTip(), this);
		return true;
	}

	void resizeEvent(QResizeEvent *e)
	{
		QListWidget::resizeEvent(e);

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
		return QListWidget::event(e);
	}

signals:
	void itemCancel(int id);
	void itemOpenDest(int id);
	void itemClear(int id);


protected:
	void mousePressEvent(QMouseEvent *event)
	{
		if(event->button() == Qt::RightButton) {
			event->accept();
			qlv_contextMenuRequested(event->pos());
			return;
		}
		QListWidget::mousePressEvent(event);
	}

private:
	void qlv_contextMenuRequested(const QPoint &pos)
	{
		FileTransItem *i = static_cast<FileTransItem*>(itemAt(pos));

		if(!i)
			return;

		QMenu p;
		p.addAction(tr("&Cancel"));			// index = 0
		p.addSeparator();				// index = 1
		p.addAction(tr("&Open Containing Folder"));	// index = 2
		p.addAction(tr("Cl&ear"));			// index = 3

		if(i->done) {
			p.actions().at(0)->setEnabled(false);
		}
		else {
			//p.actions().at(2)->setEnabled(false);
			p.actions().at(3)->setEnabled(false);
		}

		QAction* act = p.exec(QCursor::pos());
		int x;
		if(act) {
			x = p.actions().indexOf(act);
		}
		else {
			return;
		}

		// TODO: what if item is deleted during exec?

		if(x == 0) {
			if(!i->done)
				emit itemCancel(i->id);
		}
		else if(x == 2)
			emit itemOpenDest(i->id);
		else if(x == 3)
			emit itemClear(i->id);
	}

	void doResize()
	{
		for(int i = 0; i < count(); i++) {
			static_cast<FileTransItem*>(item(i))->setup();
		}
		viewport()->update();
	}
};




FileTransDelegate::FileTransDelegate(QObject* p)
	: QItemDelegate(p)
	, ftv(0)
{
	ftv = static_cast<FileTransView*>(p);
}

void FileTransDelegate::paint(QPainter* mp, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	FileTransItem* fti = static_cast<FileTransItem*>(ftv->item(index.row()));
	fti->paintCell(mp, option);
}

QSize FileTransDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QSize sz = QItemDelegate::sizeHint(option, index);
	FileTransItem* fti = static_cast<FileTransItem*>(ftv->item(index.row()));
	sz.setHeight(fti->height);
	return sz;
}



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
	QList<TransferMapping*> transferList;
	QTimer t;

	Private(FileTransDlg *_parent)
	{
		parent = _parent;
	}

	~Private()
	{
		qDeleteAll(transferList);
	}

	int findFreeId()
	{
		int n = 0;
		while(1) {
			bool found = false;
			for(int i = 0; i < lv->count(); i++) {
				QListWidgetItem *it = lv->item(i);
				FileTransItem *fi = static_cast<FileTransItem*>(it);
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
		for(int i = 0; i < lv->count(); i++) {
			QListWidgetItem *it = lv->item(i);
			FileTransItem *fi = static_cast<FileTransItem*>(it);
			if(fi->id == id)
				return fi;
		}
		return 0;
	}

	QList<FileTransItem*> getFinished()
	{
		QList<FileTransItem*> list;
		for(int i = 0; i < lv->count(); i++) {
			QListWidgetItem *it = lv->item(i);
			FileTransItem *fi = static_cast<FileTransItem*>(it);
			if(fi->done)
				list.append(fi);
		}
		return list;
	}

	TransferMapping *findMapping(FileTransferHandler *h)
	{
		QList<TransferMapping*>::iterator it = transferList.begin();
		for(; it != transferList.end(); ++it) {
			if((*it)->h == h)
				return *it;
		}
		return 0;
	}

	TransferMapping *findMapping(int id)
	{
		QList<TransferMapping*>::iterator it = transferList.begin();
		for(; it != transferList.end(); ++it) {
			if((*it)->id == id)
				return *it;
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


		if(done) {
			bool recv = (i->h->mode() == FileTransferHandler::Receiving);
			QString fname, savename;
			if(recv) {
				fname = i->h->fileName();
				savename = i->h->saveName();

				//printf("fname: [%s], savename: [%s]\n", fname.latin1(), savename.latin1());

				// rename .part to original filename
				QFileInfo fi(savename);
				QDir dir = fi.dir();
				if(dir.exists(fname))
					dir.remove(fname);
				if(!dir.rename(fi.fileName(), fname)) {
					// TODO: display some error about renaming
				}
				findItem(i->id)->fileicon = i->h->fileIcon();
			}

			PsiAccount *pa = i->h->account();
			pa->playSound(PsiAccount::eFTComplete);
		}

		parent->setProgress(i->id, i->p, i->h->totalSteps(), i->sent, bps, updateAll);

		if(done) {
			transferList.removeAll(i);
			delete i;
		}
	}
};

FileTransDlg::FileTransDlg(PsiCon *psi)
:AdvancedWidget<QDialog>(0, psi_dialog_flags)
{
	d = new Private(this);
	d->psi = psi;
	//d->psi->dialogRegister(this);

	connect(&d->t, SIGNAL(timeout()), SLOT(updateItems()));

	setWindowTitle(tr("Transfer Manager"));
#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/filemanager").icon());
#endif

	QVBoxLayout *vb = new QVBoxLayout(this);
	vb->setSpacing(6); // FIXME: Is forced spacing really necessary?
	d->lv = new FileTransView(this);
	connect(d->lv, SIGNAL(itemCancel(int)), SLOT(itemCancel(int)));
	connect(d->lv, SIGNAL(itemOpenDest(int)), SLOT(itemOpenDest(int)));
	connect(d->lv, SIGNAL(itemClear(int)), SLOT(itemClear(int)));
	connect(d->lv, SIGNAL(doubleClicked(QModelIndex)), SLOT(openFile(QModelIndex)));
	vb->addWidget(d->lv);
	QHBoxLayout *hb = new QHBoxLayout;
	vb->addLayout(hb);
	hb->addStretch(1);
	QPushButton *pb_clear = new QPushButton(tr("Clear &Finished"), this);
	connect(pb_clear, SIGNAL(clicked()), SLOT(clearFinished()));
	hb->addWidget(pb_clear);
	QPushButton *pb_close = new QPushButton(tr("&Hide"), this);
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	hb->addWidget(pb_close);

	pb_close->setDefault(true);
	pb_close->setFocus();

//	d->lv->addColumn("");
//	d->lv->header()->hide();

	d->lv->setResizeMode(QListWidget::Adjust);
	//d->lv->setAllColumnsShowFocus(true);
	d->lv->setSortingEnabled(false); //->setSorting(-1);

	resize(560, 240);
}

FileTransDlg::~FileTransDlg()
{
	//d->psi->dialogUnregister(this);
	delete d;
}

int FileTransDlg::addItem(const QString &filename, const QString &path, const QPixmap &fileicon, qlonglong size, const QString &peer, bool sending)
{
	int id = d->findFreeId();
	FileTransItem *i = new FileTransItem(d->lv, filename, path, size, peer, sending);
	if(sending)
		i->icon = IconsetFactory::icon("psi/upload").impix().pixmap();
	else
		i->icon = IconsetFactory::icon("psi/download").impix().pixmap();

	i->fileicon = fileicon;

	i->cm = d->lv->cm;

	i->id = id;
	i->setup();

	for (int k = 0; k < 3; ++k)
		d->lv->cm->setRequiredWidth(k, i->neededColumnWidth(k));

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
		if(do_repaint) {
			for (int col = 0; col < 3; ++col)
				d->lv->cm->setRequiredWidth(col, i->neededColumnWidth(col));
			d->lv->viewport()->update();
		}
	}
}

void FileTransDlg::removeItem(int id)
{
	FileTransItem *i = d->findItem(id);
	if(i)
		delete i;
	if(!d->lv->count())
		d->t.stop();
}

void FileTransDlg::setError(int id, const QString &reason)
{
	FileTransItem *i = d->findItem(id);
	if(i) {
		i->done = true;
		i->error = reason;
		i->updateRate();
		d->lv->viewport()->update();
		show();
		d->lv->scrollToItem(i);
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
	i->id = addItem(h->fileName(), h->filePath(), h->fileIcon(), h->fileSize(), peer, (h->mode() == FileTransferHandler::Sending));
	i->p = p;
	i->sent = sent;
	d->transferList.append(i);

	FileTransItem *fi = d->findItem(i->id);
	d->lv->scrollToItem(fi);

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
	QList<FileTransItem*> list = d->getFinished();
	{
		// remove related transfer mappings
		QList<FileTransItem*>::iterator it = list.begin();
		for(; it != list.end(); ++it) {
			FileTransItem *fi = *it;
			TransferMapping *i = d->findMapping(fi->id);
			d->transferList.removeAll(i);
			delete i;
		}
	}
	qDeleteAll(list);
	if (d->lv->count() == 0)
		d->lv->cm->reset();
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
	d->transferList.removeAll(i);
	delete i;

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
	foreach (TransferMapping *i, d->transferList) {
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
	d->transferList.removeAll(i);
	delete i;
	delete fi;
}

void FileTransDlg::itemOpenDest(int id)
{
	FileTransItem *i = d->findItem(id);

#if defined(Q_OS_WIN)
	QProcess::startDetached("explorer.exe", QStringList() << QLatin1String("/select,")
														  << QDir::toNativeSeparators(i->path));
#elif defined(Q_OS_MAC)
	QProcess::execute("/usr/bin/osascript", QStringList()
						<< "-e"
						<< QString("tell application \"Finder\" to reveal POSIX file \"%1\"")
						.arg(i->path));
	QProcess::execute("/usr/bin/osascript", QStringList()
						<< "-e"
						<< "tell application \"Finder\" to activate");
#else
	// we cannot select a file here, because no file browser really supports it...
	const QFileInfo fileInfo(i->path);
	QProcess::startDetached("xdg-open", QStringList(fileInfo.path()));
#endif
	//printf("item open dest: [%s]\n", path.latin1());
}

void FileTransDlg::itemClear(int id)
{
	FileTransItem *fi = d->findItem(id);
	TransferMapping *i = d->findMapping(id);
	d->transferList.removeAll(i);
	delete i;
	delete fi;
	if (d->lv->count() == 0)
		d->lv->cm->reset();
}

void FileTransDlg::openFile(QModelIndex index)
{
	FileTransItem *i = static_cast<FileTransItem*>(d->lv->item(index.row()));
	if (i->done) {
		QFileInfo fi(i->path);
		if(fi.exists()){
			QDesktopServices::openUrl(QUrl::fromLocalFile(i->path));
		}
	}
}

void FileTransDlg::killTransfers(PsiAccount *pa)
{
	QList<TransferMapping*> list = d->transferList;
	QList<TransferMapping*>::iterator it = list.begin();
	for(; it != list.end(); ++it) {
		// this account?
		if((*it)->h->account() == pa) {
			FileTransItem *fi = d->findItem((*it)->id);
			d->transferList.removeAll(*it);
			delete *it;
			delete fi;
		}
	}
}

#include "filetransdlg.moc"
