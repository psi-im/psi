/*
 * infodlg.cpp - handle vcard
 * Copyright (C) 2001-2002  Justin Karneges
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "infodlg.h"

#include "busywidget.h"
#include "common.h"
#include "desktoputil.h"
#include "discodlg.h"
#include "fileutil.h"
#include "iconset.h"
#include "iconwidget.h"
#include "lastactivitytask.h"
#include "msgmle.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "psirichtext.h"
#include "textutil.h"
#include "userlist.h"
#include "vcardfactory.h"
#include "vcardphotodlg.h"
#include "xmpp_client.h"
#include "xmpp_serverinfomanager.h"
#include "xmpp_tasks.h"
#include "xmpp_vcard.h"

#include <QAction>
#include <QCalendarWidget>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFlags>
#include <QFormLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace XMPP;

class AddressTypeDlg : public QFrame {
public:
    enum AddrType {
        None     = 0,
        Home     = 0x1,
        Work     = 0x2,
        Postal   = 0x4,
        Parcel   = 0x8,
        Dom      = 0x10,
        Intl     = 0x20,
        Pref     = 0x40,
        Voice    = 0x80,
        Fax      = 0x100,
        Pager    = 0x200,
        Msg      = 0x400,
        Cell     = 0x800,
        Video    = 0x1000,
        Bbs      = 0x2000,
        Modem    = 0x4000,
        Isdn     = 0x8000,
        Pcs      = 0x10000,
        Internet = 0x20000,
        X400     = 0x40000
    };
    Q_DECLARE_FLAGS(AddrTypes, AddrType)

    AddrTypes allowedTypes;

    AddressTypeDlg(AddrTypes allowedTypes, QWidget *parent);
    void                      setTypes(AddrTypes types);
    AddressTypeDlg::AddrTypes types() const;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AddressTypeDlg::AddrTypes)

AddressTypeDlg::AddressTypeDlg(AddrTypes allowedTypes, QWidget *parent) : QFrame(parent), allowedTypes(allowedTypes)
{
    setFrameShape(QFrame::StyledPanel);

    QList<QPair<QString, AddrType>> names = {
        { tr("Preferred"), Pref }, { tr("Home"), Home },    { tr("Work"), Work },          { tr("Postal"), Postal },
        { tr("Parcel"), Parcel },  { tr("Domestic"), Dom }, { tr("International"), Intl }, { tr("Voice"), Voice },
        { tr("Fax"), Fax },        { tr("Pager"), Pager },  { tr("Voice Message"), Msg },  { tr("Cell"), Cell },
        { tr("Video"), Video },    { QString("BBS"), Bbs }, { tr("Modem"), Modem },        { QString("ISDN"), Isdn },
        { QString("PCS"), Pcs },   { QString(), Internet }, { QString("X.400"), X400 }
    };

    auto lt = new QVBoxLayout();
    for (auto const &p : names) {
        if (!(allowedTypes & p.second))
            continue;

        auto ck = new QCheckBox(p.first);
        ck->setProperty("addrtype", int(p.second));
        lt->addWidget(ck);
    }
    setLayout(lt);
}

void AddressTypeDlg::setTypes(AddrTypes types)
{

    for (auto w : findChildren<QCheckBox *>()) {
        w->setChecked(bool(types & w->property("addrtype").toInt()));
    }
}

AddressTypeDlg::AddrTypes AddressTypeDlg::types() const
{
    AddrTypes t;
    for (auto w : findChildren<QCheckBox *>()) {
        t |= (w->isChecked() ? AddrType(w->property("addrtype").toInt()) : None);
    }
    return t;
}

class InfoWidget::Private {
public:
    Private() = default;

    int                       type       = 0;
    int                       actionType = 0;
    Jid                       jid;
    VCard                     vcard;
    PsiAccount *              pa         = nullptr;
    bool                      busy       = false;
    bool                      te_edited  = false;
    bool                      cacheVCard = false;
    JT_VCard *                jt         = nullptr;
    QByteArray                photo;
    QDate                     bday;
    QString                   dateTextFormat;
    QList<QString>            infoRequested;
    QPointer<QDialog>         showPhotoDlg;
    QPointer<QFrame>          namesDlg;
    QPointer<AddressTypeDlg>  emailsDlg;
    QPointer<QPushButton>     noBdayButton;
    QPointer<QFrame>          bdayPopup;
    QPointer<QCalendarWidget> calendar;
    QLineEdit *               le_givenname   = nullptr;
    QLineEdit *               le_middlename  = nullptr;
    QLineEdit *               le_familyname  = nullptr;
    QAction *                 homepageAction = nullptr;

    // Fake UserListItem for groupchat contacts.
    // One day this dialog should be rewritten not to talk directly to psiaccount,
    // but for now this will somehow work...

    UserListItem *userListItem;

    // use instead of pa->findRelevant(j)
    QList<UserListItem *> findRelevant(const Jid &j) const
    {
        if (userListItem) {
            return QList<UserListItem *>() << userListItem;
        } else {
            return pa->findRelevant(j);
        }
    }

    // use instead of pa->find(j)
    UserListItem *find(const Jid &j) const
    {
        if (userListItem) {
            return userListItem;
        } else {
            return pa->find(j);
        }
    }

    // use instead of pa->contactProfile()->updateEntry(u)
    void updateEntry(const UserListItem &u)
    {
        if (!userListItem) {
            pa->updateEntry(u);
        }
    }
};

InfoWidget::InfoWidget(int type, const Jid &j, const VCard &vcard, PsiAccount *pa, QWidget *parent, bool cacheVCard) :
    QWidget(parent)
{
    ui_.setupUi(this);
    d            = new Private;
    d->type      = type;
    d->jid       = j;
    d->vcard     = vcard ? vcard : VCard::makeEmpty();
    d->pa        = pa;
    d->busy      = false;
    d->te_edited = false;
    d->jt        = nullptr;
    d->pa->dialogRegister(this, j);
    d->cacheVCard     = cacheVCard;
    d->dateTextFormat = "d MMM yyyy";

    setWindowTitle(d->jid.full());
#ifndef Q_OS_MAC
    setWindowIcon(IconsetFactory::icon("psi/vCard").icon());
#endif
    // names editing dialog
    d->namesDlg = new QFrame(this);
    d->namesDlg->setFrameShape(QFrame::StyledPanel);
    QFormLayout *fl  = new QFormLayout;
    d->le_givenname  = new QLineEdit(d->namesDlg);
    d->le_middlename = new QLineEdit(d->namesDlg);
    d->le_familyname = new QLineEdit(d->namesDlg);
    fl->addRow(tr("First Name:"), d->le_givenname);
    fl->addRow(tr("Middle Name:"), d->le_middlename);
    fl->addRow(tr("Last Name:"), d->le_familyname);
    d->namesDlg->setLayout(fl);
    // end names editing dialog

    QAction *editnames = new QAction(IconsetFactory::icon(d->type == Self ? "psi/options" : "psi/info").icon(),
                                     d->type == Self ? tr("Edit names") : tr("View names"), this);
    ui_.le_fullname->addAction(editnames);
    ui_.le_fullname->widgetForAction(editnames)->setPopup(d->namesDlg);
    d->homepageAction = new QAction(IconsetFactory::icon("psi/arrowRight").icon(), tr("Open web browser"), this);
    d->homepageAction->setToolTip(tr("Open web browser"));
    d->homepageAction->setVisible(false);
    ui_.le_homepage->addAction(d->homepageAction);
    connect(d->homepageAction, SIGNAL(triggered()), SLOT(goHomepage()));

    d->emailsDlg = new AddressTypeDlg(
        AddressTypeDlg::Home | AddressTypeDlg::Work | AddressTypeDlg::X400 | AddressTypeDlg::Pref, this);
    QAction *editaddr = new QAction(IconsetFactory::icon(d->type == Self ? "psi/options" : "psi/info").icon(),
                                    d->type == Self ? tr("Edit") : tr("Details"), this);
    ui_.le_email->addAction(editaddr);
    ui_.le_email->widgetForAction(editaddr)->setPopup(d->emailsDlg);

    connect(ui_.te_desc, SIGNAL(textChanged()), this, SLOT(textChanged()));
    connect(ui_.pb_open, SIGNAL(clicked()), this, SLOT(selectPhoto()));
    connect(ui_.pb_clear, SIGNAL(clicked()), this, SLOT(clearPhoto()));
    connect(ui_.tb_photo, SIGNAL(clicked()), SLOT(showPhoto()));
    // connect(editnames, SIGNAL(triggered()), d->namesDlg, SLOT(show()));

    if (d->type == Self || d->type == MucAdm) {
        d->bdayPopup = new QFrame(this);
        d->bdayPopup->setFrameShape(QFrame::StyledPanel);
        QVBoxLayout *vbox = new QVBoxLayout(d->bdayPopup);

        d->calendar = new QCalendarWidget(d->bdayPopup);
        d->calendar->setFirstDayOfWeek(QLocale().firstDayOfWeek());
        d->calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        vbox->addWidget(d->calendar);

        QFrame *line = new QFrame(d->bdayPopup);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        vbox->addWidget(line);

        d->noBdayButton = new QPushButton(IconsetFactory::icon("psi/cancel").icon(), tr("No date"), d->bdayPopup);
        d->noBdayButton->setCheckable(true);
        vbox->addWidget(d->noBdayButton);

        d->bdayPopup->setLayout(vbox);

        QAction *showcal = new QAction(IconsetFactory::icon("psi/options").icon(), tr("Edit birthday"), this);
        showcal->setToolTip(tr("Edit birthday"));
        ui_.le_bday->addAction(showcal);
        ui_.le_bday->widgetForAction(showcal)->setPopup(d->bdayPopup);
        connect(showcal, SIGNAL(triggered()), this, SLOT(doShowCal()));
        connect(d->calendar, SIGNAL(clicked(QDate)), this, SLOT(doUpdateFromCalendar(QDate)));
        connect(d->calendar, SIGNAL(activated(QDate)), this, SLOT(doUpdateFromCalendar(QDate)));
        connect(d->noBdayButton, SIGNAL(clicked()), SLOT(doClearBirthDate()));
    } else {
        // Hide buttons
        ui_.pb_open->hide();
        ui_.pb_clear->hide();
        setReadOnly(true);
    }

    // fake UserListItem used when displaying groupchat contact
    GCContact *gcc = pa->findGCContact(j);
    if (gcc) {
        d->userListItem = new UserListItem(false);
        d->userListItem->setJid(j);
        d->userListItem->setName(j.resource());

        UserResource ur;
        ur.setName(j.resource());
        ur.setStatus(pa->gcContactStatus(j));
        d->userListItem->userResourceList().append(ur);
        d->userListItem->setAvatarFactory(pa->avatarFactory());
    } else {
        d->userListItem = nullptr;
    }

    // Add a status tab
    connect(d->pa->client(), SIGNAL(resourceAvailable(const Jid &, const Resource &)),
            SLOT(contactAvailable(const Jid &, const Resource &)));
    connect(d->pa->client(), SIGNAL(resourceUnavailable(const Jid &, const Resource &)),
            SLOT(contactUnavailable(const Jid &, const Resource &)));
    connect(d->pa, SIGNAL(updateContact(const Jid &)), SLOT(contactUpdated(const Jid &)));
    ui_.te_status->setReadOnly(true);
    ui_.te_status->setAcceptRichText(true);
    PsiRichText::install(ui_.te_status->document());
    updateStatus();
    for (UserListItem *u : d->findRelevant(j)) {
        for (UserResource r : u->userResourceList()) {
            requestResourceInfo(d->jid.withResource(r.name()));
        }
        if (u->userResourceList().isEmpty() && u->lastAvailable().isNull()) {
            requestLastActivity();
        }
    }

    setData(d->vcard);
}

InfoWidget::~InfoWidget()
{
    d->pa->dialogUnregister(this);
    delete d->userListItem;
    d->userListItem = nullptr;
    delete d;
}

/**
 * Redefined so the window does not close when changes are not saved.
 */
