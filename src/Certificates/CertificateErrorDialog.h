/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING file for the detailed license.
 */

#ifndef CERTIFICATEERRORDIALOG_H
#define CERTIFICATEERRORDIALOG_H

#include <QDialog>
#include <QString>
#include <QtCrypto>

class CertificateErrorDialog : public QDialog {
    Q_OBJECT
public:
    CertificateErrorDialog(const QString &title, const QString &host, const QCA::Certificate &cert, int result,
                           QCA::Validity validity, const QString &domainOverride, QString &tlsOverrideDomain,
                           QByteArray &tlsOverrideCert, QWidget *parent = nullptr);
    ~CertificateErrorDialog();

private:
    QCA::Certificate certificate_;
    int              result_;
    QCA::Validity    validity_;
    QString &        tlsOverrideDomain_;
    QByteArray &     tlsOverrideCert_;
    QString          domainOverride_;
    QString          host_;
};

#endif // CERTIFICATEERRORDIALOG_H
