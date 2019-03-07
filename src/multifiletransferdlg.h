#ifndef MULTIFILETRANSFERDLG_H
#define MULTIFILETRANSFERDLG_H

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class MultiFileTransferDlg;
}

namespace XMPP {
    class Jid;
    namespace Jingle {
        class Session;
    }
}

class PsiAccount;

class MultiFileTransferDlg : public QDialog
{
    Q_OBJECT

public:
    explicit MultiFileTransferDlg(PsiAccount *acc, QWidget *parent = nullptr);
    ~MultiFileTransferDlg();

    void showOutgoing(const XMPP::Jid &jid);
    void showIncoming(XMPP::Jingle::Session *session);
private:
    void updatePeerVisuals();

private:
    Ui::MultiFileTransferDlg *ui;
    class Private;
    QScopedPointer<Private> d;
};

#endif // MULTIFILETRANSFERDLG_H