bool InfoWidget::aboutToClose()
{

    // don't close if submitting
    if (d->busy && d->actionType == 1) {
        return false;
    }

    if ((d->type == Self || d->type == MucAdm) && edited()) {
        int n = QMessageBox::information(
            this, tr("Warning"),
            d->type == MucAdm
                ? tr("You have not published conference information changes.\nAre you sure you want to discard them?")
                : tr("You have not published your account information changes.\nAre you sure you want to discard "
                     "them?"),
            tr("Close and discard"), tr("Don't close"));
        if (n != 0) {
            return false;
        }
    }

    // cancel active transaction (refresh only)
    if (d->busy && d->actionType == 0) {
        delete d->jt;
        d->jt = nullptr;
    }

    return true;
}

void InfoWidget::jt_finished()
{
    d->jt             = nullptr;
    JT_VCard *jtVCard = static_cast<JT_VCard *>(sender());

    d->busy = false;
    emit released();
    fieldsEnable(true);

    if (jtVCard->success()) {
        if (d->actionType == 0) {
            d->vcard = jtVCard->vcard();
            setData(d->vcard);
        } else if (d->actionType == 1) {
            d->vcard = jtVCard->vcard();
            if (d->cacheVCard)
                VCardFactory::instance()->setVCard(d->jid, d->vcard);
            setData(d->vcard);
        }

        if (d->jid.compare(d->pa->jid(), false)) {
            if (!d->vcard.nickName().isEmpty())
                d->pa->setNick(d->vcard.nickName());
            else
                d->pa->setNick(d->pa->jid().node());
        }

        if (d->actionType == 1)
            QMessageBox::information(this, tr("Success"),
                                     d->type == MucAdm ? tr("Your conference information has been published.")
                                                       : tr("Your account information has been published."));
    } else {
        if (d->actionType == 0) {
            if (d->type == Self)
                QMessageBox::critical(
                    this, tr("Error"),
                    tr("Unable to retrieve your account information.  Perhaps you haven't entered any yet."));
            else if (d->type == MucAdm)
                QMessageBox::critical(this, tr("Error"),
                                      tr("Unable to retrieve information about this conference.\nReason: %1")
                                          .arg(jtVCard->statusString()));
            else
                QMessageBox::critical(
                    this, tr("Error"),
                    tr("Unable to retrieve information about this contact.\nReason: %1").arg(jtVCard->statusString()));
        } else {
            QMessageBox::critical(
                this, tr("Error"),
                tr("Unable to publish your account information.\nReason: %1").arg(jtVCard->statusString()));
        }
    }
}

