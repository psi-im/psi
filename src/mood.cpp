/*
 * mood.cpp
 * Copyright (C) 2006  Remko Troncon
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

#include <QDomDocument>
#include <QDomElement>

#include "mood.h"
#include "moodcatalog.h"

Mood::Mood()
{
	type_ = Unknown;
}

Mood::Mood(Mood::Type type, const QString& text) : type_(type), text_(text)
{
}

Mood::Mood(const QDomElement& e)
{
	fromXml(e);
}

Mood::Type Mood::type() const
{
	return type_;
}

QString Mood::typeText() const
{
	return MoodCatalog::instance()->findEntryByType(type_).text();
}

QString Mood::typeValue() const
{
	return MoodCatalog::instance()->findEntryByType(type_).value();
}

const QString& Mood::text() const
{
	return text_;
}

bool Mood::isNull() const
{
	return type_ == Unknown && text().isEmpty();
}

QDomElement Mood::toXml(QDomDocument& doc)
{
	QDomElement mood = doc.createElement("mood");
	mood.setAttribute("xmlns", PEP_MOOD_NS);

	if (!type() == Unknown) {
		QDomElement el = doc.createElement(MoodCatalog::instance()->findEntryByType(type()).value());
		mood.appendChild(el);
	}

	if (!text().isEmpty()) {
		QDomElement el = doc.createElement("text");
		QDomText t = doc.createTextNode(text());
		el.appendChild(t);
		mood.appendChild(el);
	}

	return mood;
}

void Mood::fromXml(const QDomElement& e)
{
	text_.clear();
	type_ = Unknown;
	if (e.tagName() != "mood")
		return;

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement m = n.toElement();
		if(m.isNull()) {
			continue;
		}
		if (m.tagName() == "text") {
			text_ = m.text();
		}
		else {
			type_ = MoodCatalog::instance()->findEntryByValue(m.tagName()).type();
		}
	}
}
