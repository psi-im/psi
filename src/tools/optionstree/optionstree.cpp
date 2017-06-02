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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "optionstree.h"

#include <QDomElement>
#include <QDomDocument>
#include <QStringList>

#include "atomicxmlfile/atomicxmlfile.h"
#include "optionstreereader.h"
#include "optionstreewriter.h"

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
 * \return value of the option. Will be invalid if non-existent.
 */
QVariant OptionsTree::getOption(const QString& name, const QVariant &defaultValue) const
{
	QVariant value=tree_.getValue(name);
	if (value==VariantTree::missingValue) {
		value = defaultValue;
		if (!value.isValid()) {
			qWarning("Accessing missing option %s", qPrintable(name));
		}
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
 * @brief returns true if the node @a node is an internal node.
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

bool OptionsTree::removeOption(const QString &name, bool internal_nodes)
{
	emit optionAboutToBeRemoved(name);
	bool ok = tree_.remove(name, internal_nodes);
	emit optionRemoved(name);
	return ok;
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

bool OptionsTree::isValidName(const QString &name)
{
	foreach(QString part, name.split('.')) {
		if (!VariantTree::isValidNodeName(part)) return false;
	}
	return true;
}


QString OptionsTree::mapLookup(const QString &basename, const QVariant &key) const
{
	QStringList children = getChildOptionNames( basename, true, true);
	foreach (QString path, children) {
		if (getOption(path+".key") == key) {
			return path;
		}
	}
	qWarning("Accessing missing key '%s' in option map '%s'", qPrintable(key.toString()), qPrintable(basename));
	return basename + "XXX";
}

QVariant OptionsTree::mapGet(const QString &basename, const QVariant &key, const QString &node) const {
	return getOption(mapLookup(basename, key) + '.' + node);
}

QVariant OptionsTree::mapGet(const QString &basename, const QVariant &key, const QString &node, const QVariant &def) const {
	QVariantList keys = mapKeyList(basename);
	if (keys.contains(key)) {
		return getOption(mapLookup(basename, key) + '.' + node);
	} else {
		return def;
	}
}


QString OptionsTree::mapPut(const QString &basename, const QVariant &key)
{
	QStringList children = getChildOptionNames( basename, true, true);
	foreach (QString path, children) {
		if (getOption(path+".key") == key) {
			return path;
		}
	}
	// FIXME performance?

	// allocate first unused index
	QString path;
	int i = 0;
	do {
		path = basename+".m"+QString::number(i);
		++i;
	} while (children.contains(path));
	setOption(path + ".key", key);
	return path;
}

void OptionsTree::mapPut(const QString &basename, const QVariant &key, const QString &node, const QVariant &value) {
	setOption(mapPut(basename, key) + '.' + node, value);
}

bool mapKeyListLessThanByNumber(const QString &s1, const QString &s2)
{
	int dotpos = s1.lastIndexOf('.');
	if (s1.leftRef(dotpos+1).compare(s2.leftRef(dotpos+1)) == 0)
	{
		QString name1 = s1.mid(dotpos+1), name2 = s2.mid(dotpos+1);
		if (name1[0] == 'm' && name2[0] == 'm')
		{
			bool ok1 = false, ok2 = false;
			unsigned int n1 = name1.mid(1).toUInt(&ok1), n2 = name2.mid(1).toUInt(&ok2);
			if (ok1 && ok2)
			{
				return n1 < n2;
			}
		}
	}
	//fallback to string comparison
	return s1 < s2;
}

QVariantList OptionsTree::mapKeyList(const QString &basename, bool sortedByNumbers) const
{
	QVariantList ret;
	QStringList children = getChildOptionNames( basename, true, true);
	if (sortedByNumbers)
	{
		qSort(children.begin(), children.end(), mapKeyListLessThanByNumber);
	}
	foreach (QString path, children) {
		ret << getOption(path+".key");
	}
	return ret;
}



/**
 * Saves all options to the specified file
 * \param fileName Name of the file to which to save options
 * \param configName Name of the root element for the file
 * \param configVersion Version of the config format
 * \param configNS Namespace of the config format
 * \return 'true' if the file saves, 'false' if it fails
 */
bool OptionsTree::saveOptions(const QString& fileName, const QString& configName, const QString& configNS, const QString& configVersion, bool streamWriter) const
{
	AtomicXmlFile f(fileName);
	if (streamWriter) {
		OptionsTreeWriter writer(this);
		writer.setName(configName);
		writer.setNameSpace(configNS);
		writer.setVersion(configVersion);
		return f.saveDocument(&writer);
	}
	QDomDocument doc(configName);

	QDomElement base = doc.createElement(configName);
	base.setAttribute("version", configVersion);
	if (!configNS.isEmpty())
		base.setAttribute("xmlns", configNS);
	doc.appendChild(base);

	tree_.toXml(doc, base);
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
bool OptionsTree::loadOptions(const QString& fileName, const QString& configName, const QString& configNS,  const QString& configVersion, bool streamReader)
{
	AtomicXmlFile f(fileName);
	if (streamReader) {
		OptionsTreeReader reader(this);
		return f.loadDocument(&reader);
	}

	QDomDocument doc;
	if (!f.loadDocument(&doc))
		return false;

	return loadOptions(doc.documentElement(), configName, configVersion, configNS);
}


/**
 * Checks for existing saved Options.
 * Does not guarantee that load succeeds if the config file was corrupted.
 */
bool OptionsTree::exists(QString fileName) {
	return AtomicXmlFile::exists(fileName);
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
