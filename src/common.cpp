/*
 * common.cpp - contains all the common variables and functions for Psi
 * Copyright (C) 2001-2003  Justin Karneges
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

#include "common.h"

#include "profiles.h"
#include "rtparse.h"

#include "psievent.h"

#include <QUrl>
#include <Q3PtrList>
#include <Q3Process>
#include <QBoxLayout>

QString PROG_NAME = "Psi";
QString PROG_VERSION = "0.11-dev" " (" __DATE__ ")"; //CVS Builds are dated
//QString PROG_VERSION = "0.11-beta3";
QString PROG_CAPS_NODE = "http://psi-im.org/caps";
QString PROG_CAPS_VERSION = "0.11-dev-rev7";
QString PROG_OPTIONS_NS = "http://psi-im.org/options";
QString PROG_STORAGE_NS = "http://psi-im.org/storage";

#ifdef HAVE_CONFIG
#include "config.h"
#endif

#ifndef PSI_DATADIR
#define PSI_DATADIR "/usr/local/share/psi"
#endif

#include <QRegExp>
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QSound>
#include <QObject>
#include <QLibrary>
#include <QTextDocument> // for Qt::escape()
#include <QSysInfo>


#include <stdio.h>

#ifdef Q_WS_X11
#include <QX11Info>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

#ifdef Q_WS_WIN
#include <windows.h>
#endif

#ifdef Q_WS_MAC
#include <sys/types.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h> // for HIToolbox/InternetConfig
#include <CoreServices/CoreServices.h>
#endif

QString activeProfile;

//QStringList dtcp_hostList;
//int dtcp_port;
//QString dtcp_proxy;
//bool link_test = false;
Qt::WFlags psi_dialog_flags = (Qt::WStyle_SysMenu | Qt::WStyle_MinMax);
//uint psi_dialog_flags = (Qt::WStyle_Customize | Qt::WStyle_DialogBorder | Qt::WStyle_Title | Qt::WStyle_SysMenu | Qt::WStyle_MinMax);

Options option;
PsiGlobal g;
PsiIconset *is;
bool useSound;


QString qstrlower(QString str)
{
	for(int n = 0; n < str.length(); ++n) {
		str[n] = str[n].lower();
	}

	return str;
}

int qstrcasecmp(const QString &str1, const QString &str2)
{
        if(str1.length() != str2.length())
                return 1;

        for(int n = 0; n < str1.length(); ++n) {
                if(str1.at(n).lower() != str2.at(n).lower())
                        return 1;
        }

        return 0;
}

int qstringlistmatch(QStringList &list, const QString &str)
{
	int n = 0;

        for(QStringList::Iterator i = list.begin(); i != list.end(); ++i, ++n) {
                if(*i == str)
                        return n;
        }

        return -1;
}

QString qstringlistlookup(QStringList &list, int x)
{
	int n = 0;
	QStringList::Iterator i = list.begin();
	for(;i != list.end() && n < x; ++i, ++n);
	if(n != x)
		return "";

	return *i;
}

QString CAP(const QString &str)
{
	return QString("%1: %2").arg(PROG_NAME).arg(str);
}

QString qstrquote(const QString &toquote, int width, bool quoteEmpty)
{
	int ql = 0, col = 0, atstart = 1, ls=0;

	QString quoted = "> "+toquote;  // quote first line
	QString rxs  = quoteEmpty ? "\n" : "\n(?!\\s*\n)";
	QRegExp rx(rxs);                // quote following lines
	quoted.replace(rx, "\n> ");
	rx.setPattern("> +>");          // compress > > > > quotes to >>>>
	quoted.replace(rx, ">>");
	quoted.replace(rx, ">>");
	quoted.replace(QRegExp(" +\n"), "\n");  // remove trailing spaces

	if (!quoteEmpty)
	{
		quoted.replace(QRegExp("^>+\n"), "\n\n");  // unquote empty lines
		quoted.replace(QRegExp("\n>+\n"), "\n\n");
	}


	for (int i=0;i<(int) quoted.length();i++)
	{
		col++;
		if (atstart && quoted[i] == '>') ql++; else atstart=0;

		switch(quoted[i].latin1())
		{
			case '\n': ql = col = 0; atstart = 1; break;
			case ' ':
			case '\t': ls = i; break;

		}
		if (quoted[i]=='\n') { ql=0; atstart = 1; }

		if (col > width)
		{
			if ((ls+width) < i)
			{
				ls = i; i = quoted.length();
				while ((ls<i) && !quoted[ls].isSpace()) ls++;
				i = ls;
			}
			if ((i<(int)quoted.length()) && (quoted[ls] != '\n'))
			{
				quoted.insert(ls, '\n');
				++ls;
				quoted.insert(ls, QString().fill('>', ql));
				i += ql+1;
				col = 0;
			}
		}
	}
	quoted += "\n\n";// add two empty lines to quoted text - the cursor
			// will be positioned at the end of those.
	return quoted;
}

QString plain2rich(const QString &plain)
{
	QString rich;
	int col = 0;

	for(int i = 0; i < (int)plain.length(); ++i) {
		if(plain[i] == '\n') {
			rich += "<br>";
			col = 0;
		}
		else if(plain[i] == '\t') {
			rich += QChar::nbsp;
			while(col % 4) {
				rich += QChar::nbsp;
				++col;
			}
		}
		else if(plain[i].isSpace()) {
			if(i > 0 && plain[i-1] == ' ')
				rich += QChar::nbsp;
			else
				rich += ' ';
		}
		else if(plain[i] == '<')
			rich += "&lt;";
		else if(plain[i] == '>')
			rich += "&gt;";
		else if(plain[i] == '\"')
			rich += "&quot;";
		else if(plain[i] == '\'')
			rich += "&apos;";
		else if(plain[i] == '&')
			rich += "&amp;";
		else
			rich += plain[i];
		++col;
	}

	return rich;
}

QString rich2plain(const QString &in)
{
	QString out;

	for(int i = 0; i < (int)in.length(); ++i) {
		// tag?
		if(in[i] == '<') {
			// find end of tag
			++i;
			int n = in.find('>', i);
			if(n == -1)
				break;
			QString str = in.mid(i, (n-i));
			i = n;

			QString tagName;
			n = str.find(' ');
			if(n != -1)
				tagName = str.mid(0, n);
			else
				tagName = str;

			if(tagName == "br")
				out += '\n';
			
			// handle output of Qt::convertFromPlainText() correctly
			if((tagName == "p" || tagName == "/p") && out.length() > 0)
				out += '\n';
		}
		// entity?
		else if(in[i] == '&') {
			// find a semicolon
			++i;
			int n = in.find(';', i);
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
		else if(in[i].isSpace()) {
			if(in[i] == QChar::nbsp)
				out += ' ';
			else if(in[i] != '\n') {
				if(i == 0)
					out += ' ';
				else {
					QChar last = out.at(out.length()-1);
					bool ok = TRUE;
					if(last.isSpace() && last != '\n')
						ok = FALSE;
					if(ok)
						out += ' ';
				}
			}
		}
		else {
			out += in[i];
		}
	}

	return out;
}


// clips plain text
QString clipStatus(const QString &str, int width, int height)
{
	QString out = "";
	int at = 0;
	int len = str.length();
	if(len == 0)
		return out;

	// only take the first "height" lines
	for(int n2 = 0; n2 < height; ++n2) {
		// only take the first "width" chars
		QString line;
		bool hasNewline = false;
		for(int n = 0; at < len; ++n, ++at) {
			if(str.at(at) == '\n') {
				hasNewline = true;
				break;
			}
			line += str.at(at);
		}
		++at;
		if((int)line.length() > width) {
			line.truncate(width-3);
			line += "...";
		}
		out += line;
		if(hasNewline)
			out += '\n';

		if(at >= len)
			break;
	}

	return out;
}

QString resolveEntities(const QString &in)
{
	QString out;

	for(int i = 0; i < (int)in.length(); ++i) {
		if(in[i] == '&') {
			// find a semicolon
			++i;
			int n = in.find(';', i);
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
		return FALSE;

	for(int n = 0; n < (int)str2.length(); ++n) {
		if(str1.at(n+at).lower() != str2.at(n).lower())
			return FALSE;
	}

	return TRUE;
}

static bool linkify_isOneOf(const QChar &c, const QString &charlist)
{
	for(int i = 0; i < (int)charlist.length(); ++i) {
		if(c == charlist.at(i))
			return TRUE;
	}

	return FALSE;
}

// encodes a few dangerous html characters
static QString linkify_htmlsafe(const QString &in)
{
	QString out;

	for(int n = 0; n < in.length(); ++n) {
		if(linkify_isOneOf(in.at(n), "\"\'`<>")) {
			// hex encode
			QString hex;
			hex.sprintf("%%%02X", in.at(n).latin1());
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
		return FALSE;

	return TRUE;
}

static bool linkify_okEmail(const QString &addy)
{
	// this makes sure that there is an '@' and a '.' after it, and that there is
	// at least one char for each of the three sections
	int n = addy.find('@');
	if(n == -1 || n == 0)
		return FALSE;
	int d = addy.find('.', n+1);
	if(d == -1 || d == 0)
		return FALSE;
	if((addy.length()-1) - d <= 0)
		return FALSE;
	if(addy.find("..") != -1)
		return false;

	return TRUE;
}

QString linkify(const QString &in)
{
	QString out = in;
	int x1, x2;
	bool isUrl, isEmail;
	QString linked, link, href;

	for(int n = 0; n < (int)out.length(); ++n) {
		isUrl = FALSE;
		isEmail = FALSE;
		x1 = n;

		if(linkify_pmatch(out, n, "http://")) {
			n += 7;
			isUrl = TRUE;
			href = "";
		}
		else if(linkify_pmatch(out, n, "https://")) {
			n += 8;
			isUrl = TRUE;
			href = "";
		}
		else if(linkify_pmatch(out, n, "ftp://")) {
			n += 6;
			isUrl = TRUE;
			href = "";
		}
		else if(linkify_pmatch(out, n, "news://")) {
			n += 7;
			isUrl = TRUE;
			href = "";
		}
		else if (linkify_pmatch(out, n, "ed2k://")) {
			n += 7;
			isUrl = TRUE;
			href = "";
		}
		else if(linkify_pmatch(out, n, "www.")) {
			isUrl = TRUE;
			href = "http://";
		}
		else if(linkify_pmatch(out, n, "ftp.")) {
			isUrl = TRUE;
			href = "ftp://";
		}
		else if(linkify_pmatch(out, n, "@")) {
			isEmail = TRUE;
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

	return out;
}

// sickening
QString emoticonify(const QString &in)
{
	RTParse p(in);
	while ( !p.atEnd() ) {
		// returns us the first chunk as a plaintext string
		QString str = p.next();

		int i = 0;
		while ( i >= 0 ) {
			// find closest emoticon
			int ePos = -1;
			Icon *closest = 0;

			int foundPos = -1, foundLen = -1;

			Q3PtrListIterator<Iconset> iconsets(is->emoticons);
			Iconset *iconset;
    			while ( (iconset = iconsets.current()) != 0 ) {
				QListIterator<Icon*> it = iconset->iterator();
				while ( it.hasNext()) {
					Icon *icon = it.next();
					if ( icon->regExp().isEmpty() )
						continue;

					// some hackery
					int iii = i;
					bool searchAgain;

					do {
						searchAgain = false;

						// find the closest match
						const QRegExp &rx = icon->regExp();
						int n = rx.search(str, iii);
						if ( n == -1 )
							continue;

						if(ePos == -1 || n < ePos || (rx.matchedLength() > foundLen && n < ePos + foundLen)) {
							// there must be whitespace at least on one side of the emoticon
							if ( ( n == 0 ) ||
							     ( n+rx.matchedLength() == (int)str.length() ) ||
							     ( n > 0 && str[n-1].isSpace() ) ||
							     ( n+rx.matchedLength() < (int)str.length() && str[n+rx.matchedLength()].isSpace() ) )
							{
								ePos = n;
								closest = icon;

								foundPos = n;
								foundLen = rx.matchedLength();
								break;
							}

							searchAgain = true;
						}

						iii = n + rx.matchedLength();
					} while ( searchAgain );
				}

				++iconsets;
			}

			QString s;
			if(ePos == -1)
				s = str.mid(i);
			else
				s = str.mid(i, ePos-i);
			p.putPlain(s);

			if ( !closest )
				break;

			p.putRich( QString("<icon name=\"%1\" text=\"%2\">").arg(Qt::escape(closest->name())).arg(Qt::escape(str.mid(foundPos, foundLen))) );
			i = foundPos + foundLen;
		}
	}

	QString out = p.output();

	//enable *bold* stuff
	// //old code
	//out=out.replace(QRegExp("(^[^<>\\s]*|\\s[^<>\\s]*)\\*(\\S+)\\*([^<>\\s]*\\s|[^<>\\s]*$)"),"\\1<b>*\\2*</b>\\3");
	//out=out.replace(QRegExp("(^[^<>\\s\\/]*|\\s[^<>\\s\\/]*)\\/([^\\/\\s]+)\\/([^<>\\s\\/]*\\s|[^<>\\s\\/]*$)"),"\\1<i>/\\2/</i>\\3");
	//out=out.replace(QRegExp("(^[^<>\\s]*|\\s[^<>\\s]*)_(\\S+)_([^<>\\s]*\\s|[^<>\\s]*$)"),"\\1<u>_\\2_</u>\\3");

	out=out.replace(QRegExp("(^_|\\s_)(\\S+)(_\\s|_$)"),"\\1<u>\\2</u>\\3");
	out=out.replace(QRegExp("(^\\*|\\s\\*)(\\S+)(\\*\\s|\\*$)"),"\\1<b>\\2</b>\\3");
	out=out.replace(QRegExp("(^\\/|\\s\\/)(\\S+)(\\/\\s|\\/$)"),"\\1<i>\\2</i>\\3");

	return out;
}

QString encodePassword(const QString &pass, const QString &key)
{
	QString result;
	int n1, n2;

	if(key.length() == 0)
		return pass;

	for(n1 = 0, n2 = 0; n1 < pass.length(); ++n1) {
		ushort x = pass.at(n1).unicode() ^ key.at(n2++).unicode();
		QString hex;
		hex.sprintf("%04x", x);
		result += hex;
		if(n2 >= key.length())
			n2 = 0;
	}
	return result;
}

QString decodePassword(const QString &pass, const QString &key)
{
	QString result;
	int n1, n2;

	if(key.length() == 0)
		return pass;

	for(n1 = 0, n2 = 0; n1 < pass.length(); n1 += 4) {
		ushort x = 0;
		if(n1 + 4 > pass.length())
			break;
		x += hexChar2int(pass.at(n1).toLatin1())*4096;
		x += hexChar2int(pass.at(n1+1).toLatin1())*256;
		x += hexChar2int(pass.at(n1+2).toLatin1())*16;
		x += hexChar2int(pass.at(n1+3).toLatin1());
		QChar c(x ^ key.at(n2++).unicode());
		result += c;
		if(n2 >= key.length())
			n2 = 0;
	}
	return result;
}

QString status2txt(int status)
{
	switch(status) {
		case STATUS_OFFLINE:    return QObject::tr("Offline");
		case STATUS_AWAY:       return QObject::tr("Away");
		case STATUS_XA:         return QObject::tr("Not Available");
		case STATUS_DND:        return QObject::tr("Do not Disturb");
		case STATUS_CHAT:       return QObject::tr("Free for Chat");
		case STATUS_INVISIBLE:  return QObject::tr("Invisible");

		case STATUS_ONLINE:
		default:                return QObject::tr("Online");
	}
}

Icon category2icon(const QString &category, const QString &type, int status)
{
	// TODO: update this to http://www.jabber.org/registrar/disco-categories.html#gateway

	// still have to add more options...
	if ( category == "service" || category == "gateway" ) {
		QString trans;

		if (type == "aim")
			trans = "aim";
		else if (type == "icq")
			trans = "icq";
		else if (type == "msn")
			trans = "msn";
		else if (type == "yahoo")
			trans = "yahoo";
		else if (type == "gadu-gadu" || type == "x-gadugadu")
			trans = "gadugadu";
		else if (type == "sms")
			trans = "sms";
		else
			trans = "transport";

		return is->transportStatus(trans, status);

		// irc
		// jud
		// pager
		// jabber
		// serverlist
		// smtp
	}
	else if ( category == "conference" ) {
		if (type == "public" || type == "private" || type == "text" || type == "irc")
			return IconsetFactory::icon("psi/groupChat");
		else if (type == "url")
			return IconsetFactory::icon("psi/www");
		// irc
		// list
		// topic
	}
	else if ( category == "validate" ) {
		if (type == "xml")
			return IconsetFactory::icon("psi/xml");
		// grammar
		// spell
	}
	else if ( category == "user" || category == "client" ) {
		// client
		// forward
		// inbox
		// portable
		// voice
		return is->status(STATUS_ONLINE);
	}
	// application
	   // bot
	   // calendar
	   // editor
	   // fileserver
	   // game
	   // whiteboard
	// headline
	   // logger
	   // notice
	   // rss
	   // stock
	// keyword
	   // dictionary
	   // dns
	   // software
	   // thesaurus
	   // web
	   // whois
	// render
	   // en2ru
	   // ??2??
	   // tts
	return Icon();
}

int hexChar2int(char c)
{
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if(c >= '0' && c <= '9')
		return c - '0';

	return 0;
}

char int2hexChar(int x)
{
	if(x < 10)
		return (char)x + '0';
	else
		return (char)x - 10 + 'a';
}

QString logencode(QString str)
{
        str.replace(QRegExp("\\\\"), "\\\\");   // backslash to double-backslash
        str.replace(QRegExp("\\|"), "\\p");     // pipe to \p
        str.replace(QRegExp("\n"), "\\n");      // newline to \n
        return str;
}

QString logdecode(const QString &str)
{
        QString ret;

        for(int n = 0; n < str.length(); ++n) {
                if(str.at(n) == '\\') {
                        ++n;
                        if(n >= str.length())
                                break;

                        if(str.at(n) == 'n')
                                ret.append('\n');
                        if(str.at(n) == 'p')
                                ret.append('|');
                        if(str.at(n) == '\\')
                                ret.append('\\');
                }
                else {
                        ret.append(str.at(n));
                }
        }

        return ret;
}

void qstringlistisort(QStringList &c)
{
	if ( c.count() <= 1 )
		return;

	QStringList::Iterator it;
	int size = c.count();

	// first, make array that is easy (and quick) to manipulate
	QString *heap = new QString[ size ];

	int i = 0;
	for (it = c.begin(); it != c.end(); ++it)
		heap[i++] = *it;

	// Insertion sort
	for (int tmp = 0; tmp < c.count(); tmp++) {
		heap[tmp] = c[tmp];
		size = tmp + 1;

		for (int j = 1; j < size; j++) {
			QString k = heap[j].lower();
			QString r = heap[j];

			for (i = j - 1; i >= 0; i--) {
				if ( QString::compare(k, heap[i].lower()) > 0 )
					break;

				heap[i+1] = heap[i];
			}

			heap[i+1] = r;
		}
	}

	// now, copy sorted data back to QStringList
	it = c.begin();
	for (i = 0; i < (int)size; i++)
		*it++ = heap[i];

	delete[] heap;
}

void openURL(const QString &url)
{
	//fprintf(stderr, "openURL: [%s]\n", url.latin1());
	bool useCustom = TRUE;

	int colon = url.find(':');
	if ( colon == -1 )
		colon = 0;
	QString service = url.left( colon );
	if ( service == "jabber" || service == "jid" ) {
		// TODO
		return;
	}

#ifdef Q_WS_WIN
	if(option.browser == 0)
		useCustom = FALSE;
#endif
#ifdef Q_WS_X11
	if(option.browser == 0 || option.browser == 2)
		useCustom = FALSE;
#endif
#ifdef Q_WS_MAC
	useCustom = FALSE;
#endif

	if(useCustom) {
		bool isMail = FALSE;
		QString s = url;
		if(url.left(7) == "mailto:") {
			s.remove(0, 7);
			isMail = TRUE;
		}

		QStringList args;

		if(isMail) {
			if(option.customMailer.isEmpty()) {
				QMessageBox::critical(0, CAP(QObject::tr("URL error")), QObject::tr("Unable to open the URL. You have not selected a mailer (see Options)."));
				return;
			}
			args += QStringList::split(' ', option.customMailer);
		}
		else {
			if(option.customBrowser.isEmpty()) {
				QMessageBox::critical(0, CAP(QObject::tr("URL error")), QObject::tr("Unable to open the URL. You have not selected a browser (see Options)."));
				return;
			}
			args += QStringList::split(' ', option.customBrowser);
		}

		args += s;
		Q3Process cmd(args);
		if(!cmd.start()) {
			QMessageBox::critical(0, CAP(QObject::tr("URL error")), QObject::tr("Unable to open the URL. Ensure that your custom browser/mailer exists (see Options)."));
		}
	}
	else {
		QUrl qurl(url);
		
		// If URL already has escaped chars, we need to tell QUrl
		// that it's escaped. The only problem would be when URL
		// would contain both escaped and unescaped chars.
		//
		// Example of valid URL:
		// http://ru.wikipedia.org/wiki/%D0%90%D0%BD%D0%B8%D0%BC%D0%B5
		QRegExp encodedChar("%[\\da-fA-F]{2}");
		if (encodedChar.indexIn(url) != -1)
			qurl.setEncodedUrl(url.toLatin1());
		
#ifdef Q_WS_WIN
		QByteArray cs = qurl.toEncoded();
		if ((unsigned int)::ShellExecuteA(NULL,NULL,cs.data(),NULL,NULL,SW_SHOW) <= 32) {
			QMessageBox::critical(0, CAP(QObject::tr("URL error")), QObject::tr("Unable to open the URL. Ensure that you have a web browser installed."));
		}
#endif
#ifdef Q_WS_X11
		// KDE
		if(option.browser == 0) {
			QStringList args;
			args += "kfmclient";
			args += "exec";
			args += url;
			Q3Process cmd(args);
			if(!cmd.start()) {
				QMessageBox::critical(0, CAP(QObject::tr("URL error")), QObject::tr("Unable to open the URL. Ensure that you have KDE installed."));
			}
		}
		// GNOME 2
		else if(option.browser == 2) {
			QStringList args;
			args += "gnome-open";
			args += url;
			Q3Process cmd(args);
			if(!cmd.start()) {
				QMessageBox::critical(0, CAP(QObject::tr("URL error")), QObject::tr("Unable to open the URL. Ensure that you have GNOME 2 installed."));
			}
		}
#endif
#ifdef Q_WS_MAC
		// Use Internet Config to hand the URL to the appropriate application, as
		// set by the user in the Internet Preferences pane.
		// NOTE: ICStart could be called once at Psi startup, saving the
		//       ICInstance in a global variable, as a minor optimization.
		//       ICStop should then be called at Psi shutdown if ICStart succeeded.
		ICInstance icInstance;
		OSType psiSignature = 'psi ';
		OSStatus error = ::ICStart( &icInstance, psiSignature );
		if ( error == noErr ) {
			ConstStr255Param hint( 0x0 );
			QByteArray cs = qurl.toEncoded();
			long start( 0 );
			long end( cs.length() );
			// Don't bother testing return value (error); launched application will report problems.
			::ICLaunchURL( icInstance, hint, cs.data(), cs.length(), &start, &end );
			ICStop( icInstance );
		}
#endif
	}
}

static bool sysinfo_done = FALSE;
static int timezone_offset = 0;
static QString timezone_str = "N/A";
static QString os_str = "Unknown";

#if defined(Q_WS_X11) || defined(Q_WS_MAC)
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#endif

static void getSysInfo()
{
#if defined(Q_WS_X11) || defined(Q_WS_MAC)
	time_t x;
	time(&x);
	char str[256];
	char fmt[32];
	strcpy(fmt, "%z");
	strftime(str, 256, fmt, localtime(&x));
	if(strcmp(fmt, str)) {
		QString s = str;
		if(s.at(0) == '+')
			s.remove(0,1);
		s.truncate(s.length()-2);
		timezone_offset = s.toInt();
	}
	strcpy(fmt, "%Z");
	strftime(str, 256, fmt, localtime(&x));
	if(strcmp(fmt, str))
		timezone_str = str;
#endif
#if defined(Q_WS_X11)
	struct utsname u;
	uname(&u);
	os_str.sprintf("%s", u.sysname);

	// get description about os
	enum LinuxName {
		LinuxNone = 0,

		LinuxMandrake,
		LinuxDebian,
		LinuxRedHat,
		LinuxGentoo,
		LinuxSlackware,
		LinuxSuSE,
		LinuxConectiva,
		LinuxCaldera,
		LinuxLFS,

		LinuxASP, // Russian Linux distros
		LinuxALT,

		LinuxPLD, // Polish Linux distros
		LinuxAurox,
		LinuxArch
	};

	enum OsFlags {
		OsUseName = 0,
		OsUseFile,
		OsAppendFile
	};

	struct OsInfo {
		LinuxName id;
		OsFlags flags;
		QString file;
		QString name;
	} osInfo[] = {
		{ LinuxMandrake,	OsUseFile,	"/etc/mandrake-release",	"Mandrake Linux"	},
		{ LinuxDebian,		OsAppendFile,	"/etc/debian_version",		"Debian GNU/Linux"	},
		{ LinuxGentoo,		OsUseFile,	"/etc/gentoo-release",		"Gentoo Linux"		},
		{ LinuxSlackware,	OsAppendFile,	"/etc/slackware-version",	"Slackware Linux"	},
		{ LinuxPLD,		OsUseFile,	"/etc/pld-release",		"PLD Linux"		},
		{ LinuxAurox,		OsUseName,	"/etc/aurox-release",		"Aurox Linux"		},
		{ LinuxArch,		OsUseFile,	"/etc/arch-release",		"Arch Linux"		},
		{ LinuxLFS,		OsAppendFile,	"/etc/lfs-release",		"LFS Linux"		},

		// untested
		{ LinuxSuSE,		OsUseFile,	"/etc/SuSE-release",		"SuSE Linux"		},
		{ LinuxConectiva,	OsUseFile,	"/etc/conectiva-release",	"Conectiva Linux"	},
		{ LinuxCaldera,		OsUseFile,	"/etc/.installed",		"Caldera Linux"		},

		// many distros use the /etc/redhat-release for compatibility, so RedHat will be the last :)
		{ LinuxRedHat,		OsUseFile,	"/etc/redhat-release",		"RedHat Linux"		},

		{ LinuxNone,		OsUseName,	"",				""			}
	};

	for (int i = 0; osInfo[i].id != LinuxNone; i++) {
		QFileInfo fi( osInfo[i].file );
		if ( fi.exists() ) {
			char buffer[128];

			QFile f( osInfo[i].file );
			f.open( QIODevice::ReadOnly );
			f.readLine( buffer, 128 );
			QString desc(buffer);

			desc = desc.stripWhiteSpace ();

			switch ( osInfo[i].flags ) {
				case OsUseFile:
					os_str = desc;
					break;
				case OsUseName:
					os_str = osInfo[i].name;
					break;
				case OsAppendFile:
					os_str = osInfo[i].name + " (" + desc + ")";
					break;
			}

			break;
		}
	}
#elif defined(Q_WS_MAC)
	os_str = "Mac OS X";
#endif

#if defined(Q_WS_WIN)
	TIME_ZONE_INFORMATION i;
	//GetTimeZoneInformation(&i);
	//timezone_offset = (-i.Bias) / 60;
	memset(&i, 0, sizeof(i));
	bool inDST = (GetTimeZoneInformation(&i) == TIME_ZONE_ID_DAYLIGHT);
	int bias = i.Bias;
	if(inDST)
		bias += i.DaylightBias;
	timezone_offset = (-bias) / 60;
	timezone_str = "";
	for(int n = 0; n < 32; ++n) {
		int w = inDST ? i.DaylightName[n] : i.StandardName[n];
		if(w == 0)
			break;
		timezone_str += QChar(w);
	}

	QSysInfo::WinVersion v = QSysInfo::WindowsVersion;
	if(v == QSysInfo::WV_95)
		os_str = "Windows 95";
	else if(v == QSysInfo::WV_98)
		os_str = "Windows 98";
	else if(v == QSysInfo::WV_Me)
		os_str = "Windows Me";
	else if(v == QSysInfo::WV_DOS_based)
		os_str = "Windows 9x/Me";
	else if(v == QSysInfo::WV_NT)
		os_str = "Windows NT 4.x";
	else if(v == QSysInfo::WV_2000)
		os_str = "Windows 2000";
	else if(v == QSysInfo::WV_XP)
		os_str = "Windows XP";
	else if(v == QSysInfo::WV_2003)
		os_str = "Windows Server 2003";
	else if(v == QSysInfo::WV_NT_based)
		os_str = "Windows NT";
#endif
	sysinfo_done = TRUE;
}

QString getOSName()
{
	if(!sysinfo_done)
		getSysInfo();

	return os_str;
}

int getTZOffset()
{
	if(!sysinfo_done)
		getSysInfo();

	return timezone_offset;
}

QString getTZString()
{
	if(!sysinfo_done)
		getSysInfo();

	return timezone_str;
}


#ifdef Q_WS_X11
QString getResourcesDir()
{
	return PSI_DATADIR;
}

QString getHomeDir()
{
	QDir proghome(QDir::homeDirPath() + "/.psi");
	if(!proghome.exists()) {
		QDir home = QDir::home();
		home.mkdir(".psi");
		chmod(QFile::encodeName(proghome.path()), 0700);
	}

	return proghome.path();
}
#endif

#ifdef Q_WS_WIN
QString getResourcesDir()
{
	return qApp->applicationDirPath();
}

QString getHomeDir()
{
	QString base;

	// Windows 9x
	if(QDir::homeDirPath() == QDir::rootDirPath())
		base = ".";
	// Windows NT/2K/XP variant
	else
		base = QDir::homeDirPath();

	// no trailing slash
	if(base.at(base.length()-1) == '/')
		base.truncate(base.length()-1);

	QDir proghome(base + "/PsiData");
	if(!proghome.exists()) {
		QDir home(base);
		home.mkdir("PsiData");
	}

	return proghome.path();
}
#endif

#ifdef Q_WS_MAC
/******************************************************************************/
/* Get path to Resources directory as a string.                               */
/* Return an empty string if can't find it.                                   */
/******************************************************************************/
QString getResourcesDir()
{
  // System routine locates resource files. We "know" that Psi.icns is
  // in the Resources directory.
  QString resourcePath;
  CFBundleRef mainBundle = CFBundleGetMainBundle();
  CFStringRef resourceCFStringRef
      = CFStringCreateWithCString( NULL, "application.icns",
                                   kCFStringEncodingASCII );
  CFURLRef resourceURLRef = CFBundleCopyResourceURL( mainBundle,
                                                     resourceCFStringRef,
                                                     NULL,
                                                     NULL );
  if ( resourceURLRef ) {
    CFStringRef resourcePathStringRef =
    CFURLCopyFileSystemPath( resourceURLRef, kCFURLPOSIXPathStyle );
    const char* resourcePathCString =
      CFStringGetCStringPtr( resourcePathStringRef, kCFStringEncodingASCII );
    if ( resourcePathCString ) {
      resourcePath.setLatin1( resourcePathCString );
    } else { // CFStringGetCStringPtr failed; use fallback conversion
      CFIndex bufferLength = CFStringGetLength( resourcePathStringRef ) + 1;
      char* resourcePathCString = new char[ bufferLength ];
      Boolean conversionSuccess =
        CFStringGetCString( resourcePathStringRef,
                            resourcePathCString, bufferLength,
                            kCFStringEncodingASCII );
      if ( conversionSuccess ) {
        resourcePath = resourcePathCString;
      }
      delete [] resourcePathCString;  // I own this
    }
    CFRelease( resourcePathStringRef ); // I own this
  }
  // Remove the tail component of the path
  if ( ! resourcePath.isNull() ) {
    QFileInfo fileInfo( resourcePath );
    resourcePath = fileInfo.dirPath( true );
  }
  return resourcePath;
}

