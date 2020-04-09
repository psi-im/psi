/*
 * iconwidget.h - misc. Iconset- and PsiIcon-aware widgets
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

#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QWidget>

class PsiIcon;

class IconButton : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(QString psiIconName READ psiIconName WRITE setPsiIcon)
    Q_PROPERTY(bool textVisible READ textVisible WRITE setTextVisible)

    Q_OVERRIDE(QPixmap pixmap DESIGNABLE false SCRIPTABLE false)
    Q_OVERRIDE(QIcon icon DESIGNABLE false SCRIPTABLE false)

public:
    IconButton(QWidget *parent = nullptr);
    ~IconButton();

    void setIcon(const QPixmap &);

public slots:
    void    setPsiIcon(const PsiIcon *, bool activate = true);
    void    forceSetPsiIcon(const PsiIcon *, bool activate = true);
    void    setPsiIcon(const QString &);
    QString psiIconName() const;

    void setText(const QString &);

    bool textVisible() const;
    void setTextVisible(bool);

public:
    class Private;

private:
    Private *d;
};

#endif // ICONBUTTON_H
