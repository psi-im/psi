/*
 * psicontactlistviewdelegate.cpp
 * Copyright (C) 2009-2010  Yandex LLC (Michail Pishchagin)
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

#include "psicontactlistviewdelegate.h"

#include <QPainter>
#include <QTimer>

#include "psiiconset.h"
#include "psioptions.h"
#include "coloropt.h"
#include "contactlistview.h"
#include "common.h"
#include "avatars.h"
#include "mood.h"
#include "activity.h"

static const QString contactListFontOptionPath = "options.ui.look.font.contactlist";
static const QString slimGroupsOptionPath = "options.ui.look.contactlist.use-slim-group-headings";
static const QString outlinedGroupsOptionPath = "options.ui.look.contactlist.use-outlined-group-headings";
static const QString contactListBackgroundOptionPath = "options.ui.look.colors.contactlist.background";
static const QString showStatusMessagesOptionPath = "options.ui.contactlist.status-messages.show";
static const QString statusSingleOptionPath = "options.ui.contactlist.status-messages.single-line";
static const QString showClientIconsPath = "options.ui.contactlist.show-client-icons";
static const QString showMoodIconsPath = "options.ui.contactlist.show-mood-icons";
static const QString showGeolocIconsPath = "options.ui.contactlist.show-geolocation-icons";
static const QString showActivityIconsPath = "options.ui.contactlist.show-activity-icons";
static const QString showTuneIconsPath = "options.ui.contactlist.show-tune-icons";
static const QString avatarSizeOptionPath = "options.ui.contactlist.avatars.size";
static const QString avatarRadiusOptionPath = "options.ui.contactlist.avatars.radius";
static const QString showAvatarsPath = "options.ui.contactlist.avatars.show";
static const QString useDefaultAvatarPath = "options.ui.contactlist.avatars.use-default-avatar";
static const QString avatarAtLeftOptionPath = "options.ui.contactlist.avatars.avatars-at-left";
static const QString showStatusIconsPath = "options.ui.contactlist.show-status-icons";
static const QString statusIconsOverAvatarsPath = "options.ui.contactlist.status-icon-over-avatar";
static const QString allClientsOptionPath = "options.ui.contactlist.show-all-client-icons";
static const QString enableGroupsOptionPath = "options.ui.contactlist.enable-groups";
static const QString statusIconsetOptionPath = "options.iconsets.status";

PsiContactListViewDelegate::PsiContactListViewDelegate(ContactListView* parent)
	: ContactListViewDelegate(parent)
	, font_(0)
	, fontMetrics_(0)
{
	alertTimer_ = new QTimer(this);
	alertTimer_->setInterval(100);
	alertTimer_->setSingleShot(false);
	connect(alertTimer_, SIGNAL(timeout()), SLOT(updateAlerts()));

	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	connect(PsiIconset::instance(), SIGNAL(rosterIconsSizeChanged(int)), SLOT(rosterIconsSizeChanged(int)));
	statusIconSize_ = PsiIconset::instance()->roster.value(PsiOptions::instance()->getOption(statusIconsetOptionPath).toString())->iconSize();
	optionChanged(slimGroupsOptionPath);
	optionChanged(outlinedGroupsOptionPath);
	optionChanged(contactListFontOptionPath);
	optionChanged(contactListBackgroundOptionPath);
	optionChanged(showStatusMessagesOptionPath);
	optionChanged(statusSingleOptionPath);
	optionChanged(showClientIconsPath);
	optionChanged(showMoodIconsPath);
	optionChanged(showGeolocIconsPath);
	optionChanged(showActivityIconsPath);
	optionChanged(showTuneIconsPath);
	optionChanged(avatarSizeOptionPath);
	optionChanged(avatarRadiusOptionPath);
	optionChanged(showAvatarsPath);
	optionChanged(useDefaultAvatarPath);
	optionChanged(avatarAtLeftOptionPath);
	optionChanged(showStatusIconsPath);
	optionChanged(statusIconsOverAvatarsPath);
	optionChanged(allClientsOptionPath);
	optionChanged(enableGroupsOptionPath);
}

PsiContactListViewDelegate::~PsiContactListViewDelegate()
{
	delete font_;
	delete fontMetrics_;
}

int PsiContactListViewDelegate::avatarSize() const
{
	return showAvatars_ ?
		qMax(avatarSize_ + 2, rowHeight_) : rowHeight_;
}

QPixmap PsiContactListViewDelegate::statusPixmap(const QModelIndex& index) const
{
	int s = statusType(index);
	ContactListModel::Type type = ContactListModel::indexType(index);
	if (type == ContactListModel::ContactType ||
	    type == ContactListModel::AccountType)
	{
		if (index.data(ContactListModel::IsAlertingRole).toBool()) {
			if (!alertingIndexes_.contains(index)) {
				alertingIndexes_[index] = true;
				alertTimer_->start();
			}

			QVariant alertData = index.data(ContactListModel::AlertPictureRole);
			QIcon alert;
			if (alertData.isValid()) {
				if (alertData.type() == QVariant::Icon) {
					alert = qvariant_cast<QIcon>(alertData);
				}
			}

			return alert.pixmap(100, 100);
		}
	}

	if (!showStatusIcons_)
		return QPixmap();

	if (type == ContactListModel::ContactType) {
		if (!index.data(ContactListModel::PresenceErrorRole).toString().isEmpty())
			s = STATUS_ERROR;
		else if (index.data(ContactListModel::IsAgentRole).toBool())
			s = statusType(index);
		else if (index.data(ContactListModel::AskingForAuthRole).toBool() && s == XMPP::Status::Offline)
			s = STATUS_ASK;
		else if (!index.data(ContactListModel::AuthorizesToSeeStatusRole).toBool() && s == XMPP::Status::Offline)
			s = STATUS_NOAUTH;
	}

	return PsiIconset::instance()->statusPtr(index.data(ContactListModel::JidRole).toString(), s)->pixmap();
}

QList<QPixmap> PsiContactListViewDelegate::clientPixmap(const QModelIndex& index) const
{
	QList<QPixmap> pixList;
	ContactListModel::Type type = ContactListModel::indexType(index);
	if (type != ContactListModel::ContactType)
		return pixList;

	QStringList vList = index.data(ContactListModel::ClientRole).toStringList();
	if(vList.isEmpty())
		return pixList;

	foreach(QString client, vList) {
		const QPixmap &pix = IconsetFactory::iconPixmap("clients/" + client);
		if(!pix.isNull())
			pixList.push_back(pix);
	}

	return pixList;
}

QPixmap PsiContactListViewDelegate::avatarIcon(const QModelIndex& index) const
{
	int avSize = showAvatars_ ? avatarSize_ : 0;
	QPixmap av = index.data(ContactListModel::IsMucRole).toBool() ? QPixmap() : index.data(ContactListModel::AvatarRole).value<QPixmap>();
	if(av.isNull() && useDefaultAvatar_)
		av = IconsetFactory::iconPixmap("psi/default_avatar");

	return AvatarFactory::roundedAvatar(av, avatarRadius_, avSize);
}

QSize PsiContactListViewDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
	if (index.isValid()) {
		if(index.data(ContactListModel::TypeRole) == ContactListModel::ContactType) {
			if(!statusSingle_ || !showStatusMessages_)
				return QSize(16, avatarSize());
			else
				return QSize(16, qMax(avatarSize(), rowHeight_*3/2));
		} else {
			return QSize(16, rowHeight_);
		}
	}

	return QSize(0, 0);
}

static QRect relativeRect(const QStyleOption& option,
						  const QSize &size,
						  const QRect& prevRect,
						  int padding = 0)
{
	QRect r = option.rect;
	const bool isRTL = option.direction == Qt::RightToLeft;
	if (isRTL) {
		if (prevRect.isValid())
			r.setRight(prevRect.left() - padding);
		if (size.isValid()) {
			r.setLeft(r.right() - size.width() + 1);
			r.setBottom(r.top() + size.height() - 1);
			r.translate(-1, 1);
		}
	} else {
		if (prevRect.isValid())
			r.setLeft(prevRect.right() + padding);
		if (size.isValid()) {
			r.setSize(size);
			r.translate(1, 1);
		}
	}
	return r;
}

void PsiContactListViewDelegate::drawContact(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	drawBackground(painter, option, index);

	QRect r = option.rect;

	QRect avatarRect(r);
	if(showAvatars_) {
		const QPixmap avatarPixmap = avatarIcon(index);
		int size = avatarSize_;
		avatarRect.setSize(QSize(size,size));
		if(avatarAtLeft_) {
			avatarRect.translate(enableGroups_ ? -5:-1, 1);
			r.setLeft(avatarRect.right() + 3);
		}
		else {
			avatarRect.moveTopRight(r.topRight());
			avatarRect.translate(-1,1);
			r.setRight(avatarRect.left() - 3);
		}
		int row = (statusSingle_ && showStatusMessages_) ? rowHeight_*3/2 : rowHeight_; // height required for nick
		int h = (size - row)/2; // padding from top to center it
		if(h > 0) {
			r.setTop(r.top() + h);
			r.setHeight(row);
		}
		else {
			avatarRect.setTop(avatarRect.top() - h);
		}

		if(!avatarPixmap.isNull()) {
			painter->drawPixmap(avatarRect.topLeft(), avatarPixmap);
		}
	}

	QRect statusRect(r);
	QPixmap statusPixmap = this->statusPixmap(index);
	if(!statusPixmap.isNull()) {
		if(statusIconsOverAvatars_ && showAvatars_) {
			statusPixmap = statusPixmap.scaled(12,12, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			statusRect.setSize(statusPixmap.size());
			statusRect.moveBottomRight(avatarRect.bottomRight());
			statusRect.translate(-1,-2);
			r.setLeft(r.left() + 3);
		} else {
			statusRect.setSize(statusPixmap.size());
			statusRect.translate(0, 1);
			if (option.direction == Qt::RightToLeft) {
				statusRect.setRight(r.right() - 1);
				r.setRight(statusRect.right() - 3);
			} else {
				statusRect.setLeft(r.left() + 1);
				r.setLeft(statusRect.right() + 3);
			}
		}
		painter->drawPixmap(statusRect.topLeft(), statusPixmap);
	} else
		r.setLeft(r.left() + 3);

	QColor textColor;
	if(index.data(ContactListModel::IsAnimRole).toBool()) {
		if(index.data(ContactListModel::PhaseRole).toBool()) {
			textColor = ColorOpt::instance()->color("options.ui.look.colors.contactlist.status-change-animation2");
		}
		else {
			textColor = ColorOpt::instance()->color("options.ui.look.colors.contactlist.status-change-animation1");
		}
	}
	else {
		if (statusType(index) == XMPP::Status::Away || statusType(index) == XMPP::Status::XA)
			textColor = ColorOpt::instance()->color("options.ui.look.colors.contactlist.status.away");
		else if (statusType(index) == XMPP::Status::DND)
			textColor = ColorOpt::instance()->color("options.ui.look.colors.contactlist.status.do-not-disturb");
		else if (statusType(index) == XMPP::Status::Offline)
			textColor = ColorOpt::instance()->color("options.ui.look.colors.contactlist.status.offline");
		else
			textColor = ColorOpt::instance()->color("options.ui.look.colors.contactlist.status.online");
	}

	QStyleOptionViewItemV2 o = option;
	o.font = *font_;
	o.font.setItalic(index.data(ContactListModel::BlockRole).toBool());
	o.fontMetrics = *fontMetrics_;
	QPalette palette = o.palette;
	palette.setColor(QPalette::Text, textColor);
	o.palette = palette;

	QString text = nameText(o, index);
	if (showStatusMessages_ && !statusText(index).isEmpty()) {
		if(!statusSingle_) {
			text = tr("%1 (%2)").arg(text).arg(statusText(index));
			drawText(painter, o, r, text, index);
		}
		else {
			QRect txtRect(r);
			txtRect.setHeight(r.height()*2/3);
			drawText(painter, o, txtRect, text, index);
			QString statusMsg = statusText(index);
			palette.setColor(QPalette::Text, ColorOpt::instance()->color("options.ui.look.colors.contactlist.status-messages"));
			o.palette = palette;
			txtRect.moveTopRight(txtRect.bottomRight());
			txtRect.setHeight(r.height() - txtRect.height());
			o.font.setPointSize(qMax(o.font.pointSize()-2, 7));
			o.fontMetrics = QFontMetrics(o.font);
			painter->save();
			drawText(painter, o, txtRect, statusMsg, index);
			painter->restore();
		}
	}
	else {
		if(showStatusMessages_ && statusSingle_)
			r.setHeight(r.height()*2/3);

		drawText(painter, o, r, text, index);
	}

	bool isMuc = index.data(ContactListModel::IsMucRole).toBool();
	QString mucMessages;
	if(isMuc)
		mucMessages = index.data(ContactListModel::MucMessagesRole).toString();

	QRect iconRect(r);
	QList<QPixmap> rightPixs;
	QList<int> rightWidths;
	if(!isMuc) {
		if (showClientIcons_) {
			const QList<QPixmap> pixList = this->clientPixmap(index);

			for (QList<QPixmap>::ConstIterator it = pixList.begin(); it != pixList.end(); ++it) {
				const QPixmap &pix = *it;
				rightPixs.push_back(pix);
				rightWidths.push_back(pix.width());
				if(!allClients_)
					break;
			}
		}

		if (showMoodIcons_ && !index.data(ContactListModel::MoodRole).isNull()) {
			QVariant v = index.data(ContactListModel::MoodRole);
			if (!v.isNull()) {
				Mood m = v.value<Mood>();
				if (m.type() != Mood::Unknown) {
					const QPixmap &pix = IconsetFactory::iconPixmap(QString("mood/%1").arg(m.typeValue()));
					if(!pix.isNull()) {
						rightPixs.push_back(pix);
						rightWidths.push_back(pix.width());
					}
				}
			}
		}

		if (showActivityIcons_ && !index.data(ContactListModel::ActivityRole).isNull()) {
			QVariant v = index.data(ContactListModel::ActivityRole);
			if (!v.isNull()) {
				const QPixmap &pix = IconsetFactory::iconPixmap(activityIconName(v.value<Activity>()));
				if(!pix.isNull()) {
					rightPixs.push_back(pix);
					rightWidths.push_back(pix.width());
				}
			}
		}

		if (showTuneIcons_ && index.data(ContactListModel::TuneRole).toBool()) {
			const QPixmap &pix = IconsetFactory::iconPixmap("pep/tune");
			rightPixs.push_back(pix);
			rightWidths.push_back(pix.width());
		}

		if (showGeolocIcons_ && index.data(ContactListModel::GeolocationRole).toBool()) {
			const QPixmap &pix = IconsetFactory::iconPixmap("pep/geolocation");
			rightPixs.push_back(pix);
			rightWidths.push_back(pix.width());
		}

		if (index.data(ContactListModel::IsSecureRole).toBool()) {
			const QPixmap &pix = IconsetFactory::iconPixmap("psi/pgp");
			rightPixs.push_back(pix);
			rightWidths.push_back(pix.width());
		}
	}

	if(rightPixs.isEmpty() && mucMessages.isEmpty())
		return;

	int sumWidth = 0;
	if(isMuc)
		sumWidth = fontMetrics_->width(mucMessages);
	else {
		foreach (int w, rightWidths) {
			sumWidth += w;
		}
		sumWidth+=rightPixs.count();
	}
	QColor bgc = (option.state & QStyle::State_Selected) ? palette.color(QPalette::Highlight) : palette.color(QPalette::Base);
	QColor tbgc = bgc;
	tbgc.setAlpha(0);
	QLinearGradient grad(r.right() - sumWidth - 20, 0, r.right() - sumWidth, 0);
	grad.setColorAt(0, tbgc);
	grad.setColorAt(1, bgc);
	QBrush tbakBr(grad);
	QRect gradRect(r);
	gradRect.setLeft(gradRect.right() - sumWidth - 20);
	painter->fillRect(gradRect, tbakBr);

	if(isMuc) {
		iconRect.setLeft(iconRect.right() - sumWidth - 1);
		painter->drawText(iconRect, mucMessages);
	}
	else {
		for (int i=0; i<rightPixs.size(); i++) {
			const QPixmap pix = rightPixs[i];
			iconRect.setRight(iconRect.right() - pix.width() -1);
			painter->drawPixmap(iconRect.topRight(), pix);
		}
	}
}

void PsiContactListViewDelegate::drawGroup(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItemV2 o = option;
	o.font = *font_;
	o.fontMetrics = *fontMetrics_;
	QPalette palette = o.palette;
	QColor background = ColorOpt::instance()->color("options.ui.look.colors.contactlist.grouping.header-background");
	QColor foreground = ColorOpt::instance()->color("options.ui.look.colors.contactlist.grouping.header-foreground");
	if (!slimGroup_)
		palette.setColor(QPalette::Base, background);
	palette.setColor(QPalette::Text, foreground);
	o.palette = palette;

	drawBackground(painter, o, index);

	QRect r = option.rect;
	if (!slimGroup_ && outlinedGroup_) {
		painter->setPen(QPen(foreground));
		QRect gr(r);
		gr.setLeft(contactList()->x());
		painter->drawRect(gr);
	}

	const QPixmap& pixmap = index.data(ContactListModel::ExpandedRole).toBool() ?
	                        IconsetFactory::iconPtr("psi/groupOpen")->pixmap() :
	                        IconsetFactory::iconPtr("psi/groupClosed")->pixmap();

	const QSize pixmapSize = pixmap.size();
	QRect pixmapRect = relativeRect(option, pixmapSize, QRect());
	r = relativeRect(option, QSize(), pixmapRect, 3);
	painter->drawPixmap(pixmapRect.topLeft(), pixmap);

	QString text = index.data(ContactListModel::DisplayGroupRole).toString();
	drawText(painter, o, r, text, index);

	if(slimGroup_ && !(option.state & QStyle::State_Selected)) {
		int h = r.y() + (r.height() / 2);
		int x = r.left() + fontMetrics_->width(text) + 8;
		painter->setPen(QPen(background,2));
		painter->drawLine(x, h, r.right(), h);
	}
}

void PsiContactListViewDelegate::drawAccount(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItemV2 o = option;
	o.font = *font_;
	o.fontMetrics = *fontMetrics_;
	QPalette palette = o.palette;
	palette.setColor(QPalette::Base, ColorOpt::instance()->color("options.ui.look.colors.contactlist.profile.header-background"));
	QColor foreground = ColorOpt::instance()->color("options.ui.look.colors.contactlist.profile.header-foreground");
	palette.setColor(QPalette::Text, foreground);
	o.palette = palette;

	drawBackground(painter, o, index);

	if (outlinedGroup_) {
		painter->setPen(QPen(foreground));
		painter->drawRect(option.rect);
	}

	const QPixmap statusPixmap = this->statusPixmap(index);
	const QSize pixmapSize = statusPixmap.size();
	const QRect avatarRect = relativeRect(o, pixmapSize, QRect());
	QString text = nameText(o, index);
	QRect r = relativeRect(o, QSize(o.fontMetrics.width(text), o.rect.height()), avatarRect, 3);
	painter->drawPixmap(avatarRect.topLeft(), statusPixmap);

	drawText(painter, o, r, text, index);

	QPixmap sslPixmap = index.data(ContactListModel::UsingSSLRole).toBool() ?
	                    IconsetFactory::iconPixmap("psi/cryptoYes") :
	                    IconsetFactory::iconPixmap("psi/cryptoNo");
	const QSize sslPixmapSize = statusPixmap.size();
	QRect sslRect = relativeRect(o, sslPixmapSize, r, 3);
	painter->drawPixmap(sslRect.topLeft(), sslPixmap);
	r = relativeRect(option, QSize(), sslRect, 3);

	text = QString("(%1/%2)")
	        .arg(index.data(ContactListModel::OnlineContactsRole).toInt())
	        .arg(index.data(ContactListModel::TotalContactsRole).toInt());
	drawText(painter, o, r, text, index);
}

QRect PsiContactListViewDelegate::nameRect(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Q_UNUSED(index);
	return option.rect;
}

QRect PsiContactListViewDelegate::groupNameRect(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Q_UNUSED(index);
	return option.rect;
}

QRect PsiContactListViewDelegate::editorRect(const QRect& nameRect) const
{
	return nameRect;
}

void PsiContactListViewDelegate::optionChanged(const QString& option)
{
	if (option == contactListFontOptionPath) {
		delete font_;
		delete fontMetrics_;

		font_ = new QFont();
		font_->fromString(PsiOptions::instance()->getOption(contactListFontOptionPath).toString());
		fontMetrics_ = new QFontMetrics(*font_);
		rowHeight_ = qMax(fontMetrics_->height()+2, statusIconSize_+2);
		contactList()->viewport()->update();
	}
	else if (option == contactListBackgroundOptionPath) {
		QPalette p = contactList()->palette();
		p.setColor(QPalette::Base, ColorOpt::instance()->color(contactListBackgroundOptionPath));
		const_cast<ContactListView*>(contactList())->setPalette(p);
		contactList()->viewport()->update();
	}
	else if (option == showStatusMessagesOptionPath) {
		showStatusMessages_ = PsiOptions::instance()->getOption(showStatusMessagesOptionPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == showClientIconsPath) {
		showClientIcons_ = PsiOptions::instance()->getOption(showClientIconsPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == showMoodIconsPath) {
		showMoodIcons_ = PsiOptions::instance()->getOption(showMoodIconsPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == showActivityIconsPath) {
		showActivityIcons_ = PsiOptions::instance()->getOption(showActivityIconsPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == showTuneIconsPath) {
		showTuneIcons_ = PsiOptions::instance()->getOption(showTuneIconsPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == showGeolocIconsPath) {
		showGeolocIcons_ = PsiOptions::instance()->getOption(showGeolocIconsPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == showAvatarsPath) {
		showAvatars_ = PsiOptions::instance()->getOption(showAvatarsPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == useDefaultAvatarPath) {
		useDefaultAvatar_ = PsiOptions::instance()->getOption(useDefaultAvatarPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == avatarAtLeftOptionPath) {
		avatarAtLeft_ = PsiOptions::instance()->getOption(avatarAtLeftOptionPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == avatarSizeOptionPath) {
		avatarSize_ = PsiOptions::instance()->getOption(avatarSizeOptionPath).toInt();
		contactList()->viewport()->update();
	}
	else if(option == avatarRadiusOptionPath) {
		avatarRadius_ = PsiOptions::instance()->getOption(avatarRadiusOptionPath).toInt();
		contactList()->viewport()->update();
	}
	else if(option == showStatusIconsPath) {
		showStatusIcons_ = PsiOptions::instance()->getOption(showStatusIconsPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == statusIconsOverAvatarsPath) {
		statusIconsOverAvatars_ = PsiOptions::instance()->getOption(statusIconsOverAvatarsPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == allClientsOptionPath) {
		allClients_= PsiOptions::instance()->getOption(allClientsOptionPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == enableGroupsOptionPath) {
		enableGroups_ = PsiOptions::instance()->getOption(enableGroupsOptionPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == slimGroupsOptionPath) {
		slimGroup_ = PsiOptions::instance()->getOption(slimGroupsOptionPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == outlinedGroupsOptionPath) {
		outlinedGroup_ = PsiOptions::instance()->getOption(outlinedGroupsOptionPath).toBool();
		contactList()->viewport()->update();
	}
	else if(option == statusSingleOptionPath) {
		statusSingle_ = !PsiOptions::instance()->getOption(statusSingleOptionPath).toBool();
		contactList()->viewport()->update();
	}
}

void PsiContactListViewDelegate::rosterIconsSizeChanged(int size)
{
	statusIconSize_ = size;
	rowHeight_ = qMax(fontMetrics_->height()+2, statusIconSize_+2);
	contactList()->viewport()->update();
}

void PsiContactListViewDelegate::drawText(QPainter* painter, const QStyleOptionViewItem& o, const QRect& rect, const QString& text, const QModelIndex& index) const
{
	QRect r(rect);
	r.moveTop(r.top() + (r.height() - o.fontMetrics.height()) / 2);
	rect.adjusted(0, 2, 0, 0);
	ContactListViewDelegate::drawText(painter, o, r, text, index);
}

void PsiContactListViewDelegate::contactAlert(const QModelIndex& index)
{
	bool alerting = index.data(ContactListModel::IsAlertingRole).toBool();
	if (alerting)
		alertingIndexes_[index] = true;
	else
		alertingIndexes_.remove(index);

	if (alertingIndexes_.isEmpty())
		alertTimer_->stop();
	else
		alertTimer_->start();
}

void PsiContactListViewDelegate::clearAlerts()
{
	alertingIndexes_.clear();
	alertTimer_->stop();
}

void PsiContactListViewDelegate::updateAlerts()
{
	Q_ASSERT(!alertingIndexes_.isEmpty());
	if (!contactList()->isVisible())
		return; // needed?

	QRect contactListRect = contactList()->rect();
	QHashIterator<QModelIndex, bool> it(alertingIndexes_);
	while (it.hasNext()) {
		it.next();
		QRect r = contactList()->visualRect(it.key());
		if (contactListRect.intersects(r)) {
			contactList()->dataChanged(it.key(), it.key());
		}
	}
}