QString getHomeDir()
{
	QDir proghome(QDir::homeDirPath() + "/.psi");
	if(!proghome.exists()) {
		QDir home = QDir::home();
		home.mkdir(".psi");
		chmod(QFile::encodeName(proghome.path()), 0700);
	}

	return proghome.path();
}
#endif


QString getHistoryDir()
{
	QDir history(pathToProfile(activeProfile) + "/history");
	if (!history.exists()) {
		QDir profile(pathToProfile(activeProfile));
		profile.mkdir("history");
	}

	return history.path();
}

QString getVCardDir()
{
	QDir vcard(pathToProfile(activeProfile) + "/vcard");
	if (!vcard.exists()) {
		QDir profile(pathToProfile(activeProfile));
		profile.mkdir("vcard");
	}

	return vcard.path();
}

bool fileCopy(const QString &src, const QString &dest)
{
	QFile in(src);
	QFile out(dest);

	if(!in.open(QIODevice::ReadOnly))
		return FALSE;
	if(!out.open(QIODevice::WriteOnly))
		return FALSE;

	char *dat = new char[16384];
	int n = 0;
	while(!in.atEnd()) {
		n = in.readBlock(dat, 16384);
		if(n == -1) {
			delete dat;
			return FALSE;
		}
		out.writeBlock(dat, n);
	}
	delete dat;

	out.close();
	in.close();

	return TRUE;
}


