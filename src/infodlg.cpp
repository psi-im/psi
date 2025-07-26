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

#include "avatars.h"
#include "busywidget.h"
#include "common.h"
#include "desktoputil.h"
#include "discodlg.h"
#include "fileutil.h"
#include "iconset.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_serverinfomanager.h"
#include "iris/xmpp_tasks.h"
#include "iris/xmpp_vcard.h"
#include "lastactivitytask.h"
#include "msgmle.h"
#include "pepmanager.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "psirichtext.h"
#include "textutil.h"
#include "userlist.h"
#include "vcardfactory.h"
#include "vcardphotodlg.h"
#include "xmpp/xmpp-im/xmpp_caps.h"
#include "xmpp/xmpp-im/xmpp_vcard4.h"

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

static QString k_optionName { "options.ui.save.vcard-info-dialog-size" };

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

    const auto &widgets = findChildren<QCheckBox *>();
    for (auto w : widgets) {
        w->setChecked(bool(types & w->property("addrtype").toInt()));
    }
}

AddressTypeDlg::AddrTypes AddressTypeDlg::types() const
{
    AddrTypes   t;
    const auto &widgets = findChildren<QCheckBox *>();
    for (auto w : widgets) {
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
    VCard4::VCard             vcard;
    PsiAccount               *pa        = nullptr;
    bool                      busy      = false;
    bool                      te_edited = false;
    QByteArray                photo;
    QString                   photoMime;
    QDate                     bday;
    QString                   dateTextFormat;
    QList<QString>            infoRequested;
    QPointer<QDialog>         showPhotoDlg;
    QPointer<QFrame>          namesDlg;
    QPointer<AddressTypeDlg>  emailsDlg;
    QPointer<QPushButton>     noBdayButton;
    QPointer<QFrame>          bdayPopup;
    QPointer<QCalendarWidget> calendar;
    QLineEdit                *le_givenname   = nullptr;
    QLineEdit                *le_middlename  = nullptr;
    QLineEdit                *le_familyname  = nullptr;
    QAction                  *homepageAction = nullptr;

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

InfoWidget::InfoWidget(int type, const Jid &j, const VCard4::VCard &vcard, PsiAccount *pa, QWidget *parent) :
    QWidget(parent)
{
    m_ui.setupUi(this);
    d            = new Private;
    d->type      = type;
    d->jid       = j;
    d->vcard     = vcard;
    d->pa        = pa;
    d->busy      = false;
    d->te_edited = false;
    d->pa->dialogRegister(window(), j);
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
    m_ui.le_fullname->addAction(editnames);
    m_ui.le_fullname->widgetForAction(editnames)->setPopup(d->namesDlg);
    d->homepageAction = new QAction(IconsetFactory::icon("psi/arrowRight").icon(), tr("Open web browser"), this);
    d->homepageAction->setToolTip(tr("Open web browser"));
    d->homepageAction->setVisible(false);
    m_ui.le_homepage->addAction(d->homepageAction);
    connect(d->homepageAction, SIGNAL(triggered()), SLOT(goHomepage()));

    d->emailsDlg = new AddressTypeDlg(
        AddressTypeDlg::Home | AddressTypeDlg::Work | AddressTypeDlg::X400 | AddressTypeDlg::Pref, this);
    QAction *editaddr = new QAction(IconsetFactory::icon(d->type == Self ? "psi/options" : "psi/info").icon(),
                                    d->type == Self ? tr("Edit") : tr("Details"), this);
    m_ui.le_email->addAction(editaddr);
    m_ui.le_email->widgetForAction(editaddr)->setPopup(d->emailsDlg);

    connect(m_ui.te_desc, SIGNAL(textChanged()), this, SLOT(textChanged()));
    connect(m_ui.pb_open, SIGNAL(clicked()), this, SLOT(selectPhoto()));
    connect(m_ui.pb_clear, SIGNAL(clicked()), this, SLOT(clearPhoto()));
    connect(m_ui.tb_photo, SIGNAL(clicked()), SLOT(showPhoto()));
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
        m_ui.le_bday->addAction(showcal);
        m_ui.le_bday->widgetForAction(showcal)->setPopup(d->bdayPopup);
        connect(showcal, SIGNAL(triggered()), this, SLOT(doShowCal()));
        connect(d->calendar, SIGNAL(clicked(QDate)), this, SLOT(doUpdateFromCalendar(QDate)));
        connect(d->calendar, SIGNAL(activated(QDate)), this, SLOT(doUpdateFromCalendar(QDate)));
        connect(d->noBdayButton, SIGNAL(clicked()), SLOT(doClearBirthDate()));
    } else {
        // Hide buttons
        m_ui.pb_open->hide();
        m_ui.pb_clear->hide();
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
        connect(d->pa->client(), &XMPP::Client::groupChatPresence, this, [this](const Jid &j, const Status &s) {
            if (d->jid.compare(j, true)) {
                UserResource ur;
                ur.setName(j.resource());
                ur.setStatus(s);
                UserResourceList url;
                url.append(ur);
                d->userListItem->userResourceList() = url;
                updateStatus();
                requestResourceInfo(j);
                if (s.photoHash()) {
                    // for vcard we need rather immediate update, regardless if pubsub avatar is set.
                    // so don't use AvatarFactory and use VCardFactory directly. who knows what's else changed
                    VCardFactory::instance()->ensureVCardPhotoUpdated(d->pa, j, VCardFactory::MucUser, *s.photoHash());
                }
            }
        });
    } else {
        d->userListItem = nullptr;
    }

    // Add a status tab
    connect(d->pa->client(), SIGNAL(resourceAvailable(const Jid &, const Resource &)),
            SLOT(contactAvailable(const Jid &, const Resource &)));
    connect(d->pa->client(), SIGNAL(resourceUnavailable(const Jid &, const Resource &)),
            SLOT(contactUnavailable(const Jid &, const Resource &)));
    connect(d->pa, SIGNAL(updateContact(const Jid &)), SLOT(contactUpdated(const Jid &)));
    connect(VCardFactory::instance(), &VCardFactory::vcardChanged, this,
            [this](const Jid &j, VCardFactory::Flags flags) {
                if (d->jid.compare(j, flags & VCardFactory::MucUser)) {
                    auto vcard = VCardFactory::instance()->vcard(j, flags);
                    if (vcard) {
                        d->vcard = vcard;
                        setData(d->vcard);
                    }
                    updateNick();
                }
            });
    m_ui.te_status->setReadOnly(true);
    m_ui.te_status->setAcceptRichText(true);
    m_ui.te_status->setKeepAtBottom(false);
    PsiRichText::install(m_ui.te_status->document());
    updateStatus();
    const auto &items = d->findRelevant(j);
    for (UserListItem *u : items) {
        const auto &rList = u->userResourceList();
        for (const UserResource &r : rList) {
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
    d->pa->dialogUnregister(window());
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
            QMessageBox::Discard | QMessageBox::No);
        if (n != QMessageBox::Discard) {
            return false;
        }
    }
    return true;
}

void InfoWidget::setData(const VCard4::VCard &i)
{
    auto names = i.names();
    d->le_givenname->setText(names.data.given.value(0));
    d->le_middlename->setText(names.data.additional.value(0));
    d->le_familyname->setText(names.data.surname.value(0));
    m_ui.le_nickname->setText(i.nickName().preferred().data.value(0));
    d->bday = i.bday();
    m_ui.le_bday->setText(d->bday.toString(d->dateTextFormat));

    const QString fullName = i.fullName().value(0).data;
    if (d->type != MucAdm && fullName.isEmpty()) {
        auto fn = QString("%1 %2 %3")
                      .arg(names.data.given.value(0), names.data.additional.value(0), names.data.surname.value(0))
                      .simplified();
        if (d->type == Self) {
            m_ui.le_fullname->setPlaceholderText(fn);
        } else {
            m_ui.le_fullname->setText(fn);
        }
    } else {
        m_ui.le_fullname->setText(fullName);
    }

    m_ui.le_fullname->setToolTip(QString("<b>") + tr("First Name:") + "</b> "
                                 + TextUtil::escape(names.data.given.value(0)) + "<br>" + "<b>" + tr("Middle Name:")
                                 + "</b> " + TextUtil::escape(names.data.additional.value(0)) + "<br>" + "<b>"
                                 + tr("Last Name:") + "</b> " + TextUtil::escape(names.data.surname.value(0)));

    // E-Mail handling
    auto email = i.emails().preferred();
    m_ui.le_email->setText(email);
    AddressTypeDlg::AddrTypes addTypes;
    addTypes |= (email.parameters.pref ? AddressTypeDlg::Pref : AddressTypeDlg::None);
    addTypes |= (email.parameters.type.contains(QLatin1String("home")) ? AddressTypeDlg::Home : AddressTypeDlg::None);
    addTypes |= (email.parameters.type.contains(QLatin1String("work")) ? AddressTypeDlg::Work : AddressTypeDlg::None);
    d->emailsDlg->setTypes(addTypes);

    QUrl homepage = i.urls();
    m_ui.le_homepage->setText(homepage.toString());
    d->homepageAction->setVisible(!m_ui.le_homepage->text().isEmpty());
    m_ui.le_phone->setText(i.phones());

    auto addr = i.addresses().preferred().data;
    m_ui.le_street->setText(addr.street.value(0));
    m_ui.le_ext->setText(addr.extaddr.value(0));
    m_ui.le_city->setText(addr.locality.value(0));
    m_ui.le_state->setText(addr.region.value(0));
    m_ui.le_pcode->setText(addr.code.value(0));
    m_ui.le_country->setText(addr.country.value(0));

    m_ui.le_orgName->setText(i.org());

    m_ui.le_title->setText(i.title());
    m_ui.le_role->setText(i.role());
    m_ui.te_desc->setPlainText(i.note());

    auto pix = d->type == MucContact ? d->pa->avatarFactory()->getMucAvatar(d->jid)
                                     : d->pa->avatarFactory()->getAvatar(d->jid);
    if (pix.isNull()) {
        d->photo = i.photo();
        if (d->photo.isEmpty()) {
            clearPhoto();
        } else {
            // FIXME ideally we should come here after avatar factory is updated
            // the this code would be unnecessary
            updatePhoto();
        }
    } else {
        // yes we could use pixmap directly. let's keep it for future code optimization
        QByteArray ba;
        QBuffer    b(&ba);
        b.open(QIODevice::WriteOnly);
        pix.toImage().save(&b, "PNG");
        d->photo = ba;
        updatePhoto();
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
        d->photo = {};
        return false;
    }
    QMimeDatabase db;
    d->photoMime   = db.mimeTypeForData(d->photo).name();
    int max_width  = m_ui.tb_photo->width() - 10;  // FIXME: Ugly magic number
    int max_height = m_ui.tb_photo->height() - 10; // FIXME: Ugly magic number

    m_ui.tb_photo->setIcon(QPixmap::fromImage(img));
    m_ui.tb_photo->setIconSize(QSize(max_width, max_height));
    return true;
}

void InfoWidget::fieldsEnable(bool x)
{
    m_ui.le_fullname->setEnabled(x);
    d->le_givenname->setEnabled(x);
    d->le_middlename->setEnabled(x);
    d->le_familyname->setEnabled(x);
    m_ui.le_nickname->setEnabled(x);
    m_ui.le_bday->setEnabled(x);
    m_ui.le_email->setEnabled(x);
    m_ui.le_homepage->setEnabled(x);
    m_ui.le_phone->setEnabled(x);
    m_ui.pb_open->setEnabled(x);
    m_ui.pb_clear->setEnabled(x);

    m_ui.le_street->setEnabled(x);
    m_ui.le_ext->setEnabled(x);
    m_ui.le_city->setEnabled(x);
    m_ui.le_state->setEnabled(x);
    m_ui.le_pcode->setEnabled(x);
    m_ui.le_country->setEnabled(x);

    m_ui.le_orgName->setEnabled(x);
    m_ui.le_orgUnit->setEnabled(x);
    m_ui.le_title->setEnabled(x);
    m_ui.le_role->setEnabled(x);
    m_ui.te_desc->setEnabled(x);

    setEdited(false);
}

void InfoWidget::setEdited(bool x)
{
    m_ui.le_fullname->setModified(x);
    d->le_givenname->setModified(x);
    d->le_middlename->setModified(x);
    d->le_familyname->setModified(x);
    m_ui.le_nickname->setModified(x);
    m_ui.le_bday->setModified(x);
    m_ui.le_email->setModified(x);
    m_ui.le_homepage->setModified(x);
    m_ui.le_phone->setModified(x);
    m_ui.le_street->setModified(x);
    m_ui.le_ext->setModified(x);
    m_ui.le_city->setModified(x);
    m_ui.le_state->setModified(x);
    m_ui.le_pcode->setModified(x);
    m_ui.le_country->setModified(x);
    m_ui.le_orgName->setModified(x);
    m_ui.le_orgUnit->setModified(x);
    m_ui.le_title->setModified(x);
    m_ui.le_role->setModified(x);

    d->te_edited = x;
}

bool InfoWidget::edited()
{
    return 
        m_ui.le_fullname->isModified() ||
        d->le_givenname->isModified() ||
        d->le_middlename->isModified() ||
        d->le_familyname->isModified() ||
        m_ui.le_nickname->isModified() ||
        m_ui.le_bday->isModified() ||
        m_ui.le_email->isModified() ||
        m_ui.le_homepage->isModified() ||
        m_ui.le_phone->isModified() ||
        m_ui.le_street->isModified() ||
        m_ui.le_ext->isModified() ||
        m_ui.le_city->isModified() ||
        m_ui.le_state->isModified() ||
        m_ui.le_pcode->isModified() ||
        m_ui.le_orgName->isModified() ||
        m_ui.le_orgUnit->isModified() ||
        m_ui.le_title->isModified() ||
        m_ui.le_role->isModified() ||
        d->te_edited;
}

void InfoWidget::setReadOnly(bool x)
{
    m_ui.le_fullname->setReadOnly(x);
    d->le_givenname->setReadOnly(x);
    d->le_middlename->setReadOnly(x);
    d->le_familyname->setReadOnly(x);
    m_ui.le_nickname->setReadOnly(x);
    // ui_.le_bday->setReadOnly(x); //always read only. use calendar
    m_ui.le_email->setReadOnly(x);
    m_ui.le_homepage->setReadOnly(x);
    m_ui.le_phone->setReadOnly(x);
    m_ui.le_street->setReadOnly(x);
    m_ui.le_ext->setReadOnly(x);
    m_ui.le_city->setReadOnly(x);
    m_ui.le_state->setReadOnly(x);
    m_ui.le_pcode->setReadOnly(x);
    m_ui.le_country->setReadOnly(x);
    m_ui.le_orgName->setReadOnly(x);
    m_ui.le_orgUnit->setReadOnly(x);
    m_ui.le_title->setReadOnly(x);
    m_ui.le_role->setReadOnly(x);
    m_ui.te_desc->setReadOnly(x);
}

void InfoWidget::release()
{
    d->busy = false;
    emit released();
    fieldsEnable(true);
}

void InfoWidget::updateNick()
{
    if (d->jid.compare(d->pa->jid(), false)) {
        if (!d->vcard.nickName().preferred().data.isEmpty())
            d->pa->setNick(d->vcard.nickName().preferred());
        else
            d->pa->setNick(d->pa->jid().node());
    }
}

void InfoWidget::doRefresh()
{
    if (!d->pa->checkConnected(this))
        return;
    if (d->busy)
        return;

    fieldsEnable(false);

    emit busy();

    VCardFactory::Flags flags;
    if (d->type == MucContact) {
        flags |= VCardFactory::MucUser;
    }
    if (d->type == MucAdm || d->type == MucView) {
        flags |= VCardFactory::MucRoom;
    }
    auto request = VCardFactory::instance()->getVCard(d->pa, d->jid, flags);
    QObject::connect(request, &VCardRequest::finished, this, [this, request]() {
        release();
        if (request->success()) {
            auto vcard = request->vcard();
            if (vcard) {
                d->vcard = vcard;
                setData(d->vcard);
            }
            updateNick();
        } else {
            if (d->type == Self)
                QMessageBox::critical(
                    this, tr("Error"),
                    tr("Unable to retrieve your account information.  Perhaps you haven't entered any yet."));
            else if (d->type == MucAdm || d->type == MucView)
                QMessageBox::critical(this, tr("Error"),
                                      tr("Unable to retrieve information about this conference.\nReason: %1")
                                          .arg(request->errorString()));
            else
                QMessageBox::critical(
                    this, tr("Error"),
                    tr("Unable to retrieve information about this contact.\nReason: %1").arg(request->errorString()));
        }
    });
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

    d->vcard = makeVCard();

    fieldsEnable(false);

    emit busy();

    VCard4::VCard       v = d->vcard;
    VCardFactory::Flags flags;
    Jid                 target;
    if (d->type == MucAdm) {
        flags |= VCardFactory::MucRoom;
        target = d->jid;
    }
    auto task = VCardFactory::instance()->setVCard(d->pa, v, target, flags);
    connect(task, &JT_VCard::finished, this, [this, task]() {
        release();
        if (task->success()) {
            updateNick();
            QMessageBox::information(this, tr("Success"),
                                     d->type == MucAdm ? tr("Your conference information has been published.")
                                                       : tr("Your account information has been published."));
        } else {
            QMessageBox::critical(
                this, tr("Error"),
                tr("Unable to publish your account information.\nReason: %1").arg(task->statusString()));
        }
    });

    bool hasAvaConv = account()->client()->serverInfoManager()->accountFeatures().hasAvatarConversion();
    if (!hasAvaConv) {
        v.detach();
        if (d->photo.size()) {
            v.setPhoto(VCard4::UriValue { d->photo, d->photoMime });
        }
        VCardFactory::instance()->setVCard(d->pa, v, target, flags | VCardFactory::ForceVCardTemp);
    }

    if (d->type == Self) {
        // publish or retract avatar depending on d->photo contents
        d->pa->avatarFactory()->setSelfAvatar(QImage::fromData(d->photo));
    }
}

PsiAccount *InfoWidget::account() const { return d->pa; }

const Jid &InfoWidget::jid() const { return d->jid; }

void InfoWidget::doShowCal()
{
    d->noBdayButton->setChecked(m_ui.le_bday->text().isEmpty());
    if (d->bday.isValid()) {
        d->calendar->setSelectedDate(d->bday);
    }
}

void InfoWidget::doUpdateFromCalendar(const QDate &date)
{
    if (d->bday != date) {
        d->bday = date;
        m_ui.le_bday->setText(date.toString(d->dateTextFormat));
        m_ui.le_bday->setModified(true);
    }
    d->bdayPopup->hide();
}

void InfoWidget::doClearBirthDate()
{
    if (!m_ui.le_bday->text().isEmpty()) {
        d->bday = QDate();
        m_ui.le_bday->setText("");
        m_ui.le_bday->setModified(true);
    }
    d->bdayPopup->hide();
}

VCard4::VCard InfoWidget::makeVCard()
{
    VCard4::VCard v;

    using namespace VCard4;

    // Full Name
    QString fullName = m_ui.le_fullname->text();
    if (!fullName.isEmpty()) {
        v.setFullName({ { PString { Parameters(), fullName } } });
    }

    // Given Name
    PNames  names;
    QString givenName  = d->le_givenname->text();
    QString middleName = d->le_middlename->text();
    QString familyName = d->le_familyname->text();

    if (!givenName.isEmpty()) {
        names.data.given.append(givenName);
    }
    if (!middleName.isEmpty()) {
        names.data.additional.append(middleName);
    }
    if (!familyName.isEmpty()) {
        names.data.surname.append(familyName);
    }
    if (!names.data.isEmpty()) {
        v.setNames(names);
    }

    // Nickname
    QString nickName = m_ui.le_nickname->text();
    if (!nickName.isEmpty()) {
        v.setNickName({ nickName });
    }

    // Birthday
    if (d->bday.isValid()) {
        v.setBday({ Parameters(), d->bday });
    } else {
        QString bdayStr = m_ui.le_bday->text();
        if (!bdayStr.isEmpty()) {
            v.setBday({ Parameters(), bdayStr });
        }
    }

    // Email
    QString emailText = m_ui.le_email->text();
    if (!emailText.isEmpty()) {
        PStrings   emails;
        Parameters emailParams;
        auto       types = d->emailsDlg->types();
        if (types & AddressTypeDlg::Pref) {
            emailParams.pref = 1;
            types ^= AddressTypeDlg::Pref;
        }
        if (!types) {
            emailParams.type << "internet";
        } else {
            if (types & AddressTypeDlg::Internet)
                emailParams.type << "internet";
            if (types & AddressTypeDlg::Home)
                emailParams.type << "home";
            if (types & AddressTypeDlg::Work)
                emailParams.type << "work";
            if (types & AddressTypeDlg::X400)
                emailParams.type << "x400";
        }
        emails.append({ emailParams, emailText });
        v.setEmails(emails);
    }

    // URL
    QString homepage = m_ui.le_homepage->text();
    if (!homepage.isEmpty()) {
        QUrl url(homepage);
        if (url.isValid()) {
            v.setUrls(url);
        }
    }

    // Phone
    QString phoneText = m_ui.le_phone->text();
    if (!phoneText.isEmpty()) {
        PUrisOrTexts phones;
        Parameters   phoneParams;
        phoneParams.type << "home"
                         << "voice";
        phones.append({ phoneParams, phoneText });
        v.setPhones(phones);
    }

    // Photo
    if (d->type != Self && !d->photo.isEmpty()) {
        v.setPhoto(UriValue(d->photo, d->photoMime));
    }

    // Address
    QString street  = m_ui.le_street->text();
    QString ext     = m_ui.le_ext->text();
    QString city    = m_ui.le_city->text();
    QString state   = m_ui.le_state->text();
    QString pcode   = m_ui.le_pcode->text();
    QString country = m_ui.le_country->text();
    if (!street.isEmpty() || !ext.isEmpty() || !city.isEmpty() || !state.isEmpty() || !pcode.isEmpty()
        || !country.isEmpty()) {
        PAddresses addresses;
        Parameters addressParams;
        addressParams.type << "home";
        VCard4::Address address;
        if (!street.isEmpty())
            address.street.append(street);
        if (!ext.isEmpty())
            address.extaddr.append(ext);
        if (!city.isEmpty())
            address.locality.append(city);
        if (!state.isEmpty())
            address.region.append(state);
        if (!pcode.isEmpty())
            address.code.append(pcode);
        if (!country.isEmpty())
            address.country.append(country);
        addresses.append({ addressParams, address });
        v.setAddresses(addresses);
    }

    // Organization
    QString orgName = m_ui.le_orgName->text();
    QString orgUnit = m_ui.le_orgUnit->text();
    if (!orgName.isEmpty()) {
        PStringLists org;
        org.append({ Parameters(), { orgName } });
        if (!orgUnit.isEmpty()) {
            org.append({ Parameters(), { orgUnit } });
        }
        v.setOrg(org);
    }

    // Title
    QString title = m_ui.le_title->text();
    if (!title.isEmpty()) {
        v.setTitle({ { PString { Parameters(), title } } });
    }

    // Role
    QString role = m_ui.le_role->text();
    if (!role.isEmpty()) {
        v.setRole({ { PString { Parameters(), role } } });
    }

    // Description (note)
    QString desc = m_ui.te_desc->toPlainText();
    if (!desc.isEmpty()) {
        v.setNote({ { PString { Parameters(), desc } } });
    }

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
    m_ui.tb_photo->setIcon(QIcon());
    m_ui.tb_photo->setText(tr("Picture not\navailable"));
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
        PsiRichText::setText(m_ui.te_status->document(), u->makeDesc());
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
        PsiRichText::setText(m_ui.te_status->document(), info);
    } else {
        m_ui.te_status->clear();
    }

    auto cursor = m_ui.te_status->textCursor();
    cursor.setPosition(0);
    m_ui.te_status->setTextCursor(cursor);
}

