#ifndef OPT_MESSAGES_COMMON_H
#define OPT_MESSAGES_COMMON_H

#include "optionstab.h"
#include "psicon.h"

class QWidget;

class OptionsTabMsgCommon : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabMsgCommon(QObject *parent);
    ~OptionsTabMsgCommon();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();
    void setData(PsiCon *psi, QWidget *);

private:
    QWidget *w_;
    PsiCon *psi_;
};

#endif // OPT_MESSAGES_COMMON_H
