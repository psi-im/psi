#include <QApplication>

#include "Certificates/CertificateErrorDialog.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QCA::Certificate cert;
	CertificateErrorDialog dlg("Certificate error", "untrusted-host.com", cert, QCA::TLS::HostMismatch, QCA::ErrorRejected, ".");
	dlg.exec();
	return app.exec();
}
