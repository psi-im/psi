#ifndef OPT_APPLICATION_H
#define OPT_APPLICATION_H

#include "optionstab.h"

class QButtonGroup;
class QWidget;

class OptionsTabApplication : public OptionsTab
{
    Q_OBJECT
public:
    OptionsTabApplication(QObject *parent);
    ~OptionsTabApplication();

    void setHaveAutoUpdater(bool);

    QWidget *widget();
    void applyOptions();
    void restoreOptions();

private:
    QWidget *w = nullptr;
    bool haveAutoUpdater_ = false;
    QString configPath_;
    bool autostartOptChanged_ = false;

private slots:
    void doEnableQuitOnClose(int);
};

#endif // OPT_APPLICATION_H
