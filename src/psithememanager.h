/*
 * psithememanager.h - manages all themes in psi
 * Copyright (C) 2010  Sergey Ilinykh
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

#ifndef PSITHEMEMANAGER_H
#define PSITHEMEMANAGER_H

#include "psithemeprovider.h"

#include <QObject>

//class ChatViewTheme;

class PsiThemeManager : public QObject {
    Q_OBJECT
public:
    PsiThemeManager(QObject *parent);
    ~PsiThemeManager();

    void registerProvider(PsiThemeProvider *provider, bool required = false);
    PsiThemeProvider *unregisterProvider(const QString &type);
    PsiThemeProvider *provider(const QString &type);
    QList<PsiThemeProvider *> registeredProviders() const;
    bool loadAll();

    class Private;
    Private *d;
};

#endif // PSITHEMEMANAGER_H
