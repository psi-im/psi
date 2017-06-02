/*
 * aboutdlg.cpp
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QTextStream>
#include <QFile>
#include <QtCrypto>

#include "applicationinfo.h"
#include "aboutdlg.h"

AboutDlg::AboutDlg(QWidget* parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	ui_.setupUi(this);

	setModal(false);

	ui_.lb_name->setText ( QString("<h3><b>%1 v%2</b></h3>").arg(ApplicationInfo::name()).arg(ApplicationInfo::version()) );

	ui_.te_license->setText ( loadText(":/COPYING") );

	QString lang_name = qApp->translate( "@default", "language_name" );
	if ( lang_name == "language_name" ) // remove the translation tab, if no translation is used
		ui_.tw_tabs->removeTab ( 3 );

	// fill in Authors tab...
	QString authors;
	authors += details(QString::fromUtf8("Justin Karneges"),
			   "justin@affinix.com", "", "",
			   tr("Original Author"));
	authors += details(QString::fromUtf8("Sergey Ilinykh"),
			   "rion4ik@gmail.com", "", "",
			   tr("Lead Developer and Current Maintainer"));
	authors += details(QString::fromUtf8("Kevin Smith"),
			   "kismith@psi-im.org", "", "",
			   tr("Past Lead Developer and Maintainer"));
	authors += details(QString::fromUtf8("Remko Tronçon"),
			   "", "", "http://el-tramo.be",
			   tr("Past Lead Developer"));
	authors += details(QString::fromUtf8("Michail Pishchagin"),
			   "mblsha@psi-im.org", "", "",
			   tr("Lead Widget Developer"));
	authors += details(QString::fromUtf8("Maciej Niedzielski"),
			   "machekku@psi-im.org", "", "",
			   tr("Developer"));
	authors += details(QString::fromUtf8("Martin Hostettler"),
			   "martin@psi-im.org", "", "",
			   tr("Developer"));
	authors += details(QString::fromUtf8("Evgeny Khryukin"),
			   "wadealer@gmail.com", "", "",
			   tr("Developer"));
	authors += details(QString::fromUtf8("Aleksey Andreev"),
			   "liuch@mail.ru", "", "",
			   tr("Developer"));
	authors += details(QString::fromUtf8("Vitaly Tonkacheyev"),
			   "thetvg@gmail.com", "", "",
			   tr("Developer"));
	ui_.te_authors->setText( authors );

	// fill in Thanks To tab...
	QString thanks;
	thanks += details(QString::fromUtf8("Frederik Schwarzer"),
			  "schwarzerf@gmail.com", "", "",
			  tr("Language coordinator, miscellaneous assistance"));
	thanks += details(QString::fromUtf8("Akito Nozaki"),
			  "anpluto@usa.net", "", "",
			  tr("Former language coordinator, miscellaneous assistance"));
	thanks += details(QString::fromUtf8("Jan Niehusmann"),
			  "jan@gondor.com", "", "",
			  tr("Build setup, miscellaneous assistance"));
	thanks += details(QString::fromUtf8("Everaldo Coelho"),
			  "", "", "http://www.everaldo.com",
			  tr("Many icons are from his Crystal icon theme"));
	thanks += details(QString::fromUtf8("Jason Kim"),
			  "", "", "",
			  tr("Graphics"));
	thanks += details(QString::fromUtf8("Hideaki Omuro"),
			  "", "", "",
			  tr("Graphics"));
	thanks += details(QString::fromUtf8("Bill Myers"),
			  "", "", "",
			  tr("Original Mac Port"));
	thanks += details(QString::fromUtf8("Eric Smith (Tarkvara Design, Inc.)"),
			 "eric@tarkvara.org", "", "",
			 tr("Mac OS X Port"));
	thanks += details(QString::fromUtf8("Tony Collins"),
	 		 "", "", "",
	 		 tr("Original End User Documentation"));
	thanks += details(QString::fromUtf8("Hal Rottenberg"),
			  "", "", "",
			 tr("Webmaster, Marketing"));
	thanks += details(QString::fromUtf8("Mircea Bardac"),
			 "", "", "",
			 tr("Bug Tracker Management"));
	thanks += details(QString::fromUtf8("Jacek Tomasiak"),
			 "", "", "",
			 tr("Patches"));

	// sponsor thanks
	thanks += details(QString::fromUtf8("Barracuda Networks, Inc."),
			 "", "", "",
			 tr("Sponsor"));
	thanks += details(QString::fromUtf8("Portugal Telecom"),
			 "", "", "",
			 tr("Sponsor"));
	thanks += details(QString::fromUtf8("Deviant Technologies, Inc."),
			 "", "", "",
			 tr("Sponsor"));
	thanks += details(QString::fromUtf8("RealNetworks, Inc."),
			 "", "", "",
			 tr("Sponsor"));
	thanks += details(QString::fromUtf8("Trolltech AS"),
			 "", "", "",
			 tr("Sponsor"));
	thanks += details(QString::fromUtf8("Google, Inc."),
			 "", "", "",
			 tr("Sponsor (Summer of Code)"));

	foreach(QCA::Provider *p, QCA::providers()) {
		QString credit = p->credit();
		if(!credit.isEmpty()) {
			thanks += details(tr("Security plugin: %1").arg(p->name()),
				"", "", "",
				credit);
		}
	}

	//thanks += tr("Thanks to many others.\n"
	//	     "The above list only reflects the contributors I managed to keep track of.\n"
	//	     "If you're not included but you think that you must be in the list, contact the developers.");
	ui_.te_thanks->setText( thanks );

	QString translation = tr(
		"I. M. Anonymous <note text=\"replace with your real name\"><br>\n"
		"&nbsp;&nbsp;<a href=\"http://me.com\">http://me.com</a><br>\n"
		"&nbsp;&nbsp;XMPP: <a href=\"xmpp:me@me.com\">me@me.com</a><br>\n"
		"&nbsp;&nbsp;<a href=\"mailto:me@me.com\">me@me.com</a><br>\n"
		"&nbsp;&nbsp;Translator<br>\n"
		"<br>\n"
		"Join the translation team today! Go to \n"
		"<a href=\"https://github.com/psi-plus/psi-plus-l10n\">\n"
		"https://github.com/psi-plus/psi-plus-l10n</a> for further details!"
	);
	ui_.te_translation->appendText(translation);
}

QString AboutDlg::loadText( const QString & fileName )
{
	QString text;

	QFile f(fileName);
	if(f.open(QIODevice::ReadOnly)) {
		QTextStream t(&f);
		while(!t.atEnd())
			text += t.readLine() + '\n';
		f.close();
	}

	return text;
}


QString AboutDlg::details( QString name, QString email, QString jabber, QString www, QString desc )
{
	QString ret;
	const QString nbsp = "&nbsp;&nbsp;";
	ret += name + "<br>\n";
	if ( !email.isEmpty() )
		ret += nbsp + "E-mail: " + "<a href=\"mailto:" + email + "\">" + email + "</a><br>\n";
	if ( !jabber.isEmpty() )
		ret += nbsp + "XMPP: " + "<a href=\"xmpp:" + jabber + "\">" + jabber + "</a><br>\n";
	if ( !www.isEmpty() )
		ret += nbsp + "WWW: " + "<a href=\"" + www + "\">" + www + "</a><br>\n";
	if ( !desc.isEmpty() )
		ret += nbsp + desc + "<br>\n";
	ret += "<br>\n";

	return ret;
}

