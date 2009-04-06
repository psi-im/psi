#ifndef FILETRANSDLG_H
#define FILETRANSDLG_H

#include "advwidget.h"
#include "ui_filetrans.h"
#include "s5b.h"

class PsiCon;
class PsiAccount;
namespace XMPP {
	class FileTransfer;
	class Jid;
}
using namespace XMPP;

class FileTransferHandler : public QObject
{
	Q_OBJECT
public:
	enum { ErrReject, ErrTransfer, ErrFile };
	enum { Sending, Receiving };
	FileTransferHandler(PsiAccount *pa, FileTransfer *ft=0);
	~FileTransferHandler();

	PsiAccount *account() const;
	int mode() const;
	Jid peer() const;
	QString fileName() const;
	qlonglong fileSize() const;
	QString description() const;
	qlonglong offset() const;
	int totalSteps() const;
	bool resumeSupported() const;
	QString saveName() const;

	void send(const Jid &to, const QString &fname, const QString &desc);
	void accept(const QString &saveName, const QString &fileName, qlonglong offset=0);

signals:
	void accepted();
	void statusMessage(const QString &s);
	void connected();
	void progress(int p, qlonglong sent);
	void error(int, int, const QString &s);

private slots:
	// s5b status
	void s5b_proxyQuery();
	void s5b_proxyResult(bool b);
	void s5b_requesting();
	void s5b_accepted();
	void s5b_tryingHosts(const StreamHostList &hosts);
	void s5b_proxyConnect();
	void s5b_waitingForActivation();

	// ft
	void ft_accepted();
	void ft_connected();
	void ft_readyRead(const QByteArray &);
	void ft_bytesWritten(int);
	void ft_error(int);
	void trySend();
	void doFinish();

private:
	class Private;
	Private *d;

	void mapSignals();
};

class FileRequestDlg : public QDialog, public Ui::FileTrans
{
	Q_OBJECT
public:
	FileRequestDlg(const Jid &j, PsiCon *psi, PsiAccount *pa);
	FileRequestDlg(const Jid &j, PsiCon *psi, PsiAccount *pa, const QStringList& files, bool direct = false);
	FileRequestDlg(const QDateTime &ts, FileTransfer *ft, PsiAccount *pa);
	~FileRequestDlg();

protected:
	void keyPressEvent(QKeyEvent *);

public slots:
	void done(int r);

private slots:
	void updateIdentity(PsiAccount *);
	void updateIdentityVisibility();
	void pa_disconnected();
	void chooseFile();
	void doStart();
	void ft_accepted();
	void ft_statusMessage(const QString &s);
	void ft_connected();
	void ft_error(int, int, const QString &);
	void t_timeout();

private:
	class Private;
	Private *d;

	void blockWidgets();
	void unblockWidgets();
};

class FileTransDlg : public AdvancedWidget<QDialog>
{
	Q_OBJECT
public:
	FileTransDlg(PsiCon *);
	~FileTransDlg();

	int addItem(const QString &filename, qlonglong size, const QString &peer, bool sending);
	void setProgress(int id, int step, int total, qlonglong sent, int bytesPerSecond, bool updateAll=false);
	void setError(int id, const QString &reason);
	void removeItem(int id);

	void takeTransfer(FileTransferHandler *h, int p, qlonglong sent);
	void killTransfers(PsiAccount *pa);

private slots:
	void clearFinished();
	void ft_progress(int p, qlonglong sent);
	void ft_error(int, int, const QString &s);
	void updateItems();

	void itemCancel(int);
	void itemOpenDest(int);
	void itemClear(int);

private:
	class Private;
	Private *d;
};

#endif
