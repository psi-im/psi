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

CertificateErrorDialog::CertificateErrorDialog(const QString& title, const QString& host, const QCA::Certificate& cert, int result, QCA::Validity validity, const QString& certsSaveDir) : certificate_(cert), result_(result), validity_(validity), certsSaveDir_(certsSaveDir), host_(host)
{
	messageBox_ = new QMessageBox(QMessageBox::Warning, title, QObject::tr("The %1 certificate failed the authenticity test.").arg(host));
	messageBox_->setInformativeText(CertificateHelpers::resultToString(result, validity));

	detailsButton_ = messageBox_->addButton(QObject::tr("&Details..."), QMessageBox::ActionRole);
	continueButton_ = messageBox_->addButton(QObject::tr("&Connect anyway"), QMessageBox::AcceptRole);
	saveButton_ = messageBox_->addButton(QObject::tr("Always &trust this server"), QMessageBox::AcceptRole);
	cancelButton_ = messageBox_->addButton(QMessageBox::Cancel);

	messageBox_->setDefaultButton(detailsButton_);
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
			QString fileName = certsSaveDir_ + "/" + host_ + ".crt";
			int i = 0;
			while (QFile(fileName).exists()) {
				fileName = certsSaveDir_ + "/" + host_ + "_" + QString::number(i) + ".crt";
				++i;
			}
			certificate_.toPEMFile(fileName);
			break;
		}
	}
	return messageBox_->result();
}
