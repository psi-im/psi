/*
 * fancypopuplist.h - the list class to hold all the FancyPopups
 * Copyright (C) 2003  Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef POPUPLIST_H
#define POPUPLIST_H

#include "fancypopup.h"

#include <QList>
#include <QObject>

class FancyPopupList : public QObject, public QList<FancyPopup *>
{
    Q_OBJECT

public:
    FancyPopupList()
    {
    }

    ~FancyPopupList()
    {
        while ( !isEmpty() )
            delete takeFirst();
    }

    FancyPopup *last()
    {
        if ( !count() )
            return 0;
        return QList<FancyPopup *>::first();
    }

    void prepend(const FancyPopup *d)
    {
        if ( isEmpty() )
            emit started();

        connect(d, SIGNAL(destroyed(QObject *)), SLOT(popupDestroyed(QObject *)));
        QList<FancyPopup *>::prepend((FancyPopup *)d);
    }

signals:
    void started();
    void finished();

private slots:
    void popupDestroyed(QObject *p)
    {
        removeAll((FancyPopup *)p);

        if ( isEmpty() )
            emit finished();
    }
};

#endif // POPUPLIST_H
