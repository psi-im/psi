#include <QMessageBox>
#include <QPushButton>

#include "Certificates/CertificateHelpers.h"
#include "Certificates/CertificateErrorDialog.h"
#include "Certificates/CertificateDisplayDialog.h"

CertificateErrorDialog::CertificateErrorDialog(const QString& title, const QString& host, const QCA::Certificate& cert, int result, QCA::Validity validity) : certificate_(cert), result_(result), validity_(validity)
{
	messageBox_ = new QMessageBox(QMessageBox::Warning, title, QObject::tr("The %1 certificate failed the authenticity test.").arg(host));
	messageBox_->setInformativeText(CertificateHelpers::resultToString(result, validity));

	detailsButton_ = messageBox_->addButton(QObject::tr("&Details..."), QMessageBox::ActionRole);
	continueButton_ = messageBox_->addButton(QObject::tr("Co&ntinue"), QMessageBox::AcceptRole);
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
	}
	return messageBox_->result();
}
