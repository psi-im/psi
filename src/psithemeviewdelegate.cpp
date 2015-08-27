/*
 * psithemeviewdelegate.cpp - renders theme items
 * Copyright (C) 2010 Rion (Sergey Ilinyh)
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psithemeviewdelegate.h"
#include "psithememodel.h"
#include <QPainter>
#include <QFontMetrics>
#include "iconset.h"

// painting
void PsiThemeViewDelegate::paint(QPainter *painter,
				   const QStyleOptionViewItem &option,
				   const QModelIndex &index) const
{
	QPixmap screenshot = index.data(PsiThemeModel::ScreenshotRole).value<QPixmap>();
	QPixmap texture = IconsetFactory::iconPixmap("psi/themeTitleTexture");
	int texturew = texture.width();
	int textureh = texture.height();

	QFont f("Serif");
	f.setBold(true);
	if (screenshot.isNull()) {
		f.setPixelSize(20);
		painter->setFont(f);
		painter->setPen(QColor(170, 170, 170));
		QRect nirect(0, option.rect.top() + textureh, option.rect.width(), 20);
		painter->drawText(nirect, Qt::AlignCenter, tr("No Image"));
	} else {
		QRect sp = screenshot.rect();
		sp.moveTopLeft(option.rect.topLeft());
		painter->drawPixmap(sp, screenshot);
	}

	f.setItalic(true);
	f.setPixelSize(textureh - 8);
	painter->setFont(f);
	QFontMetrics fm(f);
	QString text = index.data(PsiThemeModel::TitleRole).toString();
	QSize textSize = fm.size(Qt::TextSingleLine, text);
	int tw = textSize.width() + 10;
	int y = option.rect.top();
	int vw = option.rect.width();

	painter->drawPixmap(0, y, vw - tw - texturew, textureh, texture.copy(0, 0, 1, textureh).scaled(vw - tw - texturew, textureh));
	painter->drawPixmap(vw - tw - texturew, y, texturew, textureh, texture);
	painter->drawPixmap(vw - tw, y, tw, textureh, texture.copy(texturew - 1, 0, 1, textureh).scaled(tw, textureh));


	painter->setPen(option.state & QStyle::State_Selected? Qt::white : QColor(170, 170, 170));
	QRect txtr(vw - tw, y, tw, textureh-5); // 5 shadow size?
	painter->drawText(txtr, Qt::AlignCenter, text);
}

QSize PsiThemeViewDelegate::sizeHint(const QStyleOptionViewItem &option,
					   const QModelIndex &index) const
{
	int textureh = IconsetFactory::iconPixmap("psi/themeTitleTexture").height();
	QPixmap screenshot = index.data(PsiThemeModel::ScreenshotRole).value<QPixmap>();
	int h = screenshot.isNull()? textureh + 20 : qMax(textureh, screenshot.height());
	return QSize(option.rect.width(), h);
}

// editing
QWidget *PsiThemeViewDelegate::createEditor(QWidget */*parent*/,
							  const QStyleOptionViewItem &/*option*/,
							  const QModelIndex &/*index*/) const
{
	return 0;
}