/**
 * Sets the visibility of the status tab
 */
void InfoWidget::setStatusVisibility(bool visible)
{
    // Add/remove tab if necessary
    int index = m_ui.tabwidget->indexOf(m_ui.tab_status);
    if (index == -1) {
        if (visible)
            m_ui.tabwidget->addTab(m_ui.tab_status, tr("Status"));
    } else if (!visible) {
        m_ui.tabwidget->removeTab(index);
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
        const auto &uList = d->findRelevant(j->jid());
        for (UserListItem *u : uList) {
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
        const auto uList = d->findRelevant(j->jid());
        for (UserListItem *u : uList) {
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
        const auto &uList = d->findRelevant(d->jid);
        for (UserListItem *u : uList) {
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
    QString homepage = m_ui.le_homepage->text();
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
InfoDlg::InfoDlg(int type, const Jid &j, const VCard4::VCard &vc, PsiAccount *pa, QWidget *parent) : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint
                   | Qt::CustomizeWindowHint);
    setModal(false);
    m_ui.setupUi(this);
    m_iw = new InfoWidget(type, j, vc, pa, this);
    m_ui.loContents->addWidget(m_iw);

    if (type == InfoWidget::Self) {
        m_ui.pb_disco->hide();
    } else {
        m_ui.pb_submit->hide();
    }

    auto dialogSizeOption = PsiOptions::instance()->getOption(k_optionName);
    if (dialogSizeOption.isValid())
        resize(dialogSizeOption.toSize());
    else
        adjustSize();

    connect(m_ui.pb_refresh, &IconButton::clicked, m_iw, &InfoWidget::doRefresh);
    connect(m_ui.pb_refresh, &IconButton::clicked, m_iw, &InfoWidget::updateStatus);
    connect(m_ui.pb_submit, &IconButton::clicked, m_iw, &InfoWidget::publish);
    connect(m_ui.pb_close, &IconButton::clicked, this, &InfoDlg::close);
    connect(m_ui.pb_disco, &IconButton::clicked, this, &InfoDlg::doDisco);
    connect(m_iw, &InfoWidget::busy, this, &InfoDlg::doBusy);
    connect(m_iw, &InfoWidget::released, this, &InfoDlg::release);
}

void InfoDlg::closeEvent(QCloseEvent *e)
{
    if (m_iw->aboutToClose()) {
        PsiOptions::instance()->setOption(k_optionName, size());
        e->accept();
    } else {
        e->ignore();
    }
}

void InfoDlg::doDisco()
{
    DiscoDlg *w = new DiscoDlg(m_iw->account(), m_iw->jid(), "");
    connect(w, SIGNAL(featureActivated(QString, Jid, QString)), m_iw->account(),
            SLOT(featureActivated(QString, Jid, QString)));
    w->show();
}

void InfoDlg::doBusy()
{
    m_ui.pb_submit->setEnabled(false);
    m_ui.pb_refresh->setEnabled(false);
    m_ui.pb_close->setEnabled(false);
    m_ui.busy->start();
}

void InfoDlg::release()
{
    m_ui.pb_refresh->setEnabled(true);
    m_ui.pb_submit->setEnabled(true);
    m_ui.pb_close->setEnabled(true);
    m_ui.busy->stop();
}
