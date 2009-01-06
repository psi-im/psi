/*
 * parser.cpp - parse an XMPP "document"
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
  TODO:

  For XMPP::Parser to be "perfect", some things must be solved/changed in the
  Qt library:

  - Fix weird QDomElement::haveAttributeNS() bug (patch submitted to
    Trolltech on Aug 31st, 2003).
  - Fix weird behavior in QXmlSimpleReader of reporting endElement() when
    the '/' character of a self-closing tag is reached, instead of when
    the final '>' is reached.
  - Fix incremental parsing bugs in QXmlSimpleReader.  At the moment, the
    only bug I've found is related to attribute parsing, but there might
    be more (search for '###' in $QTDIR/src/xml/qxml.cpp).

  We have workarounds for all of the above problems in the code below.

  - Deal with the <?xml?> processing instruction as an event type, so that we
    can feed it back to the application properly.  Right now it is completely
    untrackable and is simply tacked into the first event's actualString.  We
    can't easily do this because QXmlSimpleReader eats an extra byte beyond
    the processing instruction before reporting it.

  - Make QXmlInputSource capable of accepting data incrementally, to ensure
    proper text encoding detection and processing over a network.  This is
    technically not a bug, as we have our own subclass below to do it, but
    it would be nice if Qt had this already.
*/

#include "parser.h"

#include <qtextcodec.h>
#include <string.h>

using namespace XMPP;

static bool qt_bug_check = false;
static bool qt_bug_have;

//----------------------------------------------------------------------------
// StreamInput
//----------------------------------------------------------------------------
class StreamInput : public QXmlInputSource
{
public:
	StreamInput()
	{
		dec = 0;
		reset();
	}

	~StreamInput()
	{
		delete dec;
	}

	void reset()
	{
		delete dec;
		dec = 0;
		in.resize(0);
		out = "";
		at = 0;
		paused = false;
		mightChangeEncoding = true;
		checkBad = true;
		last = QChar();
		v_encoding = "";
		resetLastData();
	}

	void resetLastData()
	{
		last_string = "";
	}

	QString lastString() const
	{
		return last_string;
	}

	void appendData(const QByteArray &a)
	{
		int oldsize = in.size();
		in.resize(oldsize + a.size());
		memcpy(in.data() + oldsize, a.data(), a.size());
		processBuf();
	}

	QChar lastRead()
	{
		return last;
	}

	QChar next()
	{
		if(paused)
			return EndOfData;
		else
			return readNext();
	}

	// NOTE: setting 'peek' to true allows the same char to be read again,
	//       however this still advances the internal byte processing.
	QChar readNext(bool peek=false)
	{
		QChar c;
		if(mightChangeEncoding)
			c = EndOfData;
		else {
			if(out.isEmpty()) {
				QString s;
				if(!tryExtractPart(&s))
					c = EndOfData;
				else {
					out = s;
					c = out[0];
				}
			}
			else
				c = out[0];
			if(!peek)
				out.remove(0, 1);
		}
		if(c == EndOfData) {
#ifdef XMPP_PARSER_DEBUG
			printf("next() = EOD\n");
#endif
		}
		else {
#ifdef XMPP_PARSER_DEBUG
			printf("next() = [%c]\n", c.latin1());
#endif
			last = c;
		}

		return c;
	}

	QByteArray unprocessed() const
	{
		QByteArray a;
		a.resize(in.size() - at);
		memcpy(a.data(), in.data() + at, a.size());
		return a;
	}

	void pause(bool b)
	{
		paused = b;
	}

	bool isPaused()
	{
		return paused;
	}

	QString encoding() const
	{
		return v_encoding;
	}

private:
	QTextDecoder *dec;
	QByteArray in;
	QString out;
	int at;
	bool paused;
	bool mightChangeEncoding;
	QChar last;
	QString v_encoding;
	QString last_string;
	bool checkBad;

