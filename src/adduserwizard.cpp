/****************************************************************************
** adduserwizard.cpp
**
** Copyright (C) 2005-2006 Barracuda Networks, Inc.  All rights reserved.
**
** Written by Mike Fawcett
**
*****************************************************************************/

#include "adduserwizard.h"
#include "xmpp_tasks.h"
#include "psicon.h"
#include "adduserdlg.h"
#include "iconset.h"

#include <qwidget.h>
#include <q3hbox.h>
#include <q3vbox.h>
#include <q3grid.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qvalidator.h>
#include <qapplication.h>

using namespace XMPP;

static QString transportEnding(const QStringList &list, const QString &start)
{
	for(QStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
		if((*it).startsWith(start)) {
			Jid tmp = (*it);
			tmp = tmp.withResource(""); // chop off 'registered'
			return QString("@") + tmp.full();
		}
	}
	return QString();
}

addUserWizard::addUserWizard( QWidget *parent, const char *name, PsiAccount *pai, QStringList groups, QStringList services, QString type )
: Q3Wizard( parent, name, FALSE )
{
    pa = pai;
    setFixedWidth(550);
    setMinimumWidth(550);
    resize( 550, 50 );
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    service = type;
    servicelst = services;
    grouplst += tr("<None>");
    grouplst += groups;

    setCaption(tr("Barracuda IM Client: Add User"));
    
    setupPage1();
    setupPage2();
}

addUserWizard::~addUserWizard()
{
	pa->dialogUnregister(this);
}

void addUserWizard::setupPage1()
{
    page1 = new Q3HBox( this );
    page1->setSpacing(8);

    QLabel *mainpixlabel = new QLabel( page1 );
    QPixmap mainlogo = (QPixmap)IconsetFactory::icon("psi/barracudaregister").pixmap();
    mainpixlabel->setPixmap(mainlogo);
    mainpixlabel->setFixedSize(mainlogo.width(), mainlogo.height());
    mainpixlabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    Q3VBox *main_vbox = new Q3VBox(page1);

    QLabel *infotxt = new QLabel( main_vbox );
    infotxt->setText( tr("<br>Please select the type of contact you<br>wish to add to your roster.<br>"));

    QLabel *hr1 = new QLabel( main_vbox );
    hr1->setText("<hr>");

    Q3HBox *service_hbox = new Q3HBox(main_vbox);
    /*QLabel *unametxt =*/ new QLabel( tr("Service:"), service_hbox );
    servicecb = new Q3ComboBox( service_hbox );

    QStringList services;
    services += "Barracuda";
	QStringList::ConstIterator it1 = servicelst.begin();
	for(; it1 != servicelst.end(); ++it1) {
        QString iserv = *it1;
        if( iserv.startsWith("msn") )
            services += "MSN";
        else if( iserv.startsWith("yahoo") )
            services += "Yahoo!";
        else if( iserv.startsWith("aim") )
            services += "AIM";
        else if( iserv.startsWith("icq") )
            services += "ICQ";
    }
    //services += "Google";
    services += "Jabber / Google Talk";

    servicecb->insertStringList( services );

    addPage( page1, tr("Select Contact Type") );

    connect( nextButton(), SIGNAL( clicked() ), this, SLOT( selected_service() ) );

    setNextEnabled( page1, TRUE );
    setBackEnabled( page1, TRUE );
    setHelpEnabled( page1, FALSE );
    helpButton()->hide();
}

void addUserWizard::setupPage2()
{
    page2 = new Q3HBox( this );
    page2->setSpacing(8);

    pixlabel = new QLabel( page2 );

    Q3VBox *main_vbox = new Q3VBox(page2);

    info = new QLabel( main_vbox );
    info->setMargin( 11 );

    QLabel *hr1 = new QLabel( main_vbox );
    hr1->setText("<hr>");

    Q3HBox *uname_hbox = new Q3HBox(main_vbox);
    uname = new QLabel( transport_login, uname_hbox );
    uname->setMargin( 11 );
    username = new QLineEdit( uname_hbox );

    Q3HBox *nick_hbox = new Q3HBox(main_vbox);
    QLabel *nicklbl = new QLabel( tr("Nick Name:"), nick_hbox );
    nicklbl->setMargin( 11 );
    nickname = new QLineEdit( nick_hbox );

    Q3HBox *group_hbox = new Q3HBox(main_vbox);
    QLabel *group = new QLabel( tr("Group:"), group_hbox );
    group->setMargin( 11 );
    groupcb = new Q3ComboBox( group_hbox );
    groupcb->insertStringList( grouplst );

    addPage( page2, tr("Login Information") );

    connect( finishButton(), SIGNAL( clicked() ), this, SLOT( add_user() ) );

    setFinishEnabled( page2, TRUE );
}

void addUserWizard::selected_service()
{
    QString service = servicecb->currentText();

    if( service == "AIM" ) {
        info->setText(tr("Enter the username of the AIM user<br>you wish to add to your contact list."));
        transport_text = transportEnding(servicelst, "aim");
        transport_icon = "psi/aimregister";
        transport_login = "AIM User Name:";
    } else if(service == "ICQ" ) {
        info->setText(tr("Enter the user id number of the ICQ user<br>you wish to add to your contact list."));
        transport_text = transportEnding(servicelst, "icq");
        transport_icon = "psi/icqregister";
        transport_login = "ICQ UIN:";
    } else if(service == "Yahoo!" ) {
        info->setText(tr("Enter the username of the Yahoo! user<br>you wish to add to your contact list."));
        transport_text = transportEnding(servicelst, "yahoo");
        transport_icon = "psi/yahooregister";
        transport_login = "Yahoo! User Name:";
    } else if(service == "MSN" ) {
        info->setText(tr("Enter the username of the MSN user<br>you wish to add to your contact list."));
        transport_text = transportEnding(servicelst, "msn");
        transport_icon = "psi/msnregister";
        transport_login = "MSN User Name:";
    /*} else if(service == "Google" ) {
        info->setText(tr("Enter the username of the Google user<br>you wish to add to your contact list."));
        transport_text = "@gmail.com";
        transport_icon = "psi/googleregister";
        transport_login = "Google User Name:";*/
    } else if(service == "Jabber / Google Talk" ) {
        info->setText(tr("Enter the Jabber ID of the Jabber user<br>you wish to add to your contact list."));
        transport_text = "";
        transport_icon = "psi/jabberregister";
        transport_login = "Jabber ID:";
    } else {
        info->setText(tr("Enter the username of the Barracuda IM user<br>you wish to add to your contact list."));
        transport_text = "";
        transport_icon = "psi/barracudaregister";
        transport_login = "Barracuda ID:";
    }

    logo = (QPixmap)IconsetFactory::icon(transport_icon).pixmap();
    pixlabel->setPixmap(logo);
    pixlabel->setFixedSize(logo.width(), logo.height());
    pixlabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    uname->setText(transport_login);
}

void addUserWizard::add_user()
{
    // FIXME - need to implement transport side service translation for jids
    // use getTransID from adduserdlg
    QString jid_name = username->text();
    QRegExp sbcglobal("@sbcglobal.net");

    if((servicecb->currentText() == "MSN") || (sbcglobal.search(jid_name) != -1)) {
        QRegExp rx( "@" );
        jid_name.replace( rx, "%" );
    }
    
    Jid j = Jid( jid_name + transport_text );

    QString nick = nickname->text();

    QString gname = groupcb->currentText();
	QStringList list;
	if(gname != tr("<None>"))
        list += gname;

	add(j, nick, list, true);
}
