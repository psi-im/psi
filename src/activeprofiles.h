/*
 * activeprofiles.h - Class for interacting with other psi instances
 * Copyright (C) 2006  Maciej Niedzielski
 * Copyright (C) 2006-2007  Martin Hostettler
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

#ifndef ACTIVEPSIPROFILES_H
#define ACTIVEPSIPROFILES_H

#include <QObject>
#include <QStringList>

class ActiveProfiles: public QObject
{
    Q_OBJECT
public:
    static ActiveProfiles* instance();

    bool setThisProfile(const QString &profile);
    void unsetThisProfile();
    QString thisProfile() const;

    bool isActive(const QString &profile) const;
    bool isAnyActive() const;

    bool setStatus(const QString &profile, const QString &status, const QString &message) const;
    bool openUri(const QString &profile, const QString &uri) const;
    bool raise(const QString &profile, bool withUI) const;

    ~ActiveProfiles();

signals:
    void setStatusRequested(const QString &status, const QString &message);
    void openUriRequested(const QString &uri);
    void raiseRequested();

protected:
    static ActiveProfiles *instance_;

private:
    class Private;
    Private *d;

    ActiveProfiles();

    friend class PsiConAdapter;
    friend class PsiMain;
};

#endif // ACTIVEPSIPROFILES_H