void InfoWidget::setData(const VCard &i)
{
    d->le_givenname->setText(i.givenName());
    d->le_middlename->setText(i.middleName());
    d->le_familyname->setText(i.familyName());
    ui_.le_nickname->setText(i.nickName());
    d->bday = QDate::fromString(i.bdayStr(), Qt::ISODate);
    if (d->bday.isValid()) {
        ui_.le_bday->setText(d->bday.toString(d->dateTextFormat));
    } else {
        ui_.le_bday->setText(i.bdayStr());
    }
    const QString fullName = i.fullName();
    if (d->type != Self && d->type != MucAdm && fullName.isEmpty()) {
        ui_.le_fullname->setText(QString("%1 %2 %3").arg(i.givenName()).arg(i.middleName()).arg(i.familyName()));
    } else {
        ui_.le_fullname->setText(fullName);
    }

    ui_.le_fullname->setToolTip(QString("<b>") + tr("First Name:") + "</b> " + TextUtil::escape(d->vcard.givenName())
                                + "<br>" + "<b>" + tr("Middle Name:") + "</b> "
                                + TextUtil::escape(d->vcard.middleName()) + "<br>" + "<b>" + tr("Last Name:") + "</b> "
                                + TextUtil::escape(d->vcard.familyName()));

    // E-Mail handling
    VCard::Email email;
    VCard::Email internetEmail;
    for (auto const &e : i.emailList()) {
        if (e.pref) {
            email = e;
        }
        if (e.internet) {
            internetEmail = e;
        }
    }
    if (email.userid.isEmpty() && !i.emailList().isEmpty())
        email = internetEmail.userid.isEmpty() ? i.emailList()[0] : internetEmail;
    ui_.le_email->setText(email.userid);
    AddressTypeDlg::AddrTypes addTypes;
    addTypes |= (email.pref ? AddressTypeDlg::Pref : AddressTypeDlg::None);
    addTypes |= (email.home ? AddressTypeDlg::Home : AddressTypeDlg::None);
    addTypes |= (email.work ? AddressTypeDlg::Work : AddressTypeDlg::None);
    addTypes |= (email.internet ? AddressTypeDlg::Internet : AddressTypeDlg::None);
    addTypes |= (email.x400 ? AddressTypeDlg::X400 : AddressTypeDlg::None);
    d->emailsDlg->setTypes(addTypes);

    ui_.le_homepage->setText(i.url());
    d->homepageAction->setVisible(!i.url().isEmpty());

    QString phone;
    if (!i.phoneList().isEmpty())
        phone = i.phoneList()[0].number;
    ui_.le_phone->setText(phone);

    VCard::Address addr;
    if (!i.addressList().isEmpty())
        addr = i.addressList()[0];
    ui_.le_street->setText(addr.street);
    ui_.le_ext->setText(addr.extaddr);
    ui_.le_city->setText(addr.locality);
    ui_.le_state->setText(addr.region);
    ui_.le_pcode->setText(addr.pcode);
    ui_.le_country->setText(addr.country);

    ui_.le_orgName->setText(i.org().name);

    QString unit;
    if (!i.org().unit.isEmpty())
        unit = i.org().unit[0];
    ui_.le_orgUnit->setText(unit);

    ui_.le_title->setText(i.title());
    ui_.le_role->setText(i.role());
    ui_.te_desc->setPlainText(i.desc());

    if (!i.photo().isEmpty()) {
        // printf("There is a picture...\n");
        d->photo = i.photo();
        if (!updatePhoto()) {
            clearPhoto();
        }
    } else {
        clearPhoto();
    }

    setEdited(false);
}

void InfoWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!d->vcard.photo().isEmpty()) {
        // printf("There is a picture...\n");
        d->photo = d->vcard.photo();
        updatePhoto();
    }
}

bool InfoWidget::updatePhoto()
{
    const QImage img = QImage::fromData(d->photo);
    if (img.isNull()) {
        return false;
    }
    int max_width  = ui_.tb_photo->width() - 10;  // FIXME: Ugly magic number
    int max_height = ui_.tb_photo->height() - 10; // FIXME: Ugly magic number

    ui_.tb_photo->setIcon(QPixmap::fromImage(img));
    ui_.tb_photo->setIconSize(QSize(max_width, max_height));
    return true;
}

void InfoWidget::fieldsEnable(bool x)
{
    ui_.le_fullname->setEnabled(x);
    d->le_givenname->setEnabled(x);
    d->le_middlename->setEnabled(x);
    d->le_familyname->setEnabled(x);
    ui_.le_nickname->setEnabled(x);
    ui_.le_bday->setEnabled(x);
    ui_.le_email->setEnabled(x);
    ui_.le_homepage->setEnabled(x);
    ui_.le_phone->setEnabled(x);
    ui_.pb_open->setEnabled(x);
    ui_.pb_clear->setEnabled(x);

    ui_.le_street->setEnabled(x);
    ui_.le_ext->setEnabled(x);
    ui_.le_city->setEnabled(x);
    ui_.le_state->setEnabled(x);
    ui_.le_pcode->setEnabled(x);
    ui_.le_country->setEnabled(x);

    ui_.le_orgName->setEnabled(x);
    ui_.le_orgUnit->setEnabled(x);
    ui_.le_title->setEnabled(x);
    ui_.le_role->setEnabled(x);
    ui_.te_desc->setEnabled(x);

    setEdited(false);
}

