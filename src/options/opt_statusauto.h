#ifndef OPT_STATUSAUTO_H
#define OPT_STATUSAUTO_H

#include "common.h"
#include "optionstab.h"

class QWidget;

class OptionsTabStatusAuto : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabStatusAuto(QObject *parent);
    ~OptionsTabStatusAuto();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();

    void setData(PsiCon *, QWidget *parentDialog);

private:
    QWidget *w = nullptr, *parentWidget = nullptr;
};

#endif // OPT_STATUSAUTO_H
