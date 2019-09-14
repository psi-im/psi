#include "opt_accounts.h"

#include "psioptions.h"

#include "accountmanagedlg.h"

OptionsTabAccounts::OptionsTabAccounts(QObject *parent)
: MetaOptionsTab(parent, "accounts", "", tr("Accounts"), tr("Manage accounts"), "psi/account")
, w_(nullptr)
, psi_(nullptr)
{
}

OptionsTabAccounts::~OptionsTabAccounts()
{
}

QWidget *OptionsTabAccounts::widget()
{
    if (w_) {
        return nullptr;
    }

    w_ = new AccountManageDlg(psi_);

    return w_;
}

void OptionsTabAccounts::setData(PsiCon *psi, QWidget *)
{
    psi_ = psi;
}
