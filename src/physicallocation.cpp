/*
 * physicallocation.cpp
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
#include <QStringList>

#include "physicallocation.h"

PhysicalLocation::PhysicalLocation()
{
}

PhysicalLocation::PhysicalLocation(const QDomElement& el)
{
	fromXml(el);
}

QDomElement PhysicalLocation::toXml(QDomDocument& doc)
{
	QDomElement physloc = doc.createElement("physloc");
	physloc.setAttribute("xmlns", "http://jabber.org/protocol/physloc");

	if (!country_.isEmpty()) {
		QDomElement e = doc.createElement("country");
		e.appendChild(doc.createTextNode(country_));
		physloc.appendChild(e);
	}
	if (!region_.isEmpty()) {
		QDomElement e = doc.createElement("region");
		e.appendChild(doc.createTextNode(region_));
		physloc.appendChild(e);
	}
	if (!locality_.isEmpty()) {
		QDomElement e = doc.createElement("locality");
		e.appendChild(doc.createTextNode(locality_));
		physloc.appendChild(e);
	}
	if (!area_.isEmpty()) {
		QDomElement e = doc.createElement("area");
		e.appendChild(doc.createTextNode(area_));
		physloc.appendChild(e);
	}
	if (!street_.isEmpty()) {
		QDomElement e = doc.createElement("street");
		e.appendChild(doc.createTextNode(street_));
		physloc.appendChild(e);
	}
	if (!building_.isEmpty()) {
		QDomElement e = doc.createElement("building");
		e.appendChild(doc.createTextNode(building_));
		physloc.appendChild(e);
	}
	if (!floor_.isEmpty()) {
		QDomElement e = doc.createElement("floor");
		e.appendChild(doc.createTextNode(floor_));
		physloc.appendChild(e);
	}
	if (!room_.isEmpty()) {
		QDomElement e = doc.createElement("room");
		e.appendChild(doc.createTextNode(room_));
		physloc.appendChild(e);
	}
	if (!postalcode_.isEmpty()) {
		QDomElement e = doc.createElement("postalcode");
		e.appendChild(doc.createTextNode(postalcode_));
		physloc.appendChild(e);
	}
	if (!text_.isEmpty()) {
		QDomElement e = doc.createElement("text");
		e.appendChild(doc.createTextNode(text_));
		physloc.appendChild(e);
	}

	return physloc;
}

void PhysicalLocation::fromXml(const QDomElement& e)
{
	if (e.tagName() != "physloc")
		return;

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement m = n.toElement();
		if (m.tagName() == "country")
			country_ = m.text();
		else if (m.tagName() == "region")
			region_ = m.text();
		else if (m.tagName() == "locality")
			locality_ = m.text();
		else if (m.tagName() == "area")
			area_ = m.text();
		else if (m.tagName() == "street")
			street_ = m.text();
		else if (m.tagName() == "building")
			building_ = m.text();
		else if (m.tagName() == "floor")
			floor_ = m.text();
		else if (m.tagName() == "room")
			room_ = m.text();
		else if (m.tagName() == "postalcode")
			postalcode_ = m.text();
		else if (m.tagName() == "text")
			text_ = m.text();
	}
}

const QString& PhysicalLocation::country() const
{
	return country_;
}

const QString& PhysicalLocation::region() const
{
	return region_;
}

const QString& PhysicalLocation::locality() const
{
	return locality_;
}

const QString& PhysicalLocation::area() const
{
	return area_;
}

const QString& PhysicalLocation::street() const
{
	return street_;
}

const QString& PhysicalLocation::building() const
{
	return building_;
}

const QString& PhysicalLocation::floor() const
{
	return floor_;
}

const QString& PhysicalLocation::room() const
{
	return room_;
}

const QString& PhysicalLocation::postalcode() const
{
	return postalcode_;
}

const QString& PhysicalLocation::text() const
{
	return text_;
}

bool PhysicalLocation::isNull() const
{
	return country_.isNull() && region_.isNull() && locality_.isNull() && area_.isNull() && street_.isNull() && building_.isNull() && floor_.isNull() && room_.isNull() && postalcode_.isNull() && text_.isNull();
}

bool PhysicalLocation::operator==(const PhysicalLocation& o) const
{
	return country() == o.country() && region() == o.region() && locality() == o.locality() && area() == o.area() && street() == o.street() && building() == o.building() && floor() == o.floor() && room() == o.room() && postalcode() == o.postalcode() && text() == o.text();

}

bool PhysicalLocation::operator!=(const PhysicalLocation& o) const
{
	return !((*this) == o);
}

void PhysicalLocation::setCountry(const QString& s)
{
	country_ = s;
}

void PhysicalLocation::setRegion(const QString& s)
{
	region_ = s;
}

void PhysicalLocation::setLocality(const QString& s)
{
	locality_ = s;
}

void PhysicalLocation::setArea(const QString& s)
{
	area_ = s;
}

void PhysicalLocation::setStreet(const QString& s)
{
	street_ = s;
}

void PhysicalLocation::setBuilding(const QString& s)
{
	building_ = s;
}

void PhysicalLocation::setFloor(const QString& s)
{
	floor_ = s;
}

void PhysicalLocation::setRoom(const QString& s)
{
	room_ = s;
}

void PhysicalLocation::setPostalcode(const QString& s)
{
	postalcode_ = s;
}

void PhysicalLocation::setText(const QString& s)
{
	text_ = s;
}

QString PhysicalLocation::toString() const
{
	QStringList locs;
	QString loc, str;
	if (!locality().isEmpty())
		locs += locality();

	if (!region().isEmpty())
		locs += region();

	if (!country().isEmpty())
		locs += country();

	if (!locs.isEmpty()) {
		loc = locs.join(", ");
	}

	if (!text().isEmpty()) {
		str += text();
		if (!loc.isEmpty())
			str += " (";
	}

	str += loc;

	if (!text().isEmpty() && !loc.isEmpty())
		str += ")";

	return str;
}