	void processBuf()
	{
#ifdef XMPP_PARSER_DEBUG
		printf("processing.  size=%d, at=%d\n", in.size(), at);
#endif
		if(!dec) {
			QTextCodec *codec = 0;
			uchar *p = (uchar *)in.data() + at;
			int size = in.size() - at;

			// do we have enough information to determine the encoding?
			if(size == 0)
				return;
			bool utf16 = false;
			if(p[0] == 0xfe || p[0] == 0xff) {
				// probably going to be a UTF-16 byte order mark
				if(size < 2)
					return;
				if((p[0] == 0xfe && p[1] == 0xff) || (p[0] == 0xff && p[1] == 0xfe)) {
					// ok it is UTF-16
					utf16 = true;
				}
			}
			if(utf16)
				codec = QTextCodec::codecForMib(1000); // UTF-16
			else
				codec = QTextCodec::codecForMib(106); // UTF-8

			v_encoding = codec->name();
			dec = codec->makeDecoder();

			// for utf16, put in the byte order mark
			if(utf16) {
				out += dec->toUnicode((const char *)p, 2);
				at += 2;
			}
		}

		if(mightChangeEncoding) {
			while(1) {
				int n = out.indexOf('<');
				if(n != -1) {
					// we need a closing bracket
					int n2 = out.indexOf('>', n);
					if(n2 != -1) {
						++n2;
						QString h = out.mid(n, n2-n);
						QString enc = processXmlHeader(h);
						QTextCodec *codec = 0;
						if(!enc.isEmpty())
							codec = QTextCodec::codecForName(enc.toLatin1());

						// changing codecs
						if(codec) {
							v_encoding = codec->name();
							delete dec;
							dec = codec->makeDecoder();
						}
						mightChangeEncoding = false;
						out.truncate(0);
						at = 0;
						resetLastData();
						break;
					}
				}
				QString s;
				if(!tryExtractPart(&s))
					break;
				if(checkBad && checkForBadChars(s)) {
					// go to the parser
					mightChangeEncoding = false;
					out.truncate(0);
					at = 0;
					resetLastData();
					break;
				}
				out += s;
			}
		}
	}

	QString processXmlHeader(const QString &h)
	{
		if(h.left(5) != "<?xml")
			return "";

		int endPos = h.indexOf(">");
		int startPos = h.indexOf("encoding");
		if(startPos < endPos && startPos != -1) {
			QString encoding;
			do {
				startPos++;
				if(startPos > endPos) {
					return "";
				}
			} while(h[startPos] != '"' && h[startPos] != '\'');
			startPos++;
			while(h[startPos] != '"' && h[startPos] != '\'') {
				encoding += h[startPos];
				startPos++;
				if(startPos > endPos) {
					return "";
				}
			}
			return encoding;
		}
		else
			return "";
	}

	bool tryExtractPart(QString *s)
	{
		int size = in.size() - at;
		if(size == 0)
			return false;
		uchar *p = (uchar *)in.data() + at;
		QString nextChars;
		while(1) {
			nextChars = dec->toUnicode((const char *)p, 1);
			++p;
			++at;
			if(!nextChars.isEmpty())
				break;
			if(at == (int)in.size())
				return false;
		}
		last_string += nextChars;
		*s = nextChars;

		// free processed data?
		if(at >= 1024) {
			char *p = in.data();
			int size = in.size() - at;
			memmove(p, p + at, size);
			in.resize(size);
			at = 0;
		}

		return true;
	}

	bool checkForBadChars(const QString &s)
	{
		int len = s.indexOf('<');
		if(len == -1)
			len = s.length();
		else
			checkBad = false;
		for(int n = 0; n < len; ++n) {
			if(!s.at(n).isSpace())
				return true;
		}
		return false;
	}
};


//----------------------------------------------------------------------------
// ParserHandler
//----------------------------------------------------------------------------
namespace XMPP
{
	class ParserHandler : public QXmlDefaultHandler
	{
	public:
		ParserHandler(StreamInput *_in, QDomDocument *_doc)
		{
			in = _in;
			doc = _doc;
			needMore = false;
		}

		~ParserHandler()
		{
			while (!eventList.isEmpty()) {
				delete eventList.takeFirst();
			}
		}

		bool startDocument()
		{
			depth = 0;
			return true;
		}

		bool endDocument()
		{
			return true;
		}

		bool startPrefixMapping(const QString &prefix, const QString &uri)
		{
			if(depth == 0) {
				nsnames += prefix;
				nsvalues += uri;
			}
			return true;
		}

