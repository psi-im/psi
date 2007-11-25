/*
 * optionstree.cpp - Soft-coded options structure implementation
 * Copyright (C) 2006  Kevin Smith
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

#include <QDomElement>
#include <QDomDocument>
#include <QDebug>

#include "optionstree.h"
#include "atomicxmlfile.h"

/**
 * Default constructor
 */
OptionsTree::OptionsTree(QObject *parent)
	: QObject(parent)
{
	
}

/**
 * Destructor
 */
OptionsTree::~OptionsTree()
{
	
}

/**
 * Returns the value of the specified option
 * \param name 'Path' to the option ("appearance.emoticons.useSmilies")
 * \return value of the option. Will be invalid if non-existant.
 */
QVariant OptionsTree::getOption(const QString& name) const
{
	QVariant value=tree_.getValue(name);
	if (value==VariantTree::missingValue) {
		value=QVariant(QVariant::Invalid);
		qDebug() << "Accessing missing option " << name;
	}
	return value;
}

/**
 * \brief Sets the value of the named option.
 * If the option or any parents in the 
 * hierachy do not exist, they are created and optionAboutToBeInserted and
 * optionInserted will be emited.
 *
 * Emits the optionChanged signal if the value differs from the existing value.
 * \param name "Path" to the option
 * \param value Value of the option
 */
void OptionsTree::setOption(const QString& name, const QVariant& value)
{
	const QVariant &prev = tree_.getValue(name);
	if ( prev == value ) {
		return;
	}
	if (!prev.isValid()) {
		emit optionAboutToBeInserted(name);
	}
	tree_.setValue(name, value);
	if (!prev.isValid()) {
		emit optionInserted(name);
	}
	emit optionChanged(name);
}


/**
 * @brief returns true iff the node @a node is an internal node.
 */
bool OptionsTree::isInternalNode(const QString &node) const
{
	return tree_.isInternalNode(node);
}

/**
 * \brief Sets the comment of the named option.
 * \param name "Path" to the option
 * \param comment the comment to store
 */
void OptionsTree::setComment(const QString& name, const QString& comment)
{
	tree_.setComment(name,comment);
}

/**
 * \brief Returns the comment of the specified option.
 * \param name "Path" to the option
 */
QString OptionsTree::getComment(const QString& name) const
{
	return tree_.getComment(name);
}


/**
 * Names of every stored option
 * \return Names of options
 */
QStringList OptionsTree::allOptionNames() const
{
	return tree_.nodeChildren();
}

/**
 * Names of all child options of the given option.
 * \param direct return only the direct children
 * \param internal_nodes include internal (non-final) nodes
 * \return Full names of options
 */
QStringList OptionsTree::getChildOptionNames(const QString& parent, bool direct, bool internal_nodes) const
{
	return tree_.nodeChildren(parent,direct,internal_nodes);
}

/**
 * Saves all options to the specified file
 * \param fileName Name of the file to which to save options
 * \param configName Name of the root element for the file
 * \param configVersion Version of the config format
 * \param configNS Namespace of the config format
 * \return 'true' if the file saves, 'false' if it fails
 */
bool OptionsTree::saveOptions(const QString& fileName, const QString& configName, const QString& configNS, const QString& configVersion) const
{
	QDomDocument doc(configName);

	QDomElement base = doc.createElement(configName);
	base.setAttribute("version", configVersion);
	if (!configNS.isEmpty())
		base.setAttribute("xmlns", configNS);
	doc.appendChild(base);
	
	tree_.toXml(doc, base);
	AtomicXmlFile f(fileName);
	if (!f.saveDocument(doc))
		return false;

	return true;
}

/**
 * Loads all options to from specified file
 * \param fileName Name of the file from which to load options
 * \param configName Name of the root element to check for
 * \param configVersion If specified, the function will fail if the file version doesn't match
 * \param configNS Namespace of the config format
 * \return 'true' if the file loads, 'false' if it fails
 */
bool OptionsTree::loadOptions(const QString& fileName, const QString& configName, const QString& configNS,  const QString& configVersion)
{
	AtomicXmlFile f(fileName);
	QDomDocument doc;
	if (!f.loadDocument(&doc))
		return false;

	return loadOptions(doc.documentElement(), configName, configVersion, configNS);
}

/**
 * Loads all options from an XML element
 * \param base the element to read the options from
 * \param configName Name of the root element to check for
 * \param configNS Namespace of the config format
 * \param configVersion If specified, the function will fail if the file version doesn't match
 */
bool OptionsTree::loadOptions(const QDomElement& base, const QString& configName, const QString& configNS, const QString& configVersion)
{
	Q_UNUSED(configName);
	Q_UNUSED(configNS);
	Q_UNUSED(configVersion);
	
	// Version check
	//if(base.tagName() != configName)
	//	return false;
	//if(configVersion!="" && base.attribute("version") != configVersion)
	//	return false;
	//if(configNS!="" && base.attribute("xmlns")  != configNS)
	//	return false;

	// Convert
	tree_.fromXml(base);
	return true;
}
