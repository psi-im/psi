/*
 * capsspec.cpp
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
 
#include <QString>
#include <QStringList>

#include "capsspec.h"

/**
 * \class CapsSpec
 * \brief A class representing an entity capability specification.
 * An entity capability is a combination of a node, a version, and a set of
 * extensions.
 */

/**
 * Default constructor.
 */
CapsSpec::CapsSpec()
{
}


/**
 * \brief Basic constructor.
 * @param node the node
 * @param ven the version
 * @param ext the list of extensions (separated by spaces)
 */
CapsSpec::CapsSpec(const QString& node, const QString& ver, const QString& ext) : node_(node), ver_(ver), ext_(ext) 
{
}


/**
 * \brief Returns the node of the capabilities specification.
 */
const QString& CapsSpec::node() const 
{ 
	return node_; 
}


/**
 * \brief Returns the version of the capabilities specification.
 */
const QString& CapsSpec::version() const 
{ 
	return ver_; 
}


/**
 * \brief Returns the extensions of the capabilities specification.
 */
const QString& CapsSpec::extensions() const 
{ 
	return ext_; 
}


/**
 * \brief Flattens the caps specification into the set of 'simple' 
 * specifications.
 * A 'simple' specification is a specification with exactly one extension,
 * or with the version number as the extension.
 *
 * Example: A caps specification with node=http://psi-im.org, version=0.10,
 * and ext='achat vchat' would be expanded into the following list of specs:
 *	node=http://psi-im.org, ver=0.10, ext=0.10
 *	node=http://psi-im.org, ver=0.10, ext=achat
 *	node=http://psi-im.org, ver=0.10, ext=vchat
 */
CapsSpecs CapsSpec::flatten() const 
{
	CapsSpecs l;
	l.append(CapsSpec(node(),version(),version()));
	QStringList exts(extensions().split(' ',QString::SkipEmptyParts));
	for (QStringList::ConstIterator i = exts.begin(); i != exts.end(); ++i) {
		l.append(CapsSpec(node(),version(),*i));
	}
	return l;
}

bool CapsSpec::operator==(const CapsSpec& s) const 
{
	return (node() == s.node() && version() == s.version() && extensions() == s.extensions());
}

bool CapsSpec::operator!=(const CapsSpec& s) const 
{
	return !((*this) == s);
}

bool CapsSpec::operator<(const CapsSpec& s) const 
{
	return (node() != s.node() ? node() < s.node() :
			(version() != s.version() ? version() < s.version() : 
			 extensions() < s.extensions()));
}

