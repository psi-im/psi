/*
 * tipdlg.cpp - Handles Tip of the Day
 * Copyright (C) 2001-2006  Michail Pishchagin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */  

#include "tipdlg.h"

#include "psioptions.h"
#include "psicon.h"

void TipDlg::show(PsiCon* psi)
{
	QWidget* dlg = psi->dialogFind(TipDlg::staticMetaObject.className());
	if (!dlg)
		dlg = new TipDlg(psi);

	Q_ASSERT(dlg);
	bringToFront(dlg);
}

/**
 * \class TipDlg
 * \brief A 'Tip Of The Day' dialog
 */
TipDlg::TipDlg(PsiCon* psi)
	: QDialog(0)
	, psi_(psi)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setupUi(this);
	setModal(false);

	psi_->dialogRegister(this);

	//ck_showTips->hide();

	connect(pb_close,SIGNAL(clicked()),SLOT(accept()));
	connect(pb_previous,SIGNAL(clicked()),SLOT(previous()));
	connect(pb_next,SIGNAL(clicked()),SLOT(next()));
	connect(ck_showTips,SIGNAL(toggled(bool)),SLOT(showTipsChanged(bool)));

	// add useful tips here
	addTip( tr("Hello! Thank you for downloading Psi!\nWe hope that you will enjoy using it as we have enjoyed making it!\n"
		   "<br><br>If you want to download another language translation, iconset or a new version of Psi, then you need to visit the <a href=\"http://psi-im.org\">Psi HomePage</a>.\n"
		   "<br><br>If you think, that you have found a bug or you just want to chat with other Psi users, then visit the <a href=\"http://forum.psi-im.org/\">Psi Forums</a>.\n"
		   "<br><br><div align=\"right\"><i>the Psi Team</i></div>"), "" );
	addTip( tr("You can select multiple emoticon iconsets, and assign them priorities using the options dialog."), "" );
	addTip( tr("You can use multiple useful shortcuts while typing chat messages:<br>\n"
		   "<ul>\n"
		   "<li>Ctrl+Enter to send message</li>\n"
		   "<li>Ctrl+M to add newline character</li>\n"
		   "<li>Ctrl+H to display message history dialog</li>\n"
		   "<li>Alt+S to send message</li>\n"
		   "<li>Ctrl+U to clear edit buffer</li>\n"
		   "<li>Ctrl+PgUp/PgDn to scroll chat view</li>\n"
		   "</ul>"), "" );
	addTip( tr("You can type these special commands in chat and groupchat dialogs:\n"
		   "<ul>\n"
		   "<li>\"/clear\" to clear chat view</li>\n"
		   "<li>\"/me &lt;message&gt;\" '/me' is replaced by your nick</li>\n"
		   "</ul>\n"
		   "And these work only in groupchat dialog:\n"
		   "<ul>\n"
		   "<li>\"/nick &lt;new_nickname&gt;\" to change your nickname</li>\n"
		   "</ul>"), "" );
	addTip( tr("Did you know that you can register multiple Jabber accounts with Psi? If you like to separate your work from your personal account, you can.  If you are a power user who wants to test the latest Jabber features on an unstable server, you can do that -- without running a second client to connect to your stable server.  Just click Add in the Account Setup screen."),
		"Hal Rottenberg" );
	addTip( tr("Do you chat on third-party IM networks such as AIM and ICQ?  Try enabling the \"transport-specific icons\" option.  This will allow you to quickly see at a glance which network your buddy is using.  Then you can convince him to switch to Jabber. <icon name=\"psi/smile\">"),
		"Hal Rottenberg" );
	addTip( tr("Did you know that you can use checkboxes in Account Setup dialog to enable/disable accounts? This may be useful in the case of an account that you use rarely, so it will not clutter your roster."),
		"Iain MacDonnell" );
	addTip( tr("Don't like the buttons where they are?  Want a shortcut button to change your status to Away?  Check out the Configure Toolbars window, it's available through toolbars' context menu.  You can even make a toolbar that floats!"),
		"Hal Rottenberg" );
	addTip( tr("Did you know that Psi is one of the only Jabber clients that allows you to connect to multiple servers at the same time?  You can be known as \"mrcool@jabber.org\" to your friends, and \"John.J.Smith_the_fourth@mycompany.com\" to business associates."),
		"Hal Rottenberg" );
	addTip( tr("Have you converted over from Gadu-Gadu or Trillian and you miss the cool emoticons?  Fear not, we have you covered!  Check out <a href=\"http://jisp.netflint.net\">http://jisp.netflint.net</a> for tons of \"Iconsets\" that can be added to Psi to make it look the way you like!"),
		"Hal Rottenberg" );
	addTip( tr("Did you know that a middle-click (the middle mouse button on a 3-button mouse) will \"perform the default action\" on many objects within Psi?  Try middle-clicking on a contact or a popup."),
		"Hal Rottenberg" );
	addTip( tr("In order to add contacts from different IM networks, you need to add a corresponding agent from your Jabber server. Take a look at Psi Menu -> Service Discovery."),
		"Philipp Droessler" );
	addTip( tr("You can right-click on the server name in your roster to perform several different actions.  You can change status, modify account settings, perform administrative options (if you have permission), and more."),
		"Hal Rottenberg" );
	addTip( tr("Looking for a transport or chatroom, but your server provides nothing appropriate? Use Psi Menu -> Service Discovery to look on <i>any</i> Jabber Server for nice services by typing its domain in the address field.\n<br><br>\nNote: Some server may disable transport registration to users from different servers, but that's not common yet."),
		"Patrick Hanft" );
	addTip( tr("If you're chatting in groupchats quite frequently, nick completion is an invaluable feature. The most useful shortcut is <tt>Tab-Tab</tt>; when used on beginning of new line or after a step it inserts the nickname of the person who last addressed you directly. You can then continue to press <tt>Tab</tt> and it will loop on the nicks of all the people in the room.<br/><br/>For a more complicated scenario: <tt>mblsha</tt>, <tt>Monster</tt> and <tt>mbl-revolution</tt> are all sitting in same room. If you write <tt>m</tt> and press <tt>Tab</tt> it will not result in any noticeable action. This is because there are multiple nicks that start with <tt>m</tt>, and you can either continue to <tt>Tab</tt> to loop through all nicks that start with <tt>m</tt> or write more letters until there is a unique completion. When you press the <tt>b</tt> button, and then press <tt>Tab</tt> it would complete to <tt>mbl</tt>. The more you use this feature, the more you are likely to come to like it and rely upon it. Try nick completion on someone and you'll realise how powerful it is."),
		"Michail Pishchagin and Kevin Smith" );

	// this MUST be the last tip
	addTip( tr("This is the last tip.\n<br><br>If you want to contribute your own \"tip of the day\", please publish it on the <a href=\"http://forum.psi-im.org\">Psi Forums</a> (or mail it to the one of the developers), and we'll be happy to integrate it for the next release."), "" );

	updateTip();
	ck_showTips->setChecked( PsiOptions::instance()->getOption("options.ui.tip.show").toBool());
}

TipDlg::~TipDlg()
{
	psi_->dialogUnregister(this);
}

void TipDlg::updateTip()
{
	int num = PsiOptions::instance()->getOption("options.ui.tip.number").toInt();
	if ( num < 0 )
		num = tips.count() - 1;
	else if ( num >= (int)tips.count() )
		num = 0;
	
	tv_psi->setText( tips[num] );
	
	PsiOptions::instance()->setOption("options.ui.tip.number", num+1);
}


void TipDlg::next()
{
	updateTip();
}


void TipDlg::previous()
{
	int num = PsiOptions::instance()->getOption("options.ui.tip.number").toInt();
	PsiOptions::instance()->setOption("options.ui.tip.number", num-2);
	updateTip();
}


void TipDlg::showTipsChanged( bool e )
{
	PsiOptions::instance()->setOption("options.ui.tip.show", e);
}


void TipDlg::addTip(const QString& tip, const QString& author )
{
	QString t = tip;
	if ( !author.isEmpty() )
		t += "<br><br><i>" + tr("Contributed by") + " " + author + "</i>";

	tips += "<qt>" + t + "</qt>";
}
