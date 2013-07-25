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
	QDomElement geoloc = doc.createElement("geoloc");
	geoloc.setAttribute("xmlns", "http://jabber.org/protocol/geoloc");

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

	return geoloc;
}

void GeoLocation::fromXml(const QDomElement& e)
{
	if (e.tagName() != "geoloc")
		return;

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement m = n.toElement();
		if (m.tagName() == "alt")
			alt_ = Maybe<float>(m.text().toFloat());
		if (m.tagName() == "bearing")
			bearing_ = Maybe<float>(m.text().toFloat());
		if (m.tagName() == "error")
			error_ = Maybe<float>(m.text().toFloat());
		if (m.tagName() == "lat")
			lat_ = Maybe<float>(m.text().toFloat());
		if (m.tagName() == "lon")
			lon_ = Maybe<float>(m.text().toFloat());
		if (m.tagName() == "datum")
			datum_ = m.text();
		if (m.tagName() == "description")
			description_ = m.text();
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

bool GeoLocation::isNull() const
{
	return !lat_.hasValue() || !lon_.hasValue();
}

bool GeoLocation::operator==(const GeoLocation& o) const
{
	// FIXME
	bool equal = true;
	equal = equal && (lat_.hasValue() ? lat_.value() == o.lat().value() : !o.lat().hasValue());
	equal = equal && (lon_.hasValue() ? lon_.value() == o.lon().value() : !o.lon().hasValue());
	return equal;
}

bool GeoLocation::operator!=(const GeoLocation& o) const
{
	return !((*this) == o);
}
