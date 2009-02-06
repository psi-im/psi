#include "calldlg.h"

#include <QtGui>
#include "ui_call.h"
#include "jinglertp.h"
#include "xmpp_client.h"
#include "psimedia.h"
#include "common.h"
#include "psiaccount.h"

class CallDlg::Private : public QObject
{
	Q_OBJECT

public:
	CallDlg *q;
	Ui::Call ui;
	PsiAccount *pa;
	bool incoming;
	bool active;
	JingleRtpSession *sess;
	PsiMedia::VideoWidget *vw_remote;

	Private(CallDlg *_q) :
		QObject(_q),
		q(_q),
		active(false),
		sess(0)
	{
		ui.setupUi(q);
		q->setWindowTitle("Voice Call");

		connect(ui.pb_accept, SIGNAL(clicked()), SLOT(ok_clicked()));
		connect(ui.pb_reject, SIGNAL(clicked()), SLOT(cancel_clicked()));
	}

	~Private()
	{
		sess->deleteLater();
	}

	void setOutgoing(const XMPP::Jid &jid)
	{
		incoming = false;
		ui.pb_accept->hide();
		ui.pb_reject->setText("&Cancel");
		ui.lb_status->setText(QString("Calling %1 ...").arg(jid.full()));
		sess = pa->jingleRtpManager()->createOutgoing();
		connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
		connect(sess, SIGNAL(rejected()), SLOT(sess_rejected()));
		sess->connectToJid(jid);
	}

	void setIncoming(JingleRtpSession *_sess)
	{
		incoming = true;
		sess = _sess;
		connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
		connect(sess, SIGNAL(rejected()), SLOT(sess_rejected()));
		ui.lb_status->setText(QString("Accept call from %1 ?").arg(sess->jid().full()));
	}

private slots:
	void ok_clicked()
	{
		sess->accept();
		ui.pb_accept->setEnabled(false);
	}

	void cancel_clicked()
	{
		//if(incoming && !active)
			sess->reject();
		q->close();
	}

	void sess_activated()
	{
		ui.lb_status->setText("Call active!");
		if(incoming)
			ui.pb_accept->hide();

		ui.pb_reject->setText("&Hang up");
		active = true;

		vw_remote = new PsiMedia::VideoWidget(q);
		replaceWidget(ui.lb_status, vw_remote);
		sess->setIncomingVideo(vw_remote);
		vw_remote->setMinimumSize(320, 240);
	}

	void sess_rejected()
	{
		QMessageBox::information(q, "Call ended", "Call was rejected or terminated");
		q->close();
	}
};

CallDlg::CallDlg(PsiAccount *pa, QWidget *parent) :
	QDialog(parent)
{
	d = new Private(this);
	d->pa = pa;
}

CallDlg::~CallDlg()
{
	delete d;
}

void CallDlg::setOutgoing(const XMPP::Jid &jid)
{
	d->setOutgoing(jid);
}

void CallDlg::setIncoming(JingleRtpSession *sess)
{
	d->setIncoming(sess);
}

#include "calldlg.moc"
