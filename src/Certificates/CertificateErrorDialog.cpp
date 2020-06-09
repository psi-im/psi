/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING file for the detailed license.
 */

#include "Certificates/CertificateErrorDialog.h"

#include "Certificates/CertificateDisplayDialog.h"
#include "Certificates/CertificateHelpers.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

static const QSize imgSize(64, 64);

CertificateErrorDialog::CertificateErrorDialog(const QString &title, const QString &host, const QCA::Certificate &cert,
                                               int result, QCA::Validity validity, const QString &domainOverride,
                                               QString &tlsOverrideDomain, QByteArray &tlsOverrideCert,
                                               QWidget *parent) :
    QDialog(parent),
    certificate_(cert), result_(result), validity_(validity), tlsOverrideDomain_(tlsOverrideDomain),
    tlsOverrideCert_(tlsOverrideCert), domainOverride_(domainOverride), host_(host)
{
    setWindowTitle(title);
    QVBoxLayout *     _layout        = new QVBoxLayout(this);
    QDialogButtonBox *btnBox         = new QDialogButtonBox(this);
    QPushButton *     detailsButton  = new QPushButton(QObject::tr("&Details..."), this);
    QPushButton *     continueButton = new QPushButton(QObject::tr("&Connect anyway"), this);
    QPushButton *     saveButton     = new QPushButton(
        domainOverride.isEmpty() ? QObject::tr("&Trust this certificate") : QObject::tr("&Trust this domain"), this);
    btnBox->addButton(detailsButton, QDialogButtonBox::NoRole);
    btnBox->addButton(continueButton, QDialogButtonBox::AcceptRole);
    btnBox->addButton(saveButton, QDialogButtonBox::AcceptRole);
    btnBox->addButton(QDialogButtonBox::Cancel);
    const QString infoText = QObject::tr("The %1 certificate failed the authenticity test.").arg(host)
        + QString("\n\n%1").arg(CertificateHelpers::resultToString(result, validity));
    QLabel *dlgCaption = new QLabel(infoText, this);
    QLabel *image      = new QLabel(this);
    QIcon   icon(QIcon::fromTheme("dialog-warning", qApp->style()->standardIcon(QStyle::SP_MessageBoxWarning)));
    image->setPixmap(icon.pixmap(imgSize));
    QHBoxLayout *lay = new QHBoxLayout(); // Horizontal layout to add image and caption
    lay->addWidget(image);
    lay->addWidget(dlgCaption);
    lay->addStretch();

    _layout->addLayout(lay);
    _layout->addWidget(btnBox);

    connect(detailsButton, &QPushButton::pressed, this, [this]() {
        CertificateDisplayDialog dlg(certificate_, result_, validity_);
        dlg.exec();
    });
    connect(saveButton, &QPushButton::pressed, this, [this]() {
        if (domainOverride_.isEmpty()) {
            tlsOverrideCert_ = certificate_.toDER();
        } else {
            tlsOverrideDomain_ = domainOverride_;
        }
    });
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CertificateErrorDialog::~CertificateErrorDialog() = default;