void soundPlay(const QString &str)
{
	if(str == "!beep") {
		QApplication::beep();
		return;
	}

	if(!QFile::exists(str))
		return;

#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
	QSound::play(str);
#else
	if(!option.player.isEmpty()) {
		QStringList args;
		args = QStringList::split(' ', option.player);
		args += str;
		Q3Process cmd(args);
		if(!cmd.start())
			wait3(NULL,WNOHANG,NULL);
	}
#endif
}

XMPP::Status makeStatus(int x, const QString &str, int priority)
{
	XMPP::Status s = makeStatus(x,str);
	if (priority > 127)
		s.setPriority(127);
	else if (priority < -128)
		s.setPriority(-128);
	else
		s.setPriority(priority);
	return s;
}

XMPP::Status makeStatus(int x, const QString &str)
{
	XMPP::Status s;
	s.setStatus(str);
	if(x == STATUS_OFFLINE)
		s.setIsAvailable(false);
	else if(x == STATUS_INVISIBLE)
		s.setIsInvisible(true);
	else {
		if(x == STATUS_AWAY)
			s.setShow("away");
		else if(x == STATUS_XA)
			s.setShow("xa");
		else if(x == STATUS_DND)
			s.setShow("dnd");
		else if(x == STATUS_CHAT)
			s.setShow("chat");
	}

	return s;
}

