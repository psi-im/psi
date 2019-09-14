#ifndef OPT_ACCOUNTS_H
#define OPT_ACCOUNTS_H

#include "common.h"
#include "optionstab.h"
#include "psicon.h"

class QWidget;

class OptionsTabAccounts : public MetaOptionsTab
{
    Q_OBJECT

public:

    OptionsTabAccounts(QObject *parent);
    ~OptionsTabAccounts();

    QWidget *widget();
    void setData(PsiCon *psi, QWidget *);

private:
    QWidget *w_;
    PsiCon *psi_;
};

#endif // OPT_ACCOUNTS_H
