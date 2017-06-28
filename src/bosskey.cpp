/*
 * bosskey.cpp
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
#include "bosskey.h"

#include <QApplication>

#include "psioptions.h"
#include "mainwin.h"
#include "psitrayicon.h"

BossKey::BossKey(QObject* p)
	: QObject(p)
	, isHidden_(false)
	, psiOptions(PsiOptions::instance())
	, win_(dynamic_cast<MainWin*>(p))
{
}

void BossKey::shortCutActivated()
{
	if(isHidden_) {
		doShow();
		isHidden_ = false;
	}
	else {
		doHide();
		isHidden_ = true;
	}
}

void BossKey::doHide()
{
	QWidgetList l = qApp->topLevelWidgets();
	foreach(QWidget* w, l) {
		w = w->window();
		if(!w->isHidden()) {
			hiddenWidgets_.push_back(QPointer<QWidget>(w));
			w->hide();
		}
	}
	if(win_) {
		PsiTrayIcon *ico = win_->psiTrayIcon();
		if(ico) {
			ico->hide();
		}
	}

	static const QString soundOption = "options.ui.notifications.sounds.enable";
	static const QString popupOption = "options.ui.notifications.passive-popups.enabled";
	static const QString messageOption = "options.ui.message.auto-popup";
	static const QString headLineOption = "options.ui.message.auto-popup-headlines";
	static const QString chatOption = "options.ui.chat.auto-popup";
	static const QString fileOption = "options.ui.file-transfer.auto-popup";
	static const QStringList opt = QStringList() << soundOption << popupOption
				       << messageOption << headLineOption << chatOption
				       << fileOption;
	foreach(const QString& option, opt) {
		tmpOptions_[option] = psiOptions->getOption(option);
		psiOptions->setOption(option, false);
	}
}

void BossKey::doShow()
{
	foreach(QPointer<QWidget> p, hiddenWidgets_) {
		if(p) {
			p->show();
		}
	}
	hiddenWidgets_.clear();
	if(win_) {
		PsiTrayIcon *ico = win_->psiTrayIcon();
		if(ico) {
			ico->show();
		}
	}
	foreach(const QString& key, tmpOptions_.keys()) {
		psiOptions->setOption(key, tmpOptions_.value(key));
	}
	tmpOptions_.clear();
}
