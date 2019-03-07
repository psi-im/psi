#include "multifiletransferdlg.h"
#include "ui_multifiletransferdlg.h"
#include "xmpp/jid/jid.h"
#include "jingle.h"

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
    if (d->peer.isValid()) {
        // TODO find contact and set avatar and name. then check enable buttons
    } else {
        // TODO render not selected contact. and disable transfer button
    }
}
