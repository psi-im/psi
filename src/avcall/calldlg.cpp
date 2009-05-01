#include "calldlg.h"

#include <QtGui>
#include "ui_call.h"
#include "jinglertp.h"
#include "xmpp_client.h"
#include "../psimedia/psimedia.h"
#include "common.h"
#include "psiaccount.h"
#include "psioptions.h"

// from opt_avcall.cpp
extern void options_avcall_update();

// we have this so if the user plugs in a device, but never goes to the
//   options screen to select it, and then starts a call, it'll get used
static void prep_device_opts()
{
	options_avcall_update();
	JingleRtpManager::setAudioOutDevice(PsiOptions::instance()->getOption("options.media.devices.audio-output").toString());
	JingleRtpManager::setAudioInDevice(PsiOptions::instance()->getOption("options.media.devices.audio-input").toString());
	JingleRtpManager::setVideoInDevice(PsiOptions::instance()->getOption("options.media.devices.video-input").toString());
}

class CallDlg::Private : public QObject
{
	Q_OBJECT

public:
	CallDlg *q;
	Ui::Call ui;
	PsiAccount *pa;
	bool incoming;
	bool active;
	bool activated;
	JingleRtpSession *sess;
	PsiMedia::VideoWidget *vw_remote;

	Private(CallDlg *_q) :
		QObject(_q),
		q(_q),
		active(false),
		activated(false),
		sess(0)
	{
		ui.setupUi(q);
		q->setWindowTitle(tr("Voice Call"));

		ui.lb_bandwidth->setEnabled(false);
		ui.cb_bandwidth->setEnabled(false);
		connect(ui.ck_useVideo, SIGNAL(toggled(bool)), ui.lb_bandwidth, SLOT(setEnabled(bool)));
		connect(ui.ck_useVideo, SIGNAL(toggled(bool)), ui.cb_bandwidth, SLOT(setEnabled(bool)));

		ui.cb_bandwidth->addItem(tr("High (1Mbps)"), 1000);
		ui.cb_bandwidth->addItem(tr("Average (400Kbps)"), 400);
		ui.cb_bandwidth->addItem(tr("Low (160Kbps)"), 160);
		ui.cb_bandwidth->setCurrentItem(1);

		if(!JingleRtpManager::isVideoSupported())
		{
			ui.ck_useVideo->hide();
			ui.lb_bandwidth->hide();
			ui.cb_bandwidth->hide();
		}

		connect(ui.pb_accept, SIGNAL(clicked()), SLOT(ok_clicked()));
		connect(ui.pb_reject, SIGNAL(clicked()), SLOT(cancel_clicked()));

		ui.pb_accept->setDefault(true);
		ui.pb_accept->setFocus();

		q->resize(q->minimumSize());
	}

	~Private()
	{
		if(sess)
		{
			sess->setIncomingVideo(0);
			sess->deleteLater();
		}
	}

	void setOutgoing(const XMPP::Jid &jid)
	{
		incoming = false;

		ui.le_to->setText(jid.full());

		ui.pb_reject->setText(tr("&Close"));
		ui.pb_accept->setText(tr("C&all"));
		ui.lb_status->setText(tr("Ready"));
	}

	void setIncoming(JingleRtpSession *_sess)
	{
		incoming = true;
		sess = _sess;
		connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
		connect(sess, SIGNAL(rejected()), SLOT(sess_rejected()));

		ui.lb_to->setText(tr("From:"));
		ui.le_to->setText(sess->jid().full());
		ui.le_to->setReadOnly(true);

		if(sess->mode() == JingleRtpSession::Video || sess->mode() == JingleRtpSession::Both)
		{
			ui.ck_useVideo->setChecked(true);

			// video-only session, don't allow deselecting video
			if(sess->mode() == JingleRtpSession::Video)
				ui.ck_useVideo->setEnabled(false);
		}

		ui.lb_status->setText(tr("Accept call?"));
	}

private slots:
	void ok_clicked()
	{
		JingleRtpSession::Mode mode = JingleRtpSession::Audio;
		int kbps = -1;
		if(ui.ck_useVideo->isChecked())
		{
			mode = JingleRtpSession::Both;
			kbps = ui.cb_bandwidth->itemData(ui.cb_bandwidth->currentIndex()).toInt();
		}

		if(!incoming)
		{
			ui.le_to->setReadOnly(true);
			ui.le_to->setEnabled(false);
			ui.ck_useVideo->setEnabled(false);
			ui.cb_bandwidth->setEnabled(false);

			ui.pb_accept->setEnabled(false);
			ui.pb_reject->setText(tr("&Cancel"));
			ui.pb_reject->setFocus();
			ui.busy->start();
			ui.lb_status->setText(tr("Calling..."));

			sess = pa->jingleRtpManager()->createOutgoing();
			connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
			connect(sess, SIGNAL(rejected()), SLOT(sess_rejected()));

			active = true;
			sess->connectToJid(ui.le_to->text(), mode, kbps);
		}
		else
		{
			ui.le_to->setEnabled(false);
			ui.ck_useVideo->setEnabled(false);
			ui.cb_bandwidth->setEnabled(false);

			ui.pb_accept->setEnabled(false);
			ui.pb_reject->setText(tr("&Cancel"));
			ui.pb_reject->setFocus();
			ui.busy->start();
			ui.lb_status->setText(tr("Accepting..."));

			active = true;
			sess->accept(mode, kbps);
		}
	}

	void cancel_clicked()
	{
		if(sess)
			sess->reject();
		q->close();
	}

	void sess_activated()
	{
		ui.le_to->setEnabled(true);
		ui.lb_bandwidth->hide();
		ui.cb_bandwidth->hide();

		if(sess->mode() == JingleRtpSession::Video || sess->mode() == JingleRtpSession::Both)
		{
			vw_remote = new PsiMedia::VideoWidget(q);
			replaceWidget(ui.ck_useVideo, vw_remote);
			sess->setIncomingVideo(vw_remote);
			vw_remote->setMinimumSize(320, 240);
		}
		else
			ui.ck_useVideo->hide();

		ui.busy->stop();
		ui.busy->hide();
		ui.pb_accept->hide();
		ui.pb_reject->setText(tr("&Hang up"));
		ui.lb_status->setText(tr("Call active"));
		activated = true;
	}

	void sess_rejected()
	{
		ui.busy->stop();

		QMessageBox::information(q, tr("Call ended"), tr("Call was rejected or terminated"));
		q->close();
	}
};

CallDlg::CallDlg(PsiAccount *pa, QWidget *parent) :
	QDialog(parent)
{
	d = new Private(this);
	d->pa = pa;
	prep_device_opts();
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
