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

#include "optionstree.h"


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
QVariant OptionsTree::getOption(const QString& name)
{
	QVariant value=tree_.getValue(name);
	if (value==VariantTree::missingValue) {
		value=QVariant(QVariant::Invalid);
		qDebug(qPrintable(QString("Accessing missing option %1").arg(name)));
	}
	return value;
}

/**
 * Sets the value of the named option. If the option or any parents in the 
 * hierachy do not exist, they are created. Emits the optionChanged signal
 * if the value differs from the existing value.
 * \param name "Path" to the option
 * \param value Value of the option
 */
void OptionsTree::setOption(const QString& name, const QVariant& value)
{
	if ( tree_.getValue(name) == value )
		return;
	tree_.setValue(name, value);
	emit optionChanged(name);
}

/**
 * TODO
 */
void OptionsTree::setComment(const QString& name, const QString& comment)
{
	tree_.setComment(name,comment);
}

/**
 * TODO
 */
QString OptionsTree::getComment(const QString& name)
{
	return tree_.getComment(name);
}


/**
 * Names of every stored option
 * \return Names of options
 */
QStringList OptionsTree::allOptionNames()
{
	return tree_.nodeChildren();
}

/**
 * Names of all child options of the given option.
 * \param direct return only the direct children
 * \return Names of options
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
bool OptionsTree::saveOptions(const QString& fileName, const QString& configName, const QString& configNS, const QString& configVersion)
{
	QDomDocument doc(configName);

	QDomElement base = doc.createElement(configName);
	base.setAttribute("version", configVersion);
	if (!configNS.isEmpty())
		base.setAttribute("xmlns", configNS);
	doc.appendChild(base);
	
	tree_.toXml(doc, base);
	QFile file(fileName);
	if(!file.open(QIODevice::WriteOnly))
	{
		//qWarning(qPrintable(tr(QString("Could not open options file for saving, %1").arg(fileName))));
		return false;
	}
	QTextStream text;
	text.setDevice(&file);
	text.setCodec("UTF-8");
	text << doc.toString();
	file.close();

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
	// Open the file
	QFile file(fileName);
	if(!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	
	// Load document from file
	QDomDocument doc;
	if(!doc.setContent(&file))
		return false;
	file.close();

	return loadOptions(doc.documentElement(), configName, configVersion, configNS);
}

/**
 * Loads all options from an XML element
 * \param element the element to read the options from
 * \param configName Name of the root element to check for
 * \param configVersion If specified, the function will fail if the file version doesn't match
 */
bool OptionsTree::loadOptions(const QDomElement& base, const QString& configName, const QString& configNS, const QString& configVersion)
{
#ifdef __GNUC__
#warning "Temporarily commenting out options.xml version checks"
#endif
	Q_UNUSED(configName);
	Q_UNUSED(configNS);
	Q_UNUSED(configVersion);
	
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
