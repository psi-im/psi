/*
 * geolocationdlg.h
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2009  Evgeny Khryukin
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
 */

#ifndef GEOLOCATIONDLG_H
#define GEOLOCATIONDLG_H

#include <QDialog>

#include "ui_geolocation.h"

class PsiAccount;

class GeoLocationDlg : public QDialog
{
    Q_OBJECT

public:
    GeoLocationDlg(QList<PsiAccount*>);

protected slots:
    void setGeoLocation();
    void reset();

private:
    Ui::GeoLocation ui_;
    QList<PsiAccount*> pa_;
};

#endif // GEOLOCATIONDLG_H
