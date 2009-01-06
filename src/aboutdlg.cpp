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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QTextStream>
#include <QFile>

#include "applicationinfo.h"
#include "aboutdlg.h"
#include "textutil.h"

AboutDlg::AboutDlg(QWidget* parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	ui_.setupUi(this);

	setModal(false);

	ui_.lb_name->setText ( QString("<h3><b>%1 v%2</b></h3>").arg(ApplicationInfo::name()).arg(ApplicationInfo::version()) );

	//QFont f = ui_.te_license->font();
	//f.setFixedPitch(true);
	//ui_.te_license->setFont(f);

	ui_.te_license->setText ( loadText(":/COPYING") );

	QString lang_name = qApp->translate( "@default", "language_name" );
	if ( lang_name == "language_name" ) // remove the translation tab, if no translation is used
		ui_.tw_tabs->removePage ( ui_.tw_tabs->page(3) );

	// ###cuda
	ui_.tw_tabs->removePage ( ui_.tw_tabs->page(2) );
	ui_.tw_tabs->removePage ( ui_.tw_tabs->page(1) );

	// fill in Authors tab...
	QString authors;
	authors += details(QString::fromUtf8("Justin Karneges"),
			   "justin@affinix.com", "", "",
			   tr("Founder and Original Author"));
	authors += details(QString::fromUtf8("Kevin Smith"),
			   "kismith@psi-im.org", "", "",
			   tr("Project Lead/Maintainer"));
	authors += details(QString::fromUtf8("Remko Tronçon"),
			   "remko@psi-im.org", "", "",
			   tr("Lead Developer"));
	authors += details(QString::fromUtf8("Michail Pishchagin"),
			   "mblsha@psi-im.org", "", "",
			   tr("Lead Widget Developer"));
	authors += details(QString::fromUtf8("Maciej Niedzielski"),
			   "machekku@psi-im.org", "", "",
			   tr("Developer"));
	authors += details(QString::fromUtf8("Martin Hostettler"),
			   "martin@psi-im.org", "", "",
			   tr("Developer"));
	authors += details(QString::fromUtf8("Akito Nozaki"),
			   "anpluto@usa.net", "", "",
			   tr("Miscellaneous Developer"));
	ui_.te_authors->setText( authors );

	// fill in Thanks To tab...
	QString thanks;
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
					  
	//thanks += tr("Thanks to many others.\n"
	//	     "The above list only reflects the contributors I managed to keep track of.\n"
	//	     "If you're not included but you think that you must be in the list, contact the developers.");
	ui_.te_thanks->setText( thanks );
}

static QString cleanup_text(const QString &in)
{
	QString out;
	for(int n = 0; n < in.length(); ++n)
	{
		if(in[n] == '\t')
			out += "        ";
		else if(in[n] == '\n' || in[n] == '\r')
			out += in[n];
		else if(in[n].isPrint())
			out += in[n];
	}
	return out;
}

QString AboutDlg::loadText( const QString & fileName )
{
	QString text;

	QFile f(fileName);
	if(f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		//text += "<tt>";
		QTextStream t(&f);
		t.setCodec("UTF-8");
		while(!t.atEnd())
		{
			QString line = t.readLine();
			line = cleanup_text(line);
			text += line + '\n';
		}
		f.close();
		//text += "</tt>";
	}

	return QString("<tt>") + TextUtil::plain2rich(text) + "</tt>";
}


QString AboutDlg::details( QString name, QString email, QString jabber, QString www, QString desc )
{
	QString ret;
	const QString nbsp = "&nbsp;&nbsp;";
	ret += name + "<br>\n";
	if ( !email.isEmpty() )
		ret += nbsp + "E-mail: " + "<a href=\"mailto:" + email + "\">" + email + "</a><br>\n";
	if ( !jabber.isEmpty() )
		ret += nbsp + "Jabber: " + "<a href=\"jabber:" + jabber + "\">" + jabber + "</a><br>\n";
	if ( !www.isEmpty() )
		ret += nbsp + "WWW: " + "<a href=\"" + www + "\">" + www + "</a><br>\n";
	if ( !desc.isEmpty() )
		ret += nbsp + desc + "<br>\n";
	ret += "<br>\n";

	return ret;
}

