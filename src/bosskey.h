/*
 * bosskey.h
 * Copyright (C) 2010  Evgeny Khryukin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <QPointer>
#include <QVariantHash>

class PsiOptions;
class MainWin;

class BossKey : public QObject
{
	Q_OBJECT
public:
	BossKey(QObject *p = 0);
	~BossKey() {};

public slots:
	void shortCutActivated();

private:
	void doHide();
	void doShow();

private:
	bool isHidden_;
	QList< QPointer<QWidget> > hiddenWidgets_;
	QVariantHash tmpOptions_;
	PsiOptions *psiOptions;
	MainWin* win_;
};
