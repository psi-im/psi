/*
 * urlwatcherplugin.cpp - Psi plugin to log URLs to a widget
 * Copyright (C) 2006  Kevin Smith
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QtCore>
#include <QtGui>
#include <QString>

#include "psiplugin.h"
#include "eventfilter.h"
#include "urlevent.h"


class URLWatcherPlugin : public QObject, public PsiPlugin, public EventFilter
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin EventFilter)

public:
	URLWatcherPlugin();
	~URLWatcherPlugin();

	virtual QString name() const;
	virtual QString shortName() const;
	virtual QString version() const;
	virtual QWidget* options() const;
	virtual bool enable();
	virtual bool disable();

    virtual bool processEvent(int account, const QDomElement& e);
	virtual bool processMessage(int account, const QString& fromJid, const QString& body, const QString& subject) ;

private:
	bool enabled_;
	QList<URLEvent> urls_;
	QWidget* viewer_;
	QTextEdit* viewerText_;
};

Q_EXPORT_PLUGIN(URLWatcherPlugin);

URLWatcherPlugin::URLWatcherPlugin()
{
	enabled_ = false;
	viewer_ = new QWidget();
	QVBoxLayout *vboxLayout;
	QFrame *frame;
	QLabel *label;
	vboxLayout = new QVBoxLayout(viewer_);
	vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
	frame = new QFrame(viewer_);
	frame->setObjectName(QString::fromUtf8("frame"));
	frame->setFrameShape(QFrame::StyledPanel);
	frame->setFrameShadow(QFrame::Raised);
	viewerText_ = new QTextEdit(frame);
	viewerText_->setObjectName(QString::fromUtf8("text"));
	viewerText_->setReadOnly(true);



	label = new QLabel(viewer_);
	label->setObjectName(QString::fromUtf8("label"));
	label->setText("URLs");
	vboxLayout->addWidget(label);
	vboxLayout->addWidget(viewerText_);
	//viewer_->show();
}

URLWatcherPlugin::~URLWatcherPlugin()
{
	delete viewer_;
}

QString URLWatcherPlugin::name() const
{
	return "URL Watcher Plugin";
}

QString URLWatcherPlugin::shortName() const
{
	return "urlWatcher";
}

QString URLWatcherPlugin::version() const
{
	return "0.2";
}

QWidget* URLWatcherPlugin::options() const
{
	return 0;
}

bool URLWatcherPlugin::enable()
{
	viewer_->show();
	enabled_ = true;
	return true;
}

bool URLWatcherPlugin::disable()
{
	viewer_->hide();
	enabled_ = false;
	return true;
}

QString resolveEntities(const QString &in)
{
	QString out;

	for(int i = 0; i < (int)in.length(); ++i) {
		if(in[i] == '&') {
			// find a semicolon
			++i;
			int n = in.indexOf(';', i);
			if(n == -1)
				break;
			QString type = in.mid(i, (n-i));
			i = n; // should be n+1, but we'll let the loop increment do it

			if(type == "amp")
				out += '&';
			else if(type == "lt")
				out += '<';
			else if(type == "gt")
				out += '>';
			else if(type == "quot")
				out += '\"';
			else if(type == "apos")
				out += '\'';
		}
		else {
			out += in[i];
		}
	}

	return out;
}


static bool linkify_pmatch(const QString &str1, int at, const QString &str2)
{
	if(str2.length() > (str1.length()-at))
		return false;

	for(int n = 0; n < (int)str2.length(); ++n) {
		if(str1.at(n+at).toLower() != str2.at(n).toLower())
			return false;
	}

	return true;
}

static bool linkify_isOneOf(const QChar &c, const QString &charlist)
{
	for(int i = 0; i < (int)charlist.length(); ++i) {
		if(c == charlist.at(i))
			return true;
	}

	return false;
}

// encodes a few dangerous html characters
static QString linkify_htmlsafe(const QString &in)
{
	QString out;

	for(int n = 0; n < in.length(); ++n) {
		if(linkify_isOneOf(in.at(n), "\"\'`<>")) {
			// hex encode
			QString hex;
			hex.sprintf("%%%02X", in.at(n).toLatin1());
			out.append(hex);
		}
		else {
			out.append(in.at(n));
		}
	}

	return out;
}

static bool linkify_okUrl(const QString &url)
{
	if(url.at(url.length()-1) == '.')
		return false;

	return true;
}

static bool linkify_okEmail(const QString &addy)
{
	// this makes sure that there is an '@' and a '.' after it, and that there is
	// at least one char for each of the three sections
	int n = addy.indexOf('@');
	if(n == -1 || n == 0)
		return false;
	int d = addy.indexOf('.', n+1);
	if(d == -1 || d == 0)
		return false;
	if((addy.length()-1) - d <= 0)
		return false;
	if(addy.indexOf("..") != -1)
		return false;

	return true;
}

bool URLWatcherPlugin::processEvent(int account, const QDomElement& e)
{
	Q_UNUSED(account);
	Q_UNUSED(e);
	return false;
}


bool URLWatcherPlugin::processMessage(int account, const QString& fromJid, const QString& body, const QString& subject)
{
	Q_UNUSED(account);
	Q_UNUSED(subject);

	QString newUrl;
	QString out = body;
	int x1, x2;
	bool isUrl, isEmail;
	QString linked, link, href;

	for(int n = 0; n < (int)out.length(); ++n) {
		isUrl = false;
		isEmail = false;
		x1 = n;

		if(linkify_pmatch(out, n, "http://")) {
			n += 7;
			isUrl = true;
			href = "";
		}
		else if(linkify_pmatch(out, n, "https://")) {
			n += 8;
			isUrl = true;
			href = "";
		}
		else if(linkify_pmatch(out, n, "ftp://")) {
			n += 6;
			isUrl = true;
			href = "";
		}
		else if(linkify_pmatch(out, n, "news://")) {
			n += 7;
			isUrl = true;
			href = "";
		}
		else if (linkify_pmatch(out, n, "ed2k://")) {
			n += 7;
			isUrl = true;
			href = "";
		}
		else if(linkify_pmatch(out, n, "www.")) {
			isUrl = true;
			href = "http://";
		}
		else if(linkify_pmatch(out, n, "ftp.")) {
			isUrl = true;
			href = "ftp://";
		}
		else if(linkify_pmatch(out, n, "@")) {
			isEmail = true;
			href = "mailto:";
		}

		if(isUrl) {
			// make sure the previous char is not alphanumeric
			if(x1 > 0 && out.at(x1-1).isLetterOrNumber())
				continue;

			// find whitespace (or end)
			for(x2 = n; x2 < (int)out.length(); ++x2) {
				if(out.at(x2).isSpace() || out.at(x2) == '<')
					break;
			}
			int len = x2-x1;
			QString pre = resolveEntities(out.mid(x1, x2-x1));

			// go backward hacking off unwanted punctuation
			int cutoff;
			for(cutoff = pre.length()-1; cutoff >= 0; --cutoff) {
				if(!linkify_isOneOf(pre.at(cutoff), "!?,.()[]{}<>\""))
					break;
			}
			++cutoff;
			//++x2;

			link = pre.mid(0, cutoff);
			if(!linkify_okUrl(link)) {
				n = x1 + link.length();
				continue;
			}
			href += link;
			href = linkify_htmlsafe(href);
			//printf("link: [%s], href=[%s]\n", link.latin1(), href.latin1());
			linked = QString("<a href=\"%1\">").arg(href) + Qt::escape(link) + "</a>" + Qt::escape(pre.mid(cutoff));
			out.replace(x1, len, linked);
			n = x1 + linked.length() - 1;
		}
		else if(isEmail) {
			// go backward till we find the beginning
			if(x1 == 0)
				continue;
			--x1;
			for(; x1 >= 0; --x1) {
				if(!linkify_isOneOf(out.at(x1), "_.-") && !out.at(x1).isLetterOrNumber())
					break;
			}
			++x1;

			// go forward till we find the end
			x2 = n + 1;
			for(; x2 < (int)out.length(); ++x2) {
				if(!linkify_isOneOf(out.at(x2), "_.-") && !out.at(x2).isLetterOrNumber())
					break;
			}

			int len = x2-x1;
			link = out.mid(x1, len);
			//link = resolveEntities(link);

			if(!linkify_okEmail(link)) {
				n = x1 + link.length();
				continue;
			}

			href += link;
			//printf("link: [%s], href=[%s]\n", link.latin1(), href.latin1());
			linked = QString("<a href=\"%1\">").arg(href) + link + "</a>";
			out.replace(x1, len, linked);
			n = x1 + linked.length() - 1;
		}
	}
	newUrl=out;
	qWarning("string parsed");
	if (isUrl)
	{
		qWarning(qPrintable(QString("it's a url %1").arg(newUrl)));
		newUrl=QString("&lt;%1> <a href='%2'>%2</a><br />").arg(fromJid).arg(newUrl);
		viewerText_->append(newUrl);
	}

	return false;
}

#include "urlwatcherplugin.moc"
