#include "Phone/PhoneCallDialog.h"

#include <QDialog>
#include <QLabel>
#include <QPushButton>

#include "P2P/Phone/PhoneSession.h"

PhoneCallDialog::PhoneCallDialog(PhoneSession* session, QWidget* parent)
	: QDialog(parent), session_(session)
{
	setAttribute(Qt::WA_DeleteOnClose);
	ui_.setupUi(this);
	setModal(false);
	
	setWindowTitle(QString(tr("Voice Call (%1)")).arg(session->getPeer().full()));
	
	connect(session_,SIGNAL(stateChanged(PhoneSession::State)), SLOT(handleStateChanged(PhoneSession::State)));
	handleStateChanged(session_->getState());

	connect(ui_.pb_hangup,SIGNAL(clicked()),SLOT(handleTerminateClicked()));
	connect(ui_.pb_accept,SIGNAL(clicked()),SLOT(handleAcceptClicked()));
	connect(ui_.pb_reject,SIGNAL(clicked()),SLOT(handleRejectClicked()));
}

void PhoneCallDialog::startCall()
{
	session_->start();
}

void PhoneCallDialog::handleAcceptClicked()
{
	session_->accept();
}

void PhoneCallDialog::handleRejectClicked()
{
	session_->reject();
}
	
void PhoneCallDialog::handleTerminateClicked()
{
	session_->terminate();
}

void PhoneCallDialog::handleStateChanged(PhoneSession::State state)
{
	switch (state) {
		case PhoneSession::State_Calling:
			ui_.lb_status->setText(tr("Calling"));
			ui_.pb_accept->setEnabled(false);
			ui_.pb_reject->setEnabled(false);
			ui_.pb_hangup->setEnabled(true);
			break;

		case PhoneSession::State_Accepting:
			ui_.lb_status->setText(tr("Accepting"));
			ui_.pb_accept->setEnabled(false);
			ui_.pb_reject->setEnabled(false);
			ui_.pb_hangup->setEnabled(true);
			break;

		case PhoneSession::State_Rejecting:
			ui_.lb_status->setText(tr("Rejecting"));
			ui_.pb_accept->setEnabled(false);
			ui_.pb_reject->setEnabled(false);
			ui_.pb_hangup->setEnabled(false);
			break;

		case PhoneSession::State_Terminating:
			ui_.lb_status->setText(tr("Hanging up"));
			ui_.pb_accept->setEnabled(false);
			ui_.pb_reject->setEnabled(false);
			ui_.pb_hangup->setEnabled(false);
			break;

		case PhoneSession::State_Accepted:
			ui_.lb_status->setText(tr("Accepted"));
			ui_.pb_accept->setEnabled(false);
			ui_.pb_reject->setEnabled(false);
			ui_.pb_hangup->setEnabled(true);
			break;

		case PhoneSession::State_Rejected:
			ui_.lb_status->setText(tr("Rejected"));
			ui_.pb_accept->setEnabled(false);
			ui_.pb_reject->setEnabled(false);
			ui_.pb_hangup->setEnabled(false);
			break;

		case PhoneSession::State_InProgress:
			ui_.lb_status->setText(tr("In progress"));
			ui_.pb_accept->setEnabled(false);
			ui_.pb_reject->setEnabled(false);
			ui_.pb_hangup->setEnabled(true);
			break;

		case PhoneSession::State_Terminated:
			ui_.lb_status->setText(tr("Terminated"));
			ui_.pb_accept->setEnabled(false);
			ui_.pb_reject->setEnabled(false);
			ui_.pb_hangup->setEnabled(false);
			break;
			
		case PhoneSession::State_Incoming:
			ui_.lb_status->setText(tr("Incoming Call"));
			ui_.pb_accept->setEnabled(true);
			ui_.pb_reject->setEnabled(true);
			ui_.pb_hangup->setEnabled(false);
			break;

		default:
			break;
	}
}
