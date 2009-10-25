/*
 * Copyright (C) 2008 Remko Troncon
 * See COPYING file for the detailed license.
 */

#include <QtDebug>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>

#include "Certificates/CertificateHelpers.h"
#include "Certificates/CertificateErrorDialog.h"
#include "Certificates/CertificateDisplayDialog.h"

CertificateErrorDialog::CertificateErrorDialog(const QString& title, const QString& host, const QCA::Certificate& cert, int result, QCA::Validity validity, const QString &domainOverride, QString &tlsOverrideDomain, QByteArray &tlsOverrideCert) : certificate_(cert), result_(result), validity_(validity), tlsOverrideDomain_(tlsOverrideDomain), tlsOverrideCert_(tlsOverrideCert), domainOverride_(domainOverride), host_(host)
{
	messageBox_ = new QMessageBox(QMessageBox::Warning, title, QObject::tr("The %1 certificate failed the authenticity test.").arg(host));
	messageBox_->setInformativeText(CertificateHelpers::resultToString(result, validity));

	detailsButton_ = messageBox_->addButton(QObject::tr("&Details..."), QMessageBox::ActionRole);
	continueButton_ = messageBox_->addButton(QObject::tr("&Connect anyway"), QMessageBox::AcceptRole);
	if (domainOverride.isEmpty()) {
		saveButton_ = messageBox_->addButton(QObject::tr("&Trust this certificate"), QMessageBox::AcceptRole);
	} else {
		saveButton_ = messageBox_->addButton(QObject::tr("&Trust this domain"), QMessageBox::AcceptRole);
	}
	cancelButton_ = messageBox_->addButton(QMessageBox::Cancel);

	messageBox_->setDefaultButton(detailsButton_);
}

CertificateErrorDialog::~CertificateErrorDialog()
{
	delete messageBox_;
}

int CertificateErrorDialog::exec()
{
	while (true) {
		messageBox_->exec();
		if (messageBox_->clickedButton() == detailsButton_) {
			messageBox_->setResult(QDialog::Accepted);
			CertificateDisplayDialog dlg(certificate_, result_, validity_);
			dlg.exec();
		}
		else if (messageBox_->clickedButton() == continueButton_) {
			messageBox_->setResult(QDialog::Accepted);
			break;
		}
		else if (messageBox_->clickedButton() == cancelButton_) {
			messageBox_->setResult(QDialog::Rejected);
			break;
		}
		else if (messageBox_->clickedButton() == saveButton_) {
			messageBox_->setResult(QDialog::Accepted);
			if (domainOverride_.isEmpty()) {
				tlsOverrideCert_ = certificate_.toDER();
			} else {
				tlsOverrideDomain_ = domainOverride_;
			}
			break;
		}
	}
	return messageBox_->result();
}

void CertificateErrorDialog::reject()
{
	messageBox_->reject();
}