void InfoWidget::setEdited(bool x)
{
    ui_.le_fullname->setModified(x);
    d->le_givenname->setModified(x);
    d->le_middlename->setModified(x);
    d->le_familyname->setModified(x);
    ui_.le_nickname->setModified(x);
    ui_.le_bday->setModified(x);
    ui_.le_email->setModified(x);
    ui_.le_homepage->setModified(x);
    ui_.le_phone->setModified(x);
    ui_.le_street->setModified(x);
    ui_.le_ext->setModified(x);
    ui_.le_city->setModified(x);
    ui_.le_state->setModified(x);
    ui_.le_pcode->setModified(x);
    ui_.le_country->setModified(x);
    ui_.le_orgName->setModified(x);
    ui_.le_orgUnit->setModified(x);
    ui_.le_title->setModified(x);
    ui_.le_role->setModified(x);

    d->te_edited = x;
}

bool InfoWidget::edited()
{
    bool x = false;

    if (ui_.le_fullname->isModified())
        x = true;
    if (d->le_givenname->isModified())
        x = true;
    if (d->le_middlename->isModified())
        x = true;
    if (d->le_familyname->isModified())
        x = true;
    if (ui_.le_nickname->isModified())
        x = true;
    if (ui_.le_bday->isModified())
        x = true;
    if (ui_.le_email->isModified())
        x = true;
    if (ui_.le_homepage->isModified())
        x = true;
    if (ui_.le_phone->isModified())
        x = true;
    if (ui_.le_street->isModified())
        x = true;
    if (ui_.le_ext->isModified())
        x = true;
    if (ui_.le_city->isModified())
        x = true;
    if (ui_.le_state->isModified())
        x = true;
    if (ui_.le_pcode->isModified())
        x = true;
    if (ui_.le_country->isModified())
        x = true;
    if (ui_.le_orgName->isModified())
        x = true;
    if (ui_.le_orgUnit->isModified())
        x = true;
    if (ui_.le_title->isModified())
        x = true;
    if (ui_.le_role->isModified())
        x = true;
    if (d->te_edited)
        x = true;

    return x;
}

void InfoWidget::setReadOnly(bool x)
{
    ui_.le_fullname->setReadOnly(x);
    d->le_givenname->setReadOnly(x);
    d->le_middlename->setReadOnly(x);
    d->le_familyname->setReadOnly(x);
    ui_.le_nickname->setReadOnly(x);
    // ui_.le_bday->setReadOnly(x); //always read only. use calendar
    ui_.le_email->setReadOnly(x);
    ui_.le_homepage->setReadOnly(x);
    ui_.le_phone->setReadOnly(x);
    ui_.le_street->setReadOnly(x);
    ui_.le_ext->setReadOnly(x);
    ui_.le_city->setReadOnly(x);
    ui_.le_state->setReadOnly(x);
    ui_.le_pcode->setReadOnly(x);
    ui_.le_country->setReadOnly(x);
    ui_.le_orgName->setReadOnly(x);
    ui_.le_orgUnit->setReadOnly(x);
    ui_.le_title->setReadOnly(x);
    ui_.le_role->setReadOnly(x);
    ui_.te_desc->setReadOnly(x);
}

