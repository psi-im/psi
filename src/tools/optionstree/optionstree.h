/*
 * optionstree.h - Soft-coded options structure header
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

#ifndef OPTIONSTREE_H
#define OPTIONSTREE_H

#include "varianttree.h"

/**
 * \class OptionsTree
 * \brief Dynamic hierachical options structure
 * OptionsTree allows the dynamic creation of options (of type QVariant)
 * and will save and load these to/from xml.
 */
class OptionsTree : public QObject
{
	Q_OBJECT
public:
	OptionsTree(QObject *parent = 0);
	~OptionsTree();

	QVariant getOption(const QString& name, const QVariant &defaultValue = QVariant::Invalid) const;

	// Warning: when inserting Map/Hash be very careful with keys. They are going to become xml element names.
	// full set of supported types can be found in VariantTree::variantToElement()
	void setOption(const QString& name, const QVariant& value);
	bool isInternalNode(const QString &node) const;
	void setComment(const QString& name, const QString& comment);
	QString getComment(const QString& name) const;
	QStringList allOptionNames() const;
	QStringList getChildOptionNames(const QString& = QString(""), bool direct = false, bool internal_nodes = false) const;

	bool removeOption(const QString &name, bool internal_nodes = false);

	static bool isValidName(const QString &name);

	// Map helpers
	QString mapLookup(const QString &basename, const QVariant &key) const;
	QString mapPut(const QString &basename, const QVariant &key);
	void mapPut(const QString &basename, const QVariant &key, const QString &node, const QVariant &value);
	QVariant mapGet(const QString &basename, const QVariant &key, const QString &node) const;
	QVariant mapGet(const QString &basename, const QVariant &key, const QString &node, const QVariant &def) const;
	QVariantList mapKeyList(const QString &basename, bool sortedByNumbers = false) const;


	bool saveOptions(const QString& fileName, const QString& configName, const QString& configNS, const QString& configVersion, bool streamWriter = false) const;
	bool loadOptions(const QString& fileName, const QString& configName, const QString& configNS = "", const QString& configVersion = "", bool streamReader = false);
	bool loadOptions(const QDomElement& name, const QString& configName, const QString& configNS = "", const QString& configVersion = "");
	static bool exists(QString fileName);

signals:
	void optionChanged(const QString& option);
	void optionAboutToBeInserted(const QString& option);
	void optionInserted(const QString& option);
	void optionAboutToBeRemoved(const QString& option);
	void optionRemoved(const QString& option);

private:
	VariantTree tree_;
	friend class OptionsTreeReader;
	friend class OptionsTreeWriter;
};

#endif
