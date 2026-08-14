/*
 * opt_security.h - encryption and key-management options page
 * Copyright (C) 2026 Sergey Ilinykh
 * Copyright (C) 2018 Vyacheslav Karpukhin
 * Copyright (C) 2020 Boris Pek
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef OPT_SECURITY_H
#define OPT_SECURITY_H

#include "optionstab.h"

#include <QMetaObject>

class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QWidget;
class PsiAccount;
class PsiCon;

class OptionsTabSecurity : public OptionsTab {
    Q_OBJECT
public:
    explicit OptionsTabSecurity(QObject *parent);

    QWidget *widget() override;
    void     setData(PsiCon *psi, QWidget *) override;
    void     applyOptions() override;
    void     restoreOptions() override;
    bool     stretchable() const override { return true; }

private:
    PsiAccount *currentAccount() const;
    void        refreshAccounts();
    void        refresh();
    void        refreshMethods();
    void        refreshOmemo();
    void        updateControllerConnections();
    void        setTrustForSelection(bool trusted);

    PsiCon  *psi_ = nullptr;
    QWidget *w_   = nullptr;

    QComboBox    *accounts_          = nullptr;
    QLabel       *accountStatus_     = nullptr;
    QTreeWidget  *methods_           = nullptr;
    QLabel       *ownDeviceId_       = nullptr;
    QLabel       *ownFingerprint_    = nullptr;
    QTreeWidget  *knownKeys_         = nullptr;
    QTreeWidget  *ownDevices_        = nullptr;
    QPushButton  *setUpOmemo_        = nullptr;
    QPushButton  *trustKey_          = nullptr;
    QPushButton  *distrustKey_       = nullptr;
    QPushButton  *retireOwnDevice_   = nullptr;
    QRadioButton *alwaysEnabled_     = nullptr;
    QRadioButton *enabledByDefault_  = nullptr;
    QRadioButton *disabledByDefault_ = nullptr;

    QMetaObject::Connection methodsConnection_;
    QMetaObject::Connection stateConnection_;
    QMetaObject::Connection errorConnection_;
};

#endif // OPT_SECURITY_H
