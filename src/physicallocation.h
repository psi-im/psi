/*
 * physicallocation.h
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

#ifndef PHYSICALOCATION_H
#define PHYSICALOCATION_H

#include <QString>

class QDomElement;
class QDomDocument;

class PhysicalLocation
{
public:
	PhysicalLocation();
	PhysicalLocation(const QDomElement&);
	
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
	bool isNull() const;

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

	QString toString() const;
	QDomElement toXml(QDomDocument&);
	
	bool operator==(const PhysicalLocation&) const;
	bool operator!=(const PhysicalLocation&) const;
	
protected:
	void fromXml(const QDomElement&);

private:
	QString country_, region_, locality_, area_, street_, building_, floor_, room_, postalcode_, text_;
};

#endif
