#ifndef SSLCERTDLG_H
#define SSLCERTDLG_H

#include "ui_sslcert.h"
#include <QList>
#include <QtCrypto>

class SSLCertDlg : public QDialog, public Ui::SSLCert
{
	Q_OBJECT
public:
	SSLCertDlg(QWidget *parent=0);

	void setCert(const QCA::Certificate &, int result, QCA::Validity);

	static void showCert(const QCA::Certificate &, int result, QCA::Validity);

private:
	QString makePropTable(const QString &heading, const QCA::CertificateInfo &props);
};

#endif
