/*
 * opt_popups.cpp
 * Copyright (C) 2011  Evgeny Khryukin
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

//#include <QHBoxLayout>
#include <QRadioButton>

#include "opt_popups.h"
#include "ui_opt_popups.h"
#include "psioptions.h"
#include "psicon.h"
#include "popupmanager.h"

class OptPopupsUI : public QWidget, public Ui::OptPopups
{
public:
	OptPopupsUI() : QWidget() { setupUi(this); }
};

OptionsTabPopups::OptionsTabPopups(QObject *parent)
	: OptionsTab(parent, "popups", "", tr("Popups"), tr("The popups behaviour"), "psi/tip")
	, w(0)
	, popup_(0)
{
}

QWidget *OptionsTabPopups::widget()
{
	if ( w )
		return 0;

	w = new OptPopupsUI();
	return w;
}

void OptionsTabPopups::applyOptions()
{
	if ( !w )
		return;

	OptPopupsUI *d = (OptPopupsUI *)w;

	PsiOptions *o = PsiOptions::instance();

	o->setOption("options.ui.notifications.passive-popups.enabled", d->ck_popupOn->isChecked());
	o->setOption("options.ui.notifications.passive-popups.incoming-message", d->ck_popupOnMessage->isChecked());
	o->setOption("options.ui.notifications.passive-popups.showMessage", d->ck_showPopupMessage->isChecked());
	o->setOption("options.ui.notifications.passive-popups.incoming-chat", d->ck_popupOnMessage->isChecked());
	o->setOption("options.ui.notifications.passive-popups.incoming-headline", d->ck_popupOnHeadline->isChecked());
	o->setOption("options.ui.notifications.passive-popups.incoming-file-transfer", d->ck_popupOnFile->isChecked());
	o->setOption("options.ui.notifications.passive-popups.status.online", d->ck_popupOnOnline->isChecked());
	o->setOption("options.ui.notifications.passive-popups.status.offline", d->ck_popupOnOffline->isChecked());
	o->setOption("options.ui.notifications.passive-popups.status.other-changes", d->ck_popupOnStatus->isChecked());
	o->setOption("options.ui.notifications.passive-popups.composing", d->ck_popupComposing->isChecked());
	o->setOption("options.ui.notifications.passive-popups.avatar-size", d->sb_avatar->value());

	o->setOption("options.ui.notifications.passive-popups.maximum-jid-length", QVariant(d->sb_jid->value()));
	o->setOption("options.ui.notifications.passive-popups.maximum-status-length", QVariant(d->sb_status->value()));
	o->setOption("options.ui.notifications.passive-popups.maximum-text-length", QVariant(d->sb_text->value()));
	o->setOption("options.ui.notifications.passive-popups.top-to-bottom",QVariant(d->ck_topToBottom->isChecked()));
	o->setOption("options.ui.notifications.passive-popups.at-left-corner",QVariant(d->ck_atLeft->isChecked()));
	o->setOption("options.ui.notifications.passive-popups.notify-every-muc-message",QVariant(d->ck_everyMucMessage->isChecked()));

	foreach(QObject* obj, d->sa_durations->widget()->children()) {
		QSpinBox *sb = dynamic_cast<QSpinBox*>(obj);
		if(sb) {
			const QString oName = sb->property("name").toString();
			const QString oPath = sb->property("path").toString();
			const int value = sb->value();
			popup_->setValue(oName, value);
			if(!oPath.isEmpty()) {
				PsiOptions::instance()->setOption(oPath, value*1000);
			}
		}
	}

	foreach(QObject* obj, d->gb_type->children()) {
		QRadioButton *rb = dynamic_cast<QRadioButton*>(obj);
		if(rb && rb->isChecked()) {
			o->setOption("options.ui.notifications.typename", rb->objectName());
			break;
		}
	}
}

void OptionsTabPopups::restoreOptions()
{
	if ( !w )
		return;

	OptPopupsUI *d = (OptPopupsUI *)w;

	PsiOptions *o = PsiOptions::instance();

	d->ck_popupOn->setChecked( o->getOption("options.ui.notifications.passive-popups.enabled").toBool() );
	d->ck_popupOnMessage->setChecked( o->getOption("options.ui.notifications.passive-popups.incoming-message").toBool() || o->getOption("options.ui.notifications.passive-popups.incoming-chat").toBool() );
	d->ck_showPopupMessage->setChecked( o->getOption("options.ui.notifications.passive-popups.showMessage").toBool());
	d->ck_popupOnHeadline->setChecked( o->getOption("options.ui.notifications.passive-popups.incoming-headline").toBool() );
	d->ck_popupOnFile->setChecked( o->getOption("options.ui.notifications.passive-popups.incoming-file-transfer").toBool() );
	d->ck_popupOnOnline->setChecked( o->getOption("options.ui.notifications.passive-popups.status.online").toBool() );
	d->ck_popupOnOffline->setChecked( o->getOption("options.ui.notifications.passive-popups.status.offline").toBool() );
	d->ck_popupOnStatus->setChecked( o->getOption("options.ui.notifications.passive-popups.status.other-changes").toBool() );
	d->ck_popupComposing->setChecked( o->getOption("options.ui.notifications.passive-popups.composing").toBool() );
	d->sb_avatar->setValue( o->getOption("options.ui.notifications.passive-popups.avatar-size").toInt() );

	d->sb_jid->setValue(o->getOption("options.ui.notifications.passive-popups.maximum-jid-length").toInt());
	d->sb_status->setValue(o->getOption("options.ui.notifications.passive-popups.maximum-status-length").toInt());
	d->sb_text->setValue(o->getOption("options.ui.notifications.passive-popups.maximum-text-length").toInt());
	d->ck_topToBottom->setChecked(o->getOption("options.ui.notifications.passive-popups.top-to-bottom").toBool());
	d->ck_atLeft->setChecked(o->getOption("options.ui.notifications.passive-popups.at-left-corner").toBool());
	d->ck_everyMucMessage->setChecked(o->getOption("options.ui.notifications.passive-popups.notify-every-muc-message").toBool());

	QWidget *areaWidget = new QWidget;
	QVBoxLayout *vBox = new QVBoxLayout(areaWidget);
	foreach(const QString& option, popup_->optionsNamesList()) {
		QHBoxLayout *l = new QHBoxLayout;
		l->addWidget(new QLabel(option));
		l->addStretch();
		QSpinBox *sb = new QSpinBox();
		sb->setMinimum(-1);
		sb->setValue(popup_->value(option));
		sb->setProperty("path", popup_->optionPath(option));
		sb->setProperty("name", option);
		l->addWidget(sb);
		vBox->addLayout(l);
	}
	d->sa_durations->setWidget(areaWidget);

	delete d->gb_type->layout();
	qDeleteAll(d->gb_type->children());
	QHBoxLayout *l = new QHBoxLayout(d->gb_type);

	foreach(QString type_, popup_->availableTypes()) {
		QRadioButton* rb = new QRadioButton(type_);
		rb->setObjectName(type_);
		d->gb_type->layout()->addWidget(rb);
		l->addWidget(rb);
		if(popup_->currentType() == type_)
			rb->setChecked(true);
	}

	if(l->count() == 1)
		d->gb_type->setVisible(false);

	emit connectDataChanged(w);
}

void OptionsTabPopups::setData(PsiCon *psi, QWidget *)
{
	popup_ = psi->popupManager();
}
