/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING file for the detailed license.
 */

#ifndef CERTIFICATEERRORDIALOG_H
#define CERTIFICATEERRORDIALOG_H

#include <QtCrypto>
#include <QString>

class QMessageBox;
class QPushButton;

class CertificateErrorDialog
{
	public:
		CertificateErrorDialog(const QString& title, const QString& host, const QCA::Certificate& cert, int result, QCA::Validity validity, const QString& certsSaveDir);

		QMessageBox* getMessageBox() {
			return messageBox_;
		}

		int exec();
	
	private:
		QMessageBox* messageBox_;
		QPushButton* detailsButton_;
		QPushButton* continueButton_;
		QPushButton* cancelButton_;
		QPushButton* saveButton_;
		QCA::Certificate certificate_;
		int result_;
		QCA::Validity validity_;
		QString certsSaveDir_;
		QString host_;
};

#endif
