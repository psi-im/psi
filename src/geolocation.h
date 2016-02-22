/*
 * geolocation.h
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

#ifndef GEOLOCATION_H
#define GEOLOCATION_H

#include <QString>

#include "maybe.h"

#define PEP_GEOLOC_TN "geoloc"
#define PEP_GEOLOC_NS "http://jabber.org/protocol/geoloc"

class QDomElement;
class QDomDocument;

class GeoLocation
{
public:
	GeoLocation();
	GeoLocation(const QDomElement&);

	const Maybe<float>& alt() const;
	const Maybe<float>& bearing() const;
	const Maybe<float>& error() const;
	const Maybe<float>& lat() const;
	const Maybe<float>& lon() const;
	const QString& datum() const;
	const QString& description() const;

	bool isNull() const;

	void setAlt(float);
	void setBearing(float);
	void setError(float);
	void setLat(float);
	void setLon(float);
	void setDatum(const QString&);
	void setDescription(const QString&);

	const QString& country() const;
	const QString& region() const;
	const QString& locality() const;
	const QString& area() const;
	const QString& street() const;
	const QString& building() const;
	const QString& floor() const;
	const QString& room() const;
	const QString& postalcode() const;
	const QString& text() const;

	void setCountry(const QString& s);
 	void setRegion(const QString& s);
	void setLocality(const QString& s);
	void setArea(const QString& s);
	void setStreet(const QString& s);
	void setBuilding(const QString& s);
	void setFloor(const QString& s);
	void setRoom(const QString& s);
	void setPostalcode(const QString& s);
	void setText(const QString& s);

	QDomElement toXml(QDomDocument&);
	QString toString() const;

	bool operator==(const GeoLocation&) const;
	bool operator!=(const GeoLocation&) const;

protected:
	void fromXml(const QDomElement&);

private:
	Maybe<float> alt_, bearing_, error_, lat_, lon_;
	QString datum_, description_;
	QString country_, region_, locality_, area_, street_, building_, floor_, room_, postalcode_, text_;
};

#endif