		bool startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &atts)
		{
			if(depth == 0) {
				Parser::Event *e = new Parser::Event;
				QXmlAttributes a;
				for(int n = 0; n < atts.length(); ++n) {
					QString uri = atts.uri(n);
					QString ln = atts.localName(n);
					if(a.index(uri, ln) == -1)
						a.append(atts.qName(n), uri, ln, atts.value(n));
				}
				e->setDocumentOpen(namespaceURI, localName, qName, a, nsnames, nsvalues);
				nsnames.clear();
				nsvalues.clear();
				e->setActualString(in->lastString());

				in->resetLastData();
				eventList.append(e);
				in->pause(true);
			}
			else {
				QDomElement e = doc->createElementNS(namespaceURI, qName);
				for(int n = 0; n < atts.length(); ++n) {
					QString uri = atts.uri(n);
					QString ln = atts.localName(n);
					bool have;
					if(!uri.isEmpty()) {
						have = e.hasAttributeNS(uri, ln);
						if(qt_bug_have)
							have = !have;
					}
					else
						have = e.hasAttribute(ln);
					if(!have)
						e.setAttributeNS(uri, atts.qName(n), atts.value(n));
				}

				if(depth == 1) {
					elem = e;
					current = e;
				}
				else {
					current.appendChild(e);
					current = e;
				}
			}
			++depth;
			return true;
		}

		bool endElement(const QString &namespaceURI, const QString &localName, const QString &qName)
		{
			--depth;
			if(depth == 0) {
				Parser::Event *e = new Parser::Event;
				e->setDocumentClose(namespaceURI, localName, qName);
				e->setActualString(in->lastString());
				in->resetLastData();
				eventList.append(e);
				in->pause(true);
			}
			else {
				// done with a depth 1 element?
				if(depth == 1) {
					Parser::Event *e = new Parser::Event;
					e->setElement(elem);
					e->setActualString(in->lastString());
					in->resetLastData();
					eventList.append(e);
					in->pause(true);

					elem = QDomElement();
					current = QDomElement();
				}
				else
					current = current.parentNode().toElement();
			}

			if(in->lastRead() == '/')
				checkNeedMore();

			return true;
		}

		bool characters(const QString &str)
		{
			if(depth >= 1) {
				QString content = str;
				if(content.isEmpty())
					return true;

				if(!current.isNull()) {
					QDomText text = doc->createTextNode(content);
					current.appendChild(text);
				}
			}
			return true;
		}

		/*bool processingInstruction(const QString &target, const QString &data)
		{
			printf("Processing: [%s], [%s]\n", target.latin1(), data.latin1());
			in->resetLastData();
			return true;
		}*/

		void checkNeedMore()
		{
			// Here we will work around QXmlSimpleReader strangeness and self-closing tags.
			// The problem is that endElement() is called when the '/' is read, not when
			// the final '>' is read.  This is a potential problem when obtaining unprocessed
			// bytes from StreamInput after this event, as the '>' character will end up
			// in the unprocessed chunk.  To work around this, we need to advance StreamInput's
			// internal byte processing, but not the xml character data.  This way, the '>'
			// will get processed and will no longer be in the unprocessed return, but
			// QXmlSimpleReader can still read it.  To do this, we call StreamInput::readNext
			// with 'peek' mode.
			QChar c = in->readNext(true); // peek
			if(c == QXmlInputSource::EndOfData) {
				needMore = true;
			}
			else {
				// We'll assume the next char is a '>'.  If it isn't, then
				// QXmlSimpleReader will deal with that problem on the next
				// parse.  We don't need to take any action here.
				needMore = false;

				// there should have been a pending event
				if (!eventList.isEmpty()) {
					Parser::Event *e = eventList.first();
					e->setActualString(e->actualString() + '>');
					in->resetLastData();
				}
			}
		}

		Parser::Event *takeEvent()
		{
			if(needMore)
				return 0;
			if(eventList.isEmpty())
				return 0;

			Parser::Event *e = eventList.takeFirst();
			in->pause(false);
			return e;
		}

		StreamInput *in;
		QDomDocument *doc;
		int depth;
		QStringList nsnames, nsvalues;
		QDomElement elem, current;
		QList<Parser::Event*> eventList;
		bool needMore;
	};
};


//----------------------------------------------------------------------------
// Event
//----------------------------------------------------------------------------
class Parser::Event::Private
{
public:
	int type;
	QString ns, ln, qn;
	QXmlAttributes a;
	QDomElement e;
	QString str;
	QStringList nsnames, nsvalues;
};

Parser::Event::Event()
{
	d = 0;
}