int makeSTATUS(const XMPP::Status &s)
{
	int type = STATUS_ONLINE;
	if(!s.isAvailable())
		type = STATUS_OFFLINE;
	else if(s.isInvisible())
		type= STATUS_INVISIBLE;
	else {
		if(s.show() == "away")
			type = STATUS_AWAY;
		else if(s.show() == "xa")
			type = STATUS_XA;
		else if(s.show() == "dnd")
			type = STATUS_DND;
		else if(s.show() == "chat")
			type = STATUS_CHAT;
	}
	return type;
}

#include <qlayout.h>
QLayout *rw_recurseFindLayout(QLayout *lo, QWidget *w)
{
	//printf("scanning layout: %p\n", lo);
	QLayoutIterator it = lo->iterator();
	for(QLayoutItem *i; (i = it.current()); ++it) {
		//printf("found: %p,%p\n", i->layout(), i->widget());
		QLayout *slo = i->layout();
		if(slo) {
			QLayout *tlo = rw_recurseFindLayout(slo, w);
			if(tlo)
				return tlo;
		}
		else if(i->widget() == w)
			return lo;
	}
	return 0;
}

QLayout *rw_findLayoutOf(QWidget *w)
{
	return rw_recurseFindLayout(w->parentWidget()->layout(), w);
}

