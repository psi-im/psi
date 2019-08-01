#ifndef OPT_CHAT_H
#define OPT_CHAT_H

#include "optionstab.h"

class QButtonGroup;
class QWidget;

class OptionsTabChat : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabChat(QObject *parent);
    ~OptionsTabChat();

    QWidget *widget();
    void applyOptions();
    void restoreOptions();

private:
    QWidget *w;
    QButtonGroup *bg_defAct;
};

#endif // OPT_CHAT_H