Parser::Event::Event(const Event &from)
{
	d = 0;
	*this = from;
}

Parser::Event & Parser::Event::operator=(const Event &from)
{
	delete d;
	d = 0;
	if(from.d)
		d = new Private(*from.d);
	return *this;
}

Parser::Event::~Event()
{
	delete d;
}

bool Parser::Event::isNull() const
{
	return (d ? false: true);
}

int Parser::Event::type() const
{
	if(isNull())
		return -1;
	return d->type;
}

QString Parser::Event::nsprefix(const QString &s) const
{
	QStringList::ConstIterator it = d->nsnames.begin();
	QStringList::ConstIterator it2 = d->nsvalues.begin();
	for(; it != d->nsnames.end(); ++it) {
		if((*it) == s)
			return (*it2);
		++it2;
	}
	return QString::null;
}

QString Parser::Event::namespaceURI() const
{
	return d->ns;
}

QString Parser::Event::localName() const
{
	return d->ln;
}

QString Parser::Event::qName() const
{
	return d->qn;
}

QXmlAttributes Parser::Event::atts() const
{
	return d->a;
}

QString Parser::Event::actualString() const
{
	return d->str;
}

QDomElement Parser::Event::element() const
{
	return d->e;
}

void Parser::Event::setDocumentOpen(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &atts, const QStringList &nsnames, const QStringList &nsvalues)
{
	if(!d)
		d = new Private;
	d->type = DocumentOpen;
	d->ns = namespaceURI;
	d->ln = localName;
	d->qn = qName;
	d->a = atts;
	d->nsnames = nsnames;
	d->nsvalues = nsvalues;
}

void Parser::Event::setDocumentClose(const QString &namespaceURI, const QString &localName, const QString &qName)
{
	if(!d)
		d = new Private;
	d->type = DocumentClose;
	d->ns = namespaceURI;
	d->ln = localName;
	d->qn = qName;
}

void Parser::Event::setElement(const QDomElement &elem)
{
	if(!d)
		d = new Private;
	d->type = Element;
	d->e = elem;
}

void Parser::Event::setError()
{
	if(!d)
		d = new Private;
	d->type = Error;
}

void Parser::Event::setActualString(const QString &str)
{
	d->str = str;
}

//----------------------------------------------------------------------------
// Parser
//----------------------------------------------------------------------------
class Parser::Private
{
public:
	Private()
	{
		doc = 0;
		in = 0;
		handler = 0;
		reader = 0;
		reset();
	}

	~Private()
	{
		reset(false);
	}

	void reset(bool create=true)
	{
		delete reader;
		delete handler;
		delete in;
		delete doc;

		if(create) {
			doc = new QDomDocument;
			in = new StreamInput;
			handler = new ParserHandler(in, doc);
			reader = new QXmlSimpleReader;
			reader->setContentHandler(handler);

			// initialize the reader
			in->pause(true);
			reader->parse(in, true);
			in->pause(false);
		}
	}

	QDomDocument *doc;
	StreamInput *in;
	ParserHandler *handler;
	QXmlSimpleReader *reader;
};

Parser::Parser()
{
	d = new Private;

	// check for evil bug in Qt <= 3.2.1
	if(!qt_bug_check) {
		qt_bug_check = true;
		QDomElement e = d->doc->createElementNS("someuri", "somename");
		if(e.hasAttributeNS("someuri", "somename"))
			qt_bug_have = true;
		else
			qt_bug_have = false;
	}
}

Parser::~Parser()
{
	delete d;
}

void Parser::reset()
{
	d->reset();
}

void Parser::appendData(const QByteArray &a)
{
	d->in->appendData(a);

	// if handler was waiting for more, give it a kick
	if(d->handler->needMore)
		d->handler->checkNeedMore();
}

Parser::Event Parser::readNext()
{
	Event e;
	if(d->handler->needMore)
		return e;
	Event *ep = d->handler->takeEvent();
	if(!ep) {
		if(!d->reader->parseContinue()) {
			e.setError();
			return e;
		}
		ep = d->handler->takeEvent();
		if(!ep)
			return e;
	}
	e = *ep;
	delete ep;
	return e;
}

QByteArray Parser::unprocessed() const
{
	return d->in->unprocessed();
}

QString Parser::encoding() const
{
	return d->in->encoding();
}