void replaceWidget(QWidget *a, QWidget *b)
{
	if(!a)
		return;

	QLayout *lo = rw_findLayoutOf(a);
	if(!lo)
		return;
	//printf("decided on this: %p\n", lo);

	if(lo->inherits("QBoxLayout")) {
		QBoxLayout *bo = (QBoxLayout *)lo;
		int n = bo->findWidget(a);
		bo->insertWidget(n+1, b);
		delete a;
	}
}

void closeDialogs(QWidget *w)
{
	// close qmessagebox?
	QList<QDialog*> dialogs;
	QObjectList list = w->children();
	for(QObjectList::Iterator it = list.begin() ; it != list.end(); ++it) {
		if((*it)->inherits("QDialog"))
			dialogs.append((QDialog *)(*it));
	}
	for(QList<QDialog*>::Iterator w = dialogs.begin(); w != dialogs.end(); ++w) {
		(*w)->close();
	}
}

QString enc822jid(const QString &s)
{
	QString out;
	for(int n = 0; n < (int)s.length(); ++n) {
		if(s[n] == '\\' || s[n] == '<' || s[n] == '>') {
			QString hex;
			hex.sprintf("\\x%02X", (unsigned char )s[n].toLatin1());
			out.append(hex);
		}
		else
			out += s[n];
	}
	return out;
}

