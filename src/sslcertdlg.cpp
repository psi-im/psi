#include "sslcertdlg.h"

#include <q3textbrowser.h>
#include <qdatetime.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <q3dict.h>
#include <qca.h>
#include "common.h"
#include "iconwidget.h"
#include "certutil.h"

static void setLabelStatus(QLabel *l, bool ok)
{
	if(ok)
		l->setPaletteForegroundColor(QColor("#2A993B"));
	else
		l->setPaletteForegroundColor(QColor("#810000"));
}

static QString makePropEntry(QCA::CertificateInfoType var, const QString &name, const QCA::CertificateInfo &list)
{
	QString val;
	QList<QString> values = list.values(var);
	for (int i = 0; i < values.size(); ++i)
		val += values.at(i) + "<br>";

	if(val.isEmpty())
		return "";
	else
		return QString("<tr><td><nobr><b>") + name + "</b></nobr></td><td>" + val + "</td></tr>";
}

QString SSLCertDlg::makePropTable(const QString &heading, const QCA::CertificateInfo &list)
{
	QString str;
	str += "<tr><td><i>" + heading + "</i><br>";
	str += "<table>";
	str += makePropEntry(QCA::Organization, tr("Organization:"), list);
	str += makePropEntry(QCA::OrganizationalUnit, tr("Organizational unit:"), list);
	str += makePropEntry(QCA::Locality, tr("Locality:"), list);
	str += makePropEntry(QCA::State, tr("State:"), list);
	str += makePropEntry(QCA::Country, tr("Country:"), list);
	str += makePropEntry(QCA::CommonName, tr("Common name:"), list);
	str += makePropEntry(QCA::Email, tr("Email:"), list);
	str += "</table></td></tr>";
	return str;
}

SSLCertDlg::SSLCertDlg(QWidget *parent)
:QDialog(parent)
{
	setupUi(this);
	setModal(true);
	setWindowTitle(CAP(caption()));

	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	pb_close->setDefault(true);
	pb_close->setFocus();
}

void SSLCertDlg::setCert(const QCA::Certificate &cert, int result, QCA::Validity validity)
{
	if(cert.isNull())
		return;

	if(result == QCA::TLS::Valid) {
		lb_valid->setText(tr("The certificate is valid."));
		setLabelStatus(lb_valid, true);
	}
	else {
		lb_valid->setText(tr("The certificate is NOT valid!") + "\n" + QString(tr("Reason: %1.")).arg(CertUtil::resultToString(result, validity)));
		setLabelStatus(lb_valid, false);
	}

	QDateTime now = QDateTime::currentDateTime();
	QDateTime notBefore = cert.notValidBefore();
	QDateTime notAfter = cert.notValidAfter();
	lb_notBefore->setText(cert.notValidBefore().toString());
	setLabelStatus(lb_notBefore, now > notBefore);
	lb_notAfter->setText(cert.notValidAfter().toString());
	setLabelStatus(lb_notAfter, now < notAfter);

	lb_sn->setText(cert.serialNumber().toString());

	QString str;
	str += "<table>";
	str += makePropTable(tr("Subject Details:"), cert.subjectInfo());
	str += makePropTable(tr("Issuer Details:"), cert.issuerInfo());
	str += "</table>";
	tb_cert->setText(str);
}

void SSLCertDlg::showCert(const QCA::Certificate &certificate, int result, QCA::Validity validity)
{
	SSLCertDlg *w = new SSLCertDlg(0);
	w->setCert(certificate, result, validity);
	w->exec();
	delete w;
}
