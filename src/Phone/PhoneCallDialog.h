#ifndef PHONECALLDIALOG_H
#define PHONECALLDIALOG_H

#include <QDialog>

#include "xmpp/jid/jid.h"
#include "ui_PhoneCall.h"
#include "P2P/Phone/PhoneSession.h"

class PhoneCallDialog : public QDialog
{
		Q_OBJECT

	public:
		PhoneCallDialog(PhoneSession*, QWidget* parent = 0);

		void startCall();

	private slots:
		void handleAcceptClicked();
		void handleRejectClicked();
		void handleTerminateClicked();
		void handleStateChanged(PhoneSession::State state);

	private:
		PhoneSession* session_;
		Ui::PhoneCall ui_;
};

#endif