void InfoWidget::doRefresh()
{
    if (!d->pa->checkConnected(this))
        return;
    if (d->busy)
        return;

    fieldsEnable(false);

    d->actionType = 0;
    emit busy();

    d->jt = VCardFactory::instance()->getVCard(
        d->jid, d->pa->client()->rootTask(), this, [this]() { jt_finished(); }, d->cacheVCard, d->type == MucContact);
}

void InfoWidget::publish()
{
    if (!d->pa->checkConnected(this)) {
        return;
    }
    if (!(d->type == Self || d->type == MucAdm)) {
        return;
    }
    if (d->busy) {
        return;
    }

    VCard submit_vcard = makeVCard();

    fieldsEnable(false);

    d->actionType = 1;
    emit busy();

    if (d->type == MucAdm) {
        VCardFactory::instance()->setTargetVCard(d->pa, submit_vcard, d->jid, this, SLOT(jt_finished()));
    } else {
        VCardFactory::instance()->setVCard(d->pa, submit_vcard, this, SLOT(jt_finished()));
    }
}

PsiAccount *InfoWidget::account() const { return d->pa; }

const Jid &InfoWidget::jid() const { return d->jid; }

void InfoWidget::doShowCal()
{
    d->noBdayButton->setChecked(ui_.le_bday->text().isEmpty());
    if (d->bday.isValid()) {
        d->calendar->setSelectedDate(d->bday);
    }
}

void InfoWidget::doUpdateFromCalendar(const QDate &date)
{
    if (d->bday != date) {
        d->bday = date;
        ui_.le_bday->setText(date.toString(d->dateTextFormat));
        ui_.le_bday->setModified(true);
    }
    d->bdayPopup->hide();
}

void InfoWidget::doClearBirthDate()
{
    if (!ui_.le_bday->text().isEmpty()) {
        d->bday = QDate();
        ui_.le_bday->setText("");
        ui_.le_bday->setModified(true);
    }
    d->bdayPopup->hide();
}

VCard InfoWidget::makeVCard()
{
    VCard v = VCard::makeEmpty();

    v.setFullName(ui_.le_fullname->text());
    v.setGivenName(d->le_givenname->text());
    v.setMiddleName(d->le_middlename->text());
    v.setFamilyName(d->le_familyname->text());
    v.setNickName(ui_.le_nickname->text());
    v.setBdayStr(d->bday.isValid() ? d->bday.toString(Qt::ISODate) : ui_.le_bday->text());

    if (!ui_.le_email->text().isEmpty()) {
        VCard::Email email;
        auto         types = d->emailsDlg->types();
        if (types & AddressTypeDlg::Pref) {
            email.pref = true;
            types ^= AddressTypeDlg::Pref;
        }
        if (!types) {
            email.internet = true;
        } else {
            email.internet = bool(types & AddressTypeDlg::Internet);
            email.home     = bool(types & AddressTypeDlg::Home);
            email.work     = bool(types & AddressTypeDlg::Work);
            email.x400     = bool(types & AddressTypeDlg::X400);
        }
        email.userid = ui_.le_email->text();

        VCard::EmailList list;
        list << email;
        v.setEmailList(list);
    }

    v.setUrl(ui_.le_homepage->text());

    if (!ui_.le_phone->text().isEmpty()) {
        VCard::Phone p;
        p.home   = true;
        p.voice  = true;
        p.number = ui_.le_phone->text();

        VCard::PhoneList list;
        list << p;
        v.setPhoneList(list);
    }

    if (!d->photo.isEmpty()) {
        // printf("Adding a pixmap to the vCard...\n");
        v.setPhoto(d->photo);
    }

    if (!ui_.le_street->text().isEmpty() || !ui_.le_ext->text().isEmpty() || !ui_.le_city->text().isEmpty()
        || !ui_.le_state->text().isEmpty() || !ui_.le_pcode->text().isEmpty() || !ui_.le_country->text().isEmpty()) {
        VCard::Address addr;
        addr.home     = true;
        addr.street   = ui_.le_street->text();
        addr.extaddr  = ui_.le_ext->text();
        addr.locality = ui_.le_city->text();
        addr.region   = ui_.le_state->text();
        addr.pcode    = ui_.le_pcode->text();
        addr.country  = ui_.le_country->text();

        VCard::AddressList list;
        list << addr;
        v.setAddressList(list);
    }

    VCard::Org org;

    org.name = ui_.le_orgName->text();

    if (!ui_.le_orgUnit->text().isEmpty()) {
        org.unit << ui_.le_orgUnit->text();
    }

    v.setOrg(org);

    v.setTitle(ui_.le_title->text());
    v.setRole(ui_.le_role->text());
    v.setDesc(ui_.te_desc->toPlainText());

    return v;
}