QString dec822jid(const QString &s)
{
	QString out;
	for(int n = 0; n < (int)s.length(); ++n) {
		if(s[n] == '\\' && n + 3 < (int)s.length()) {
			int x = n + 1;
			n += 3;
			if(s[x] != 'x')
				continue;
			ushort val = 0;
			val += hexChar2int(s[x+1].toLatin1())*16;
			val += hexChar2int(s[x+2].toLatin1());
			QChar c(val);
			out += c;
		}
		else
			out += s[n];
	}
	return out;
}

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h> // needed for WM_CLASS hinting

void x11wmClass(Display *dsp, WId wid, QString resName)
{
	char app_name[] = "psi";

	//Display *dsp = x11Display();                 // get the display
	//WId win = winId();                           // get the window
	XClassHint classhint;                          // class hints
	classhint.res_name = (char *)resName.latin1(); // res_name
	classhint.res_class = app_name;                // res_class
	XSetClassHint(dsp, wid, &classhint);           // set the class hints
}

//>>>-- Nathaniel Gray -- Caltech Computer Science ------>
//>>>-- Mojave Project -- http://mojave.cs.caltech.edu -->
// Copied from http://www.nedit.org/archives/discuss/2002-Aug/0386.html

// Helper function
bool getCardinal32Prop(Display *display, Window win, char *propName, long *value)
{
	Atom nameAtom, typeAtom, actual_type_return;
	int actual_format_return, result;
	unsigned long nitems_return, bytes_after_return;
	long *result_array=NULL;

	nameAtom = XInternAtom(display, propName, False);
	typeAtom = XInternAtom(display, "CARDINAL", False);
	if (nameAtom == None || typeAtom == None) {
		//qDebug("Atoms not interned!");
		return false;
	}


	// Try to get the property
	result = XGetWindowProperty(display, win, nameAtom, 0, 1, False,
		typeAtom, &actual_type_return, &actual_format_return,
		&nitems_return, &bytes_after_return,
		(unsigned char **)&result_array);

	if( result != Success ) {
		//qDebug("not Success");
		return false;
	}
	if( actual_type_return == None || actual_format_return == 0 ) {
		//qDebug("Prop not found");
		return false;
	}
	if( actual_type_return != typeAtom ) {
		//qDebug("Wrong type atom");
	}
	*value = result_array[0];
	XFree(result_array);
	return true;
}


