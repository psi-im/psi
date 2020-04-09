/*
 * moodcatalog.h
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef MOODCATALOG_H
#define MOODCATALOG_H

#include "mood.h"

#include <QList>
#include <QObject>

class QString;

class MoodCatalog : public QObject {
public:
    class Entry {
    public:
        Entry();
        Entry(Mood::Type, const QString &, const QString &);
        Mood::Type     type() const;
        const QString &value() const;
        const QString &text() const;
        bool           isNull() const;
        bool           operator<(const MoodCatalog::Entry &m) const;

    private:
        Mood::Type type_;
        QString    value_;
        QString    text_;
    };

    static MoodCatalog *instance();

    Entry findEntryByType(Mood::Type) const;
    Entry findEntryByValue(const QString &) const;
    Entry findEntryByText(const QString &text) const;

    const QList<Entry> &entries() const;

private:
    MoodCatalog();

    QList<Entry>        entries_;
    static MoodCatalog *instance_;
};

#endif // MOODCATALOG_H
