#ifndef ACCOUNTSTATUSMENU_H
#define ACCOUNTSTATUSMENU_H

#include "psiaccount.h"
#include "psicon.h"
#include "statusmenu.h"

class AccountStatusMenu : public StatusMenu {
    Q_OBJECT
private:
    PsiAccount *account;

public:
    AccountStatusMenu(QWidget *parent, PsiCon *_psi, PsiAccount *_account) :
        StatusMenu(parent, _psi), account(_account) {};

    void fill();

protected:
    void addChoose();
    void addReconnect();

private:
    void addIgnoreGlobalActions();

private slots:
    void chooseStatusActivated();

signals:
    void reconnectActivated();
};

#endif // ACCOUNTSTATUSMENU_H
