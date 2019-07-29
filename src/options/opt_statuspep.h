#ifndef OPT_STAUSPEP_H
#define OPT_STAUSPEP_H

#include "common.h"
#include "optionstab.h"
#include "psicon.h"

class QWidget;

class OptionsTabStatusPep : public OptionsTab
{
    Q_OBJECT

public:

    OptionsTabStatusPep(QObject *parent);
    ~OptionsTabStatusPep();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();
    void setData(PsiCon *psi, QWidget *);

private:
    QWidget *w_;
    PsiCon *psi_;
    QStringList blackList_;
    QString tuneFilters_;
    bool controllersChanged_;
};

#endif // OPT_STAUSPEP_H