// Get the desktop number that a window is on
bool desktopOfWindow(Window *window, long *desktop)
{
	Display *display = QX11Info::display();
	bool result = getCardinal32Prop(display, *window, (char *)"_NET_WM_DESKTOP", desktop);
	//if( result )
	//	qDebug("Desktop: " + QString::number(*desktop));
	return result;
}


// Get the current desktop the WM is displaying
bool currentDesktop(long *desktop)
{
	Window rootWin;
	Display *display = QX11Info::display();
	bool result;

	rootWin = RootWindow(QX11Info::display(), XDefaultScreen(QX11Info::display()));
	result = getCardinal32Prop(display, rootWin, (char *)"_NET_CURRENT_DESKTOP", desktop);
	//if( result )
	//	qDebug("Current Desktop: " + QString::number(*desktop));
	return result;
}
#endif

void bringToFront(QWidget *w, bool)
{
#ifdef Q_WS_X11
	// If we're not on the current desktop, do the hide/show trick
	long dsk, curr_dsk;
	Window win = w->winId();
	if(desktopOfWindow(&win, &dsk) && currentDesktop(&curr_dsk)) {
		if(dsk != curr_dsk) {
			w->hide();
			//qApp->processEvents();
		}
	}

	// FIXME: multi-desktop hacks for Win and Mac required
#endif

	w->show();
	if(w->isMinimized()) {
		//w->hide();
		if(w->isMaximized())
			w->showMaximized();
		else
			w->showNormal();
	}
	//if(grabFocus)
	//	w->setActiveWindow();
	w->raise();
	w->setActiveWindow();
}

bool operator!=(const QMap<QString, QString> &m1, const QMap<QString, QString> &m2)
{
	if ( m1.size() != m2.size() )
		return true;

	QMap<QString, QString>::ConstIterator it = m1.begin(), it2;
	for ( ; it != m1.end(); ++it) {
		it2 = m2.find( it.key() );
		if ( it2 == m2.end() )
			return true;
		if ( it.data() != it2.data() )
			return true;
	}

	return false;
}

