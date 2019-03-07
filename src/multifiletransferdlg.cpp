#include "multifiletransferdlg.h"
#include "ui_multifiletransferdlg.h"
#include "xmpp/jid/jid.h"
#include "jingle.h"
#include "multifiletransferview.h"
#include "psiaccount.h"
#include "avatars.h"
#include "psicontact.h"

#include <iconset.h>

using namespace XMPP;

class MultiFileTransferDlg::Private
{
public:
    PsiAccount *account;
    Jid peer;
    XMPP::Jingle::Session *session = nullptr;
};

MultiFileTransferDlg::MultiFileTransferDlg(PsiAccount *acc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MultiFileTransferDlg),
    d(new Private)
{
    ui->setupUi(this);
    ui->listView->setItemDelegate(new MultiFileTransferDelegate(this));
    d->account = acc;
    setAttribute(Qt::WA_DeleteOnClose); // TODO or maybe hide and wait for transfer to be completed?
}

MultiFileTransferDlg::~MultiFileTransferDlg()
{
    delete ui;
}

void MultiFileTransferDlg::showOutgoing(const XMPP::Jid &jid)
{
    d->peer = jid;
    updatePeerVisuals();
}

void MultiFileTransferDlg::showIncoming(XMPP::Jingle::Session *session)
{
    d->session = session;
    d->peer = session->peer();
    updatePeerVisuals();
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
            auto contact = d->account->findContact(d->peer);
            if (contact) {
                ui->lblPeerName->setText(contact->name());
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
