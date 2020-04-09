#include "opt_accounts.h"

#include "psioptions.h"

#include "accountmanagedlg.h"

static const QString accSingleOption = "options.ui.account.single";

OptionsTabAccounts::OptionsTabAccounts(QObject *parent) :
    MetaOptionsTab(parent, "accounts", "", tr("Accounts"), tr("Manage accounts"), "psi/account"), w_(nullptr),
    psi_(nullptr)
{
}

OptionsTabAccounts::~OptionsTabAccounts() {}

QWidget *OptionsTabAccounts::widget()
{
    if (w_) {
        return nullptr;
    }

    w_            = new AccountManageDlg(psi_);
    PsiOptions *o = PsiOptions::instance();

    static_cast<AccountManageDlg *>(w_)->enableElements(!o->getOption(accSingleOption).toBool());
    connect(o, &PsiOptions::optionChanged, this, [this](const QString &option) {
        if (!w_)
            return;

        if (option == accSingleOption)
            static_cast<AccountManageDlg *>(w_)->enableElements(
                !PsiOptions::instance()->getOption(accSingleOption).toBool());
    });

    return w_;
}

void OptionsTabAccounts::setData(PsiCon *psi, QWidget *) { psi_ = psi; }
