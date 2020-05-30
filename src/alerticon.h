/*
 * alerticon.h - class for handling animating alert icons
 * Copyright (C) 2003  Michail Pishchagin
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

#ifndef ALERTICON_H
#define ALERTICON_H

#include "iconset.h"

class Impix;
class QIcon;
class QImage;
class QPixmap;
class QString;

class AlertIcon : public PsiIcon {
    Q_OBJECT
public:
    AlertIcon(const PsiIcon *icon);
    ~AlertIcon();

    // reimplemented
    bool           isAnimated() const override;
    QPixmap        pixmap(const QSize &desiredSize = QSize()) const override;
    QImage         image(const QSize &desiredSize = QSize()) const override;
    QIcon          icon() const override;
    const Impix &  impix() const override;
    int            frameNumber() const override;
    const QString &name() const override;

    PsiIcon *copy() const override;

public slots:
    void activated(bool playSound = true) override;
    void stop() override;

public:
    class Private;

private:
    Private *d;
    friend class Private;
};

void alertIconUpdateAlertStyle();

#endif // ALERTICON_H
