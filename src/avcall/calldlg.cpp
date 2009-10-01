/*
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "calldlg.h"

#include <QtGui>
#include "ui_call.h"
#include "avcall.h"
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
	AvCallManager::setAudioOutDevice(PsiOptions::instance()->getOption("options.media.devices.audio-output").toString());
	AvCallManager::setAudioInDevice(PsiOptions::instance()->getOption("options.media.devices.audio-input").toString());
	AvCallManager::setVideoInDevice(PsiOptions::instance()->getOption("options.media.devices.video-input").toString());
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
	AvCall *sess;
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
		ui.cb_bandwidth->setCurrentIndex(1);

		if(!AvCallManager::isVideoSupported())
		{
			ui.ck_useVideo->hide();
			ui.lb_bandwidth->hide();
			ui.cb_bandwidth->hide();
		}

		connect(ui.pb_accept, SIGNAL(clicked()), SLOT(ok_clicked()));
		connect(ui.pb_reject, SIGNAL(clicked()), SLOT(cancel_clicked()));

		ui.pb_accept->setDefault(true);
		ui.pb_accept->setFocus();

		q->resize(q->minimumSizeHint());
	}

	~Private()
	{
		if(sess)
		{
			if(active)
				sess->reject();

			sess->setIncomingVideo(0);
			sess->disconnect(this);
			sess->unlink();
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

	void setIncoming(AvCall *_sess)
	{
		incoming = true;
		sess = _sess;
		connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
		connect(sess, SIGNAL(error()), SLOT(sess_error()));

		ui.lb_to->setText(tr("From:"));
		ui.le_to->setText(sess->jid().full());
		ui.le_to->setReadOnly(true);

		if(sess->mode() == AvCall::Video || sess->mode() == AvCall::Both)
		{
			ui.ck_useVideo->setChecked(true);

			// video-only session, don't allow deselecting video
			if(sess->mode() == AvCall::Video)
				ui.ck_useVideo->setEnabled(false);
		}

		ui.lb_status->setText(tr("Accept call?"));
	}

private slots:
	void ok_clicked()
	{
		AvCall::Mode mode = AvCall::Audio;
		int kbps = -1;
		if(ui.ck_useVideo->isChecked())
		{
			mode = AvCall::Both;
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

			sess = pa->avCallManager()->createOutgoing();
			connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
			connect(sess, SIGNAL(error()), SLOT(sess_error()));

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
		if(sess && incoming && !active)
			sess->reject();
		q->close();
	}

	void sess_activated()
	{
		ui.le_to->setEnabled(true);
		ui.lb_bandwidth->hide();
		ui.cb_bandwidth->hide();

		if(sess->mode() == AvCall::Video || sess->mode() == AvCall::Both)
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

	void sess_error()
	{
		if(!activated)
			ui.busy->stop();

		QMessageBox::information(q, tr("Call ended"), sess->errorString());
		q->close();
	}
};

CallDlg::CallDlg(PsiAccount *pa, QWidget *parent) :
	QDialog(parent)
{
	d = new Private(this);
	d->pa = pa;
	d->pa->dialogRegister(this);
	prep_device_opts();
}

CallDlg::~CallDlg()
{
	d->pa->dialogUnregister(this);
	delete d;
}

void CallDlg::setOutgoing(const XMPP::Jid &jid)
{
	d->setOutgoing(jid);
}

void CallDlg::setIncoming(AvCall *sess)
{
	d->setIncoming(sess);
}

#include "calldlg.moc"
