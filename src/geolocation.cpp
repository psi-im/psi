/*
 * geolocation.cpp
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
#include <QApplication>

#include "geolocation.h"

GeoLocation::GeoLocation()
{
}

GeoLocation::GeoLocation(const QDomElement& el)
{
	fromXml(el);
}

QDomElement GeoLocation::toXml(QDomDocument& doc)
{
	QDomElement geoloc = doc.createElement(PEP_GEOLOC_TN);
	geoloc.setAttribute("xmlns", PEP_GEOLOC_NS);

	if (alt_.hasValue()) {
		QDomElement e = doc.createElement("alt");
		e.appendChild(doc.createTextNode(QString::number(alt_.value())));
		geoloc.appendChild(e);
	}
	if (bearing_.hasValue()) {
		QDomElement e = doc.createElement("bearing");
		e.appendChild(doc.createTextNode(QString::number(bearing_.value())));
		geoloc.appendChild(e);
	}
	if (error_.hasValue()) {
		QDomElement e = doc.createElement("error");
		e.appendChild(doc.createTextNode(QString::number(error_.value())));
		geoloc.appendChild(e);
	}
	if (lat_.hasValue()) {
		QDomElement e = doc.createElement("lat");
		e.appendChild(doc.createTextNode(QString::number(lat_.value())));
		geoloc.appendChild(e);
	}
	if (lon_.hasValue()) {
		QDomElement e = doc.createElement("lon");
		e.appendChild(doc.createTextNode(QString::number(lon_.value())));
		geoloc.appendChild(e);
	}
	if (!datum_.isEmpty()) {
		QDomElement e = doc.createElement("datum");
		e.appendChild(doc.createTextNode(datum_));
		geoloc.appendChild(e);
	}
	if (!description_.isEmpty()) {
		QDomElement e = doc.createElement("description");
		e.appendChild(doc.createTextNode(description_));
		geoloc.appendChild(e);
	}
	if (!country_.isEmpty()) {
		QDomElement e = doc.createElement("country");
		e.appendChild(doc.createTextNode(country_));
		geoloc.appendChild(e);
	}
	if (!region_.isEmpty()) {
		QDomElement e = doc.createElement("region");
		e.appendChild(doc.createTextNode(region_));
		geoloc.appendChild(e);
	}
	if (!locality_.isEmpty()) {
		QDomElement e = doc.createElement("locality");
		e.appendChild(doc.createTextNode(locality_));
		geoloc.appendChild(e);
	}
	if (!area_.isEmpty()) {
		QDomElement e = doc.createElement("area");
		e.appendChild(doc.createTextNode(area_));
		geoloc.appendChild(e);
	}
	if (!street_.isEmpty()) {
		QDomElement e = doc.createElement("street");
		e.appendChild(doc.createTextNode(street_));
		geoloc.appendChild(e);
	}
	if (!building_.isEmpty()) {
		QDomElement e = doc.createElement("building");
		e.appendChild(doc.createTextNode(building_));
		geoloc.appendChild(e);
	}
	if (!floor_.isEmpty()) {
		QDomElement e = doc.createElement("floor");
		e.appendChild(doc.createTextNode(floor_));
		geoloc.appendChild(e);
	}
	if (!room_.isEmpty()) {
		QDomElement e = doc.createElement("room");
		e.appendChild(doc.createTextNode(room_));
		geoloc.appendChild(e);
	}
	if (!postalcode_.isEmpty()) {
		QDomElement e = doc.createElement("postalcode");
		e.appendChild(doc.createTextNode(postalcode_));
		geoloc.appendChild(e);
	}
	if (!text_.isEmpty()) {
		QDomElement e = doc.createElement("text");
		e.appendChild(doc.createTextNode(text_));
		geoloc.appendChild(e);
	}

	return geoloc;
}

void GeoLocation::fromXml(const QDomElement& e)
{
	if (e.tagName() != PEP_GEOLOC_TN)
		return;

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement m = n.toElement();
		if (m.isNull()) {
			continue;
		}
		if (m.tagName() == "alt")
			alt_ = Maybe<float>(m.text().toFloat());
		else if (m.tagName() == "bearing")
			bearing_ = Maybe<float>(m.text().toFloat());
		else if (m.tagName() == "error")
			error_ = Maybe<float>(m.text().toFloat());
		else if (m.tagName() == "lat")
			lat_ = Maybe<float>(m.text().toFloat());
		else if (m.tagName() == "lon")
			lon_ = Maybe<float>(m.text().toFloat());
		else if (m.tagName() == "datum")
			datum_ = m.text();
		else if (m.tagName() == "description")
			description_ = m.text();
		else if (m.tagName() == "country")
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

void GeoLocation::setAlt(float alt)
{
	alt_ = Maybe<float>(alt);
}
void GeoLocation::setBearing(float bearing)
{
	bearing_ = Maybe<float>(bearing);
}

void GeoLocation::setError(float error)
{
	error_ = Maybe<float>(error);
}

void GeoLocation::setLat(float lat)
{
	lat_ = Maybe<float>(lat);
}

void GeoLocation::setLon(float lon)
{
	lon_ = Maybe<float>(lon);
}

void GeoLocation::setDatum(const QString& datum)
{
	datum_ = datum;
}

void GeoLocation::setDescription(const QString& description)
{
	description_ = description;
}

const Maybe<float>& GeoLocation::alt() const
{
	return alt_;
}

const Maybe<float>& GeoLocation::bearing() const
{
	return bearing_;
}

const Maybe<float>& GeoLocation::error() const
{
	return error_;
}

const Maybe<float>& GeoLocation::lat() const
{
	return lat_;
}

const Maybe<float>& GeoLocation::lon() const
{
	return lon_;
}

const QString& GeoLocation::datum() const
{
	return datum_;
}

const QString& GeoLocation::description() const
{
	return description_;
}

const QString& GeoLocation::country() const
{
	return country_;
}

const QString& GeoLocation::region() const
{
	return region_;
}

const QString& GeoLocation::locality() const
{
	return locality_;
}

const QString& GeoLocation::area() const
{
	return area_;
}

const QString& GeoLocation::street() const
{
	return street_;
}

const QString& GeoLocation::building() const
{
	return building_;
}

const QString& GeoLocation::floor() const
{
	return floor_;
}

const QString& GeoLocation::room() const
{
	return room_;
}

const QString& GeoLocation::postalcode() const
{
	return postalcode_;
}

const QString& GeoLocation::text() const
{
	return text_;
}

bool GeoLocation::isNull() const
{
	return !lat_.hasValue() && !lon_.hasValue() && !alt_.hasValue() && !bearing_.hasValue() && !error_.hasValue() && country_.isNull() && region_.isNull() && locality_.isNull()
		&& area_.isNull() && street_.isNull() && building_.isNull() && floor_.isNull() && room_.isNull() && postalcode_.isNull() && text_.isNull() && description_.isNull() && datum_.isNull();
}

bool GeoLocation::operator==(const GeoLocation& o) const
{
	// FIXME
	bool equal = true;
	equal = equal && (lat_.hasValue() ? lat_.value() == o.lat().value() : !o.lat().hasValue());
	equal = equal && (lon_.hasValue() ? lon_.value() == o.lon().value() : !o.lon().hasValue());
	equal = equal && (alt_.hasValue() ? alt_.value() == o.alt().value() : !o.alt().hasValue());
	equal = equal && (bearing_.hasValue() ? bearing_.value() == o.bearing().value() : !o.bearing().hasValue());
	equal = equal && (error_.hasValue() ? error_.value() == o.error().value() : !o.error().hasValue());
	equal = equal && country() == o.country() && region() == o.region() && locality() == o.locality()
			&& area() == o.area() && street() == o.street() && datum() == o.datum() && building() == o.building()
			&& floor() == o.floor() && room() == o.room() && postalcode() == o.postalcode() && text() == o.text()
			&& description() == o.description();
	return equal;
}

bool GeoLocation::operator!=(const GeoLocation& o) const
{
	return !((*this) == o);
}

void GeoLocation::setCountry(const QString& s)
{
	country_ = s;
}

void GeoLocation::setRegion(const QString& s)
{
	region_ = s;
}

void GeoLocation::setLocality(const QString& s)
{
	locality_ = s;
}

void GeoLocation::setArea(const QString& s)
{
	area_ = s;
}

void GeoLocation::setStreet(const QString& s)
{
	street_ = s;
}

void GeoLocation::setBuilding(const QString& s)
{
	building_ = s;
}

void GeoLocation::setFloor(const QString& s)
{
	floor_ = s;
}

void GeoLocation::setRoom(const QString& s)
{
	room_ = s;
}

void GeoLocation::setPostalcode(const QString& s)
{
	postalcode_ = s;
}

void GeoLocation::setText(const QString& s)
{
	text_ = s;
}

QString GeoLocation::toString() const
{
	QString loc;

	if(alt_.hasValue() || lon_.hasValue() || lat_.hasValue()) {

		loc += QObject::tr("Latitude/Longitude/Altitude: ");

		if(lat_.hasValue())
			loc += QString::number(lat_.value()) + "/";
		else
			loc += "0/";

		if(lon_.hasValue())
			loc += QString::number(lon_.value()) + "/";
		else
			loc += "0/";

		if(alt_.hasValue())
			loc += QString::number(alt_.value());
		else
			loc += "0";

	}

	if(bearing_.hasValue())
		loc += QObject::tr("\nBearing: ") + QString::number(bearing_.value());

	if(error_.hasValue())
		loc += QObject::tr("\nError: ") + QString::number(error_.value());

	if (!datum().isEmpty())
		loc += QObject::tr("\nDatum: ") + datum();

	if (!country().isEmpty())
		loc += QObject::tr("\nCountry: ") + country();

	if (!postalcode().isEmpty())
		loc += QObject::tr("\nPostalcode: ") + postalcode();

	if (!region().isEmpty())
		loc += QObject::tr("\nRegion: ") + region();

	if (!locality().isEmpty())
		loc += QObject::tr("\nLocality: ") + locality();

	if (!area().isEmpty())
		loc += QObject::tr("\nArea: ") + area();

	if (!street().isEmpty())
		loc += QObject::tr("\nStreet: ") + street();

	if (!building().isEmpty() && (!area().isEmpty() || !street().isEmpty()))
		loc += ", " + building();

	if (!floor().isEmpty())
		loc += QObject::tr("\nFloor: ") + floor();

	if (!room().isEmpty())
		loc += QObject::tr("\nRoom: ") + room();

	if (!description().isEmpty())
		loc += QObject::tr("\nDescription: ") + description();

	if (!text().isEmpty())
		loc += "\n" + text();

	return loc;
}
