/*
 * alertable.h - simplify alert icon plumbing
 * Copyright (C) 2006  Michail Pishchagin
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

#ifndef ALERTABLE_H
#define ALERTABLE_H

#include <QObject>

class AlertIcon;
class PsiIcon;
class QIcon;

class Alertable : public QObject
{
    Q_OBJECT
public:
    Alertable(QObject* parent = 0);
    ~Alertable();

    bool alerting() const;
    QIcon currentAlertFrame() const;

    void setAlert(const PsiIcon* icon);

private:
    AlertIcon* alert_;
};

#endif
