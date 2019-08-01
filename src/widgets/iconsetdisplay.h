/*
 * iconsetdisplay.h - contact list widget
 * Copyright (C) 2001-2005  Justin Karneges, Michail Pishchagin
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

#ifndef ICONSETDISPLAY_H
#define ICONSETDISPLAY_H

#include <QListWidget>

class Iconset;
class IconsetDisplayItem;

class IconsetDisplay : public QListWidget
{
    Q_OBJECT
public:
    IconsetDisplay(QWidget *parent = nullptr);
    ~IconsetDisplay();

    void setIconset(const Iconset &);
private:
    friend class IconsetDisplayItem;
};

#endif // ICONSETDISPLAY_H
