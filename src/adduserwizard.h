/****************************************************************************
** adduserwizard.h
**
** Copyright (C) 2005-2006 Barracuda Networks, Inc.  All rights reserved.
**
** Written by Mike Fawcett
**
*****************************************************************************/

#ifndef ADDUSERWIZARD_H
#define ADDUSERWIZARD_H

#include <q3wizard.h>
#include <q3combobox.h>
#include "psiaccount.h"
#include "xmpp_tasks.h"
#include "psicon.h"

class QWidget;
class Q3HBox;
class QLineEdit;
class QLabel;
class Q3Grid;

class addUserWizard : public Q3Wizard
{
    Q_OBJECT

public:
    addUserWizard( QWidget *parent = 0, const char *name = 0, PsiAccount * = 0, const QStringList groups = QStringList(), const QStringList services = QStringList(), QString trans = "none" );
	~addUserWizard();
    PsiAccount *pa;

signals:
	void add(const XMPP::Jid &, const QString &, const QStringList &, bool authReq);

public slots:
    void add_user();
    void selected_service();

protected:
    void setupPage1();
    void setupPage2();

    QPixmap logo;
    QLabel *pixlabel, *uname;
    Q3Grid *grid2;
    Q3HBox *page1, *page2;
    QLineEdit *username, *nickname;
    QString service, transport_text, transport_icon, transport_login;
    QStringList servicelst, grouplst;
    Q3ComboBox *servicecb, *groupcb;
    XMPP::JT_Register *reg;
    QLabel *info;
};

#endif
