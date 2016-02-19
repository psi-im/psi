/*
 * activity.cpp
 * Copyright (C) 2008 Armando Jagucki
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

#include <QDomDocument>
#include <QDomElement>

#include "activity.h"
#include "activitycatalog.h"

Activity::Activity()
{
	type_ = Unknown;
	specificType_ = UnknownSpecific;
}

Activity::Activity(Activity::Type type, Activity::SpecificType specificType, const QString& text)
{
	type_ = type;
	specificType_ = specificType;
	text_ = text;
}

Activity::Activity(const QDomElement& e)
{
	fromXml(e);
}

Activity::Type Activity::type() const
{
	return type_;
}

QString Activity::typeText() const
{
	return ActivityCatalog::instance()->findEntryByType(type_).text();
}

Activity::SpecificType Activity::specificType() const
{
	return specificType_;
}

QString Activity::specificTypeText() const
{
	return ActivityCatalog::instance()->findEntryByType(specificType_).text();
}

const QString& Activity::text() const
{
	return text_;
}

QString Activity::typeValue() const
{
	return ActivityCatalog::instance()->findEntryByType(type_).value();
}

QString Activity::specificTypeValue() const
{
	return ActivityCatalog::instance()->findEntryByType(specificType_).value();
}

bool Activity::isNull() const
{
	return type_ == Unknown && text().isEmpty();
}

QDomElement Activity::toXml(QDomDocument& doc)
{
	QDomElement activity = doc.createElement("activity");
	activity.setAttribute("xmlns", "http://jabber.org/protocol/activity");

	if (!type() == Unknown) {
		ActivityCatalog* ac = ActivityCatalog::instance();
		QDomElement el = doc.createElement(ac->findEntryByType(type()).value());

		if (!specificType() == UnknownSpecific) {
			QDomElement elChild = doc.createElement(ac->findEntryByType(specificType()).value());
			el.appendChild(elChild);
		}

		activity.appendChild(el);
	}
	else {
		qWarning("activity.cpp: ERROR: Converting unknown activity");
	}

	if (!text().isEmpty()) {
		QDomElement el = doc.createElement("text");
		QDomText t = doc.createTextNode(text());
		el.appendChild(t);
		activity.appendChild(el);
	}

	return activity;
}

void Activity::fromXml(const QDomElement& element)
{
	if (element.tagName() != "activity")
		return;

	type_ = Activity::Unknown;
	specificType_ = Activity::UnknownSpecific;

	for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
		QDomElement child = node.toElement();
		if (child.tagName() == "text") {
			text_ = child.text();
		}
		else {
			ActivityCatalog* ac = ActivityCatalog::instance();
			type_ = ac->findEntryByValue(child.tagName()).type();

			if (child.hasChildNodes()) {
				QDomElement specificTypeElement = child.firstChildElement();
				if (!specificTypeElement.isNull()) {
					specificType_ = ac->findEntryByValue(specificTypeElement.tagName()).specificType();
				}
			}
		}
	}
}
