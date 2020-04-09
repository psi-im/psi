/*
 * coloropt.h - Psi color options class
 * Copyright (C) 2011  Sergey Ilinykh
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

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPalette>

class ColorData {
public:
    ColorData() : role(QPalette::NoRole), valid(false) {}
    ColorData(const QColor &color, QPalette::ColorRole role) : color(color), role(role), valid(true) {}

    QColor              color;
    QPalette::ColorRole role;
    bool                valid;
};

class ColorOpt : public QObject {
    Q_OBJECT
public:
    static ColorOpt *   instance();
    QColor              color(const QString &opt, const QColor &defaultColor = QColor()) const;
    QPalette::ColorRole colorRole(const QString &opt) const;

signals:
    void changed(const QString &opt);

private:
    ColorOpt();

public slots:
    static void reset();

private slots:
    void optionChanged(const QString &opt);

private:
    static QScopedPointer<ColorOpt> instance_;
    QHash<QString, ColorData>       colors;
};