void InfoWidget::textChanged() { d->te_edited = true; }

/**
 * Opens a file browser dialog, and if selected, calls the setPreviewPhoto with the consecuent path.
 * \see setPreviewPhoto(const QString& path)
 */
void InfoWidget::selectPhoto()
{
    QString str = FileUtil::getInbandImageFileName(this);
    if (!str.isEmpty()) {
        setPreviewPhoto(str);
    }
}

/**
 * Loads the image from the requested URL, and inserts the resized image into the preview box.
 * \param path image file to load
 */
void InfoWidget::setPreviewPhoto(const QString &path)
{
    QFile photo_file(path);
    if (!photo_file.open(QIODevice::ReadOnly))
        return;

    QByteArray photo_data  = photo_file.readAll();
    QImage     photo_image = QImage::fromData(photo_data);
    if (!photo_image.isNull()) {
        d->photo = photo_data;
        updatePhoto();
        d->te_edited = true;
    }
}

/**
 * Clears the preview image box and marks the te_edited signal in the private.
 */
void InfoWidget::clearPhoto()
{
    ui_.tb_photo->setIcon(QIcon());
    ui_.tb_photo->setText(tr("Picture not\navailable"));
    d->photo = QByteArray();

    // the picture changed, so notify there are some changes done
    d->te_edited = true;
}

/**
 * Updates the status info of the contact
 */
void InfoWidget::updateStatus()
{
    UserListItem *u = d->find(d->jid);
    if (u) {
        PsiRichText::setText(ui_.te_status->document(), u->makeDesc());
    } else if (d->jid.node().isEmpty() && d->jid.domain() == d->pa->jid().domain()) { // requesting info for our server.
        // let's add some more stuff..
        static const QMap<QString, QString> transMap {
            { "abuse-addresses", tr("Abuse") },       { "admin-addresses", tr("Administrators") },
            { "feedback-addresses", tr("Feedback") }, { "sales-addresses", tr("Sales") },
            { "security-addresses", tr("Security") }, { "support-addresses", tr("Support") }
        };
        QString                            info;
        QMapIterator<QString, QStringList> it(d->pa->serverInfoManager()->extraServerInfo());
        while (it.hasNext()) {
            auto l  = it.next();
            auto at = transMap.value(l.key());
            if (!at.isEmpty()) {
                info += QLatin1String("<b>") + at + QLatin1String(":</b><br>");
                for (const auto &a : l.value()) {
                    info += QLatin1String("  ") + a + QLatin1String("<br>");
                }
            }
        }
        PsiRichText::setText(ui_.te_status->document(), info);
    } else {
        ui_.te_status->clear();
    }
}

/**
 * Sets the visibility of the status tab
 */
void InfoWidget::setStatusVisibility(bool visible)
{
    // Add/remove tab if necessary
    int index = ui_.tabwidget->indexOf(ui_.tab_status);
    if (index == -1) {
        if (visible)
            ui_.tabwidget->addTab(ui_.tab_status, tr("Status"));
    } else if (!visible) {
        ui_.tabwidget->removeTab(index);
    }
}

/**
 * \brief Requests per-resource information.
 *
 * Gets information about client version and time.
 */
void InfoWidget::requestResourceInfo(const Jid &j)
{
    d->infoRequested += j.full();

    JT_ClientVersion *jcv = new JT_ClientVersion(d->pa->client()->rootTask());
    connect(jcv, SIGNAL(finished()), SLOT(clientVersionFinished()));
    jcv->get(j);
    jcv->go(true);

    JT_EntityTime *jet = new JT_EntityTime(d->pa->client()->rootTask());
    connect(jet, SIGNAL(finished()), SLOT(entityTimeFinished()));
    jet->get(j);
    jet->go(true);
}

void InfoWidget::clientVersionFinished()
{
    JT_ClientVersion *j = static_cast<JT_ClientVersion *>(sender());
    if (j->success()) {
        for (UserListItem *u : d->findRelevant(j->jid())) {
            UserResourceList::Iterator rit   = u->userResourceList().find(j->jid().resource());
            bool                       found = !(rit == u->userResourceList().end());
            if (!found) {
                continue;
            }

            (*rit).setClient(j->name(), j->version(), j->os());
            d->updateEntry(*u);
            updateStatus();
        }
    }
}

