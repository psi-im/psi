/*
 * chatviewcommon.cpp - shared part of any chatview
 * Copyright (C) 2010 Rion
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QApplication>
#include <QWidget>
#include <QColor>
#include <QRegExp>

#include <math.h>

#include "chatviewcommon.h"
#include "psioptions.h"

void ChatViewCommon::setLooks(QWidget *w)
{
	QPalette pal = w->palette(); // copy widget's palette to non const QPalette
	pal.setColor(QPalette::Inactive, QPalette::HighlightedText,
				 pal.color(QPalette::Active, QPalette::HighlightedText));
	pal.setColor(QPalette::Inactive, QPalette::Highlight,
				 pal.color(QPalette::Active, QPalette::Highlight));
	w->setPalette(pal);        // set the widget's palette
}

bool ChatViewCommon::updateLastMsgTime(QDateTime t)
{
	bool doInsert = t.date() != _lastMsgTime.date();
	_lastMsgTime = t;
	return doInsert;
}

QString ChatViewCommon::getMucNickColor(const QString &nick, bool isSelf, QStringList validList)
{
	const char* rgbBlack = "#000000";
	QString nickwoun = nick; // nick without underscores
	nickwoun.replace(QRegExp("^_*"), "");
	nickwoun.replace(QRegExp("_*$"), "");

	int sender;
	if(isSelf || nickwoun.isEmpty()) {
		sender = -1;
	} else {
		if (!_nicks.contains(nickwoun)) {
			//not found in map
			_nicks.insert(nickwoun,_nickNumber);
			_nickNumber++;
		}
		sender=_nicks[nickwoun];
	}

	QStringList nickColors = validList.isEmpty()
		? PsiOptions::instance()->getOption("options.ui.look.colors.muc.nick-colors").toStringList()
		: validList;

	if(!PsiOptions::instance()->getOption("options.ui.muc.use-nick-coloring").toBool()) {
		return rgbBlack;
	} else {
		if (PsiOptions::instance()->getOption("options.ui.muc.use-hash-nick-coloring").toBool()) {
			/* Hash-driven colors */
			Q_ASSERT(nickwoun.size());
			quint32 hash = qHash(nickwoun); // almost unique hash
			if (_palette.isEmpty()) {
				generatePalette();
			}
			const QColor &precolor = _palette.at(hash % _palette.size());
			return precolor.name();
		} else {
			/* Colors from list */
			if (nickColors.empty()) {
				return rgbBlack;
			}
			if(sender == -1 || nickColors.size() == 1) {
				return nickColors[nickColors.size()-1];
			} else {
				int n = sender % (nickColors.size()-1);
				return nickColors[n];
			}
		}
	}
}

void ChatViewCommon::generatePalette()
{
	QColor bg = qApp->palette().color(QPalette::Base); // background color
	QList<QColor> result;
	QColor color;
	for (int k = 0; k < 10 ; ++k) {
		color = QColor::fromHsv(36*k, 255, 255);
		if (compatibleColors(color, bg)) {
			result << color;
		}
		color = QColor::fromHsv(36*k, 255, 170);
		if (compatibleColors(color, bg)) {
			result << color;
		}
	}
	_palette = result;
}

bool ChatViewCommon::compatibleColors(const QColor &c1, const QColor &c2)
{
	int dR = c1.red()-c2.red();
	int dG = c1.green()-c2.green();
	int dB = c1.blue()-c2.blue();

	double dV = abs(c1.value()-c2.value());
	double dC = sqrt(0.2126*dR*dR + 0.7152*dG*dG + 0.0722*dB*dB);

	if ((dC < 80. && dV > 100) ||
		(dC < 110. && dV <= 100 && dV > 10) ||
		(dC < 125. && dV <= 10)) {
		return false;
	}

	return true;
}

QList<QColor> ChatViewCommon::getPalette()
{
	if (_palette.isEmpty()) {
		generatePalette();
	}
	return _palette;
}

