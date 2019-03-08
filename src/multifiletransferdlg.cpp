#include "multifiletransferdlg.h"
#include "ui_multifiletransferdlg.h"
#include "xmpp/jid/jid.h"
#include "jingle.h"
#include "multifiletransferview.h"
#include "psiaccount.h"
#include "avatars.h"
#include "psicontact.h"
#include "psicon.h"
#include "multifiletransfermodel.h"
#include "userlist.h"

#include <iconset.h>

using namespace XMPP;

class MultiFileTransferDlg::Private
{
public:
    PsiAccount *account;
    Jid peer;
    XMPP::Jingle::Session *session = nullptr;
    MultiFileTransferModel *model = nullptr;
};

MultiFileTransferDlg::MultiFileTransferDlg(PsiAccount *acc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MultiFileTransferDlg),
    d(new Private)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose); // TODO or maybe hide and wait for transfer to be completed?

    // we need square avatar space. to update minimum width to fit height
    QFontMetrics fm(font());
    int minHeight = 3 *(fm.height() + fm.leading()) + ui->ltNames->spacing() * 2;
    ui->lblMyAvatar->setMinimumWidth(minHeight);
    ui->lblPeerAvatar->setMinimumWidth(minHeight);


    ui->listView->setItemDelegate(new MultiFileTransferDelegate(this));
    d->account = acc;
    updateMyVisuals();

    ui->lblMyAvatar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->lblMyAvatar, &QLabel::customContextMenuRequested, this, [this](const QPoint &p){
        QList<QPair<Jid, PsiAccount*>> accs;
        for (auto const &a: d->account->psi()->contactList()->enabledAccounts()) {
            if (a->isAvailable()) {
                accs.append({a->selfContact()->jid(), a});
            }
        }
        if (accs.size() < 2) {
            return;
        }
        QMenu m(this);
        for (auto const &j: accs) {
            m.addAction(j.first.full())->setData(QVariant::fromValue(j.second));
        }
        auto act = m.exec(p);
        if (act) {
            d->account = act->data().value<PsiAccount*>();
            updateMyVisuals();
        }
    });

    d->model = new MultiFileTransferModel(this);
}

MultiFileTransferDlg::~MultiFileTransferDlg()
{
    delete ui;
}

void MultiFileTransferDlg::showOutgoing(const XMPP::Jid &jid, const QStringList &fileList)
{
    d->peer = jid;
    updatePeerVisuals();
    for (auto const &fname: fileList) {
        QFileInfo fi(fname);
        if (fi.isFile() && fi.isReadable()) {
            auto mftItem = d->model->addTransfer(MultiFileTransferModel::Outgoing, fi.baseName(), fi.size());
        }
    }
}

void MultiFileTransferDlg::showIncoming(XMPP::Jingle::Session *session)
{
    d->session = session;
    d->peer = session->peer();
    updatePeerVisuals();
}

void MultiFileTransferDlg::updateMyVisuals()
{
    QPixmap avatar;
    ui->lblMyAvatar->setToolTip(d->account->jid().full());
    avatar = d->account->avatarFactory()->getAvatar(d->account->jid());
    if (avatar.isNull()) {
        avatar = IconsetFactory::iconPixmap("psi/default_avatar");
    }
    ui->lblMyAvatar->setPixmap(avatar);
    ui->lblMyName->setText(d->account->nick());
}

void MultiFileTransferDlg::updatePeerVisuals()
{
    QPixmap avatar;
    if (d->peer.isValid()) {
        ui->lblPeerAvatar->setToolTip(d->peer.full());

        if (d->account->findGCContact(d->peer)) {
            avatar = d->account->avatarFactory()->getMucAvatar(d->peer);
            ui->lblPeerName->setText(d->peer.resource());

        } else {
            avatar = d->account->avatarFactory()->getAvatar(d->peer);
            auto item = d->account->userList()->find(d->peer.withResource(QString()));
            if (item) {
                ui->lblPeerName->setText(item->name());
            } else {
                ui->lblPeerName->setText(d->peer.node());
            }
        }
    } else {
        ui->lblPeerAvatar->setToolTip(tr("Not selected"));
        ui->lblPeerName->setText(tr("Not selected"));
    }

    if (avatar.isNull()) {
        avatar = IconsetFactory::iconPixmap("psi/default_avatar");
    }
    ui->lblPeerAvatar->setPixmap(avatar);
}
