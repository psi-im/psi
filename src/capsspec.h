/*
 * capsspec.h
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CAPSSPEC_H
#define CAPSSPEC_H

#include <QList>
#include <QString>

class CapsSpec;
typedef QList<CapsSpec> CapsSpecs;

class CapsSpec
{
	public:
		CapsSpec();
		CapsSpec(const QString&, const QString&, const QString&);
		const QString& node() const; 
		const QString& version() const; 
		const QString& extensions() const; 
		CapsSpecs flatten() const;

		bool operator==(const CapsSpec&) const;
		bool operator!=(const CapsSpec&) const;
		bool operator<(const CapsSpec&) const;
		
	private:
		QString node_, ver_, ext_;
};


#endif
