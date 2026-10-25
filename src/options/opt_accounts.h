#ifndef OPT_ACCOUNTS_H
#define OPT_ACCOUNTS_H
#include "optionstab.h"
#include <QPointer>
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class PsiAccount;

class OptionsTabAccounts : public MetaOptionsTab {
    Q_OBJECT
public:
    explicit OptionsTabAccounts(QObject *parent);
};

class OptionsTabXmppAccounts : public OptionsTab {
    Q_OBJECT
public:
    explicit OptionsTabXmppAccounts(QObject *parent);
    QWidget *widget() override;
    void     setData(PsiCon *psi, QWidget *) override;

private:
    QPointer<QWidget> w_;
    PsiCon           *psi_ = nullptr;
};

class OptionsTabServerless : public OptionsTab {
    Q_OBJECT
public:
    explicit OptionsTabServerless(QObject *parent);
    QWidget *widget() override;
    void     setData(PsiCon *psi, QWidget *) override;
    void     restoreOptions() override;
    void     applyOptions() override;

private:
    PsiAccount       *account() const;
    void              refreshInterfaces();
    void              updateStatus();
    PsiCon           *psi_ = nullptr;
    QPointer<QWidget> w_;
    QCheckBox        *enabled_ = nullptr, *automatic_ = nullptr;
    QLineEdit        *name_      = nullptr;
    QComboBox        *interface_ = nullptr;
    QSpinBox         *port_      = nullptr;
    QLabel           *identity_ = nullptr, *status_ = nullptr;
};
#endif
