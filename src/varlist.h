/*
 * varlist.h - class for handling a list of string vars
 * Copyright (C) 2001-2002  Justin Karneges
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef VARLIST_H
#define VARLIST_H

#include <QList>
#include <QString>

class OptionsTree;
class QDomDocument;
class QDomElement;
class QStringList;

class VarListItem {
public:
    VarListItem() {}

    const QString &key() const { return v_key; }
    const QString &data() const { return v_data; }

    void setKey(const QString &s) { v_key = s; }
    void setData(const QString &s) { v_data = s; }

private:
    QString v_key;
    QString v_data;
};

class VarList : public QList<VarListItem> {
public:
    VarList();

    void           set(const QString &, const QString &);
    void           unset(const QString &);
    const QString &get(const QString &);

    VarList::Iterator findByKey(const QString &);
    VarList::Iterator findByNum(int);

    QStringList varsToStringList();

    void fromOptions(OptionsTree *o, QString base);
    void toOptions(OptionsTree *o, QString base);

    QDomElement toXml(QDomDocument &doc, const QString &tagName);
    void        fromXml(const QDomElement &);
};

#endif // VARLIST_H