void InfoWidget::entityTimeFinished()
{
    JT_EntityTime *j = static_cast<JT_EntityTime *>(sender());
    if (j->success()) {
        for (UserListItem *u : d->findRelevant(j->jid())) {
            UserResourceList::Iterator rit   = u->userResourceList().find(j->jid().resource());
            bool                       found = !(rit == u->userResourceList().end());
            if (!found)
                continue;

            (*rit).setTimezone(j->timezoneOffset());
            d->updateEntry(*u);
            updateStatus();
        }
    }
}

void InfoWidget::requestLastActivity()
{
    LastActivityTask *jla = new LastActivityTask(d->jid.bare(), d->pa->client()->rootTask());
    connect(jla, SIGNAL(finished()), SLOT(requestLastActivityFinished()));
    jla->go(true);
}

void InfoWidget::requestLastActivityFinished()
{
    LastActivityTask *j = static_cast<LastActivityTask *>(sender());
    if (j->success()) {
        for (UserListItem *u : d->findRelevant(d->jid)) {
            u->setLastUnavailableStatus(makeStatus(STATUS_OFFLINE, j->status()));
            u->setLastAvailable(j->time());
            d->updateEntry(*u);
            updateStatus();
        }
    }
}

void InfoWidget::contactAvailable(const Jid &j, const Resource &r)
{
    if (d->jid.compare(j, false)) {
        if (!d->infoRequested.contains(j.withResource(r.name()).full())) {
            requestResourceInfo(j.withResource(r.name()));
        }
    }
}

void InfoWidget::contactUnavailable(const Jid &j, const Resource &r)
{
    if (d->jid.compare(j, false)) {
        d->infoRequested.removeAll(j.withResource(r.name()).full());
    }
}

void InfoWidget::contactUpdated(const XMPP::Jid &j)
{
    if (d->jid.compare(j, false)) {
        updateStatus();
    }
}

void InfoWidget::showPhoto()
{
    if (!d->photo.isEmpty()) {
        QPixmap pixmap;
        if (!d->showPhotoDlg) {
            if (pixmap.loadFromData(d->photo)) {
                d->showPhotoDlg = new ShowPhotoDlg(this, pixmap);
                d->showPhotoDlg->show();
            }
        } else {
            ::bringToFront(d->showPhotoDlg);
        }
    }
}

void InfoWidget::goHomepage()
{
    QString homepage = ui_.le_homepage->text();
    if (!homepage.isEmpty()) {
        if (homepage.indexOf("://") == -1) {
            homepage = "http://" + homepage;
        }
        DesktopUtil::openUrl(homepage);
    }
}

// --------------------------------------------
// InfoDlg
// --------------------------------------------
InfoDlg::InfoDlg(int type, const Jid &j, const VCard &vc, PsiAccount *pa, QWidget *parent, bool cacheVCard)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint
                   | Qt::CustomizeWindowHint);
    setModal(false);
    ui_.setupUi(this);
    iw = new InfoWidget(type, j, vc, pa, parent, cacheVCard);
    ui_.loContents->addWidget(iw);

    if (type == InfoWidget::Self) {
        ui_.pb_disco->hide();
    } else {
        ui_.pb_submit->hide();
    }

    connect(ui_.pb_refresh, SIGNAL(clicked()), iw, SLOT(doRefresh()));
    connect(ui_.pb_refresh, SIGNAL(clicked()), iw, SLOT(updateStatus()));
    connect(ui_.pb_submit, SIGNAL(clicked()), iw, SLOT(publish()));
    connect(ui_.pb_close, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui_.pb_disco, SIGNAL(clicked()), this, SLOT(doDisco()));
    connect(iw, SIGNAL(busy()), SLOT(doBusy()));
    connect(iw, SIGNAL(released()), SLOT(release()));
}

void InfoDlg::closeEvent(QCloseEvent *e)
{
    if (iw->aboutToClose()) {
        e->accept();
    } else {
        e->ignore();
    }
}

void InfoDlg::doDisco()
{
    DiscoDlg *w = new DiscoDlg(iw->account(), iw->jid(), "");
    connect(w, SIGNAL(featureActivated(QString, Jid, QString)), iw->account(),
            SLOT(featureActivated(QString, Jid, QString)));
    w->show();
}

void InfoDlg::doBusy()
{
    ui_.pb_submit->setEnabled(false);
    ui_.pb_refresh->setEnabled(false);
    ui_.pb_close->setEnabled(false);
    ui_.busy->start();
}

void InfoDlg::release()
{
    ui_.pb_refresh->setEnabled(true);
    ui_.pb_submit->setEnabled(true);
    ui_.pb_close->setEnabled(true);
    ui_.busy->stop();
}
