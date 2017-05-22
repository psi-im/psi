/*
 * contactlistviewdelegate.cpp - base class for painting contact list items
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#include "contactlistviewdelegate_p.h"

#include "avatars.h"
#include "coloropt.h"
#include "common.h"
#include "contactlistitem.h"
#include "contactlistmodel.h"
#include "contactlistview.h"
#include "psiiconset.h"
#include "psioptions.h"
#include "debug.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QMutableSetIterator>
#include <QSetIterator>

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
static const QString nickIndentPath = "options.ui.look.contactlist.nick-indent";

#define AWAY_COLOR QLatin1String("options.ui.look.colors.contactlist.status.away")
#define DND_COLOR QLatin1String("options.ui.look.colors.contactlist.status.do-not-disturb")
#define OFFLINE_COLOR QLatin1String("options.ui.look.colors.contactlist.status.offline")
#define ONLINE_COLOR QLatin1String("options.ui.look.colors.contactlist.status.online")
#define ANIMATION1_COLOR QLatin1String("options.ui.look.colors.contactlist.status-change-animation1")
#define ANIMATION2_COLOR QLatin1String("options.ui.look.colors.contactlist.status-change-animation2")
#define STATUS_MESSAGE_COLOR QLatin1String("options.ui.look.colors.contactlist.status-messages")
#define HEADER_BACKGROUND_COLOR QLatin1String("options.ui.look.colors.contactlist.grouping.header-background")
#define HEADER_FOREGROUND_COLOR QLatin1String("options.ui.look.colors.contactlist.grouping.header-foreground")

#define ALERT_INTERVAL 100 /* msecs */
#define ANIM_INTERVAL 300 /* msecs */

static QRect relativeRect(const QStyleOption &option, const QSize &size, const QRect &prevRect, int padding = 0)
{
	QRect r = option.rect;
	bool isRTL = option.direction == Qt::RightToLeft;
	if (isRTL) {
		if (prevRect.isValid()) {
			r.setRight(prevRect.left() - padding);
		}

		if (size.isValid()) {
			r.setLeft(r.right() - size.width() + 1);
			r.setBottom(r.top() + size.height() - 1);
			r.translate(-1, 1);
		}
	}
	else {
		if (prevRect.isValid()) {
			r.setLeft(prevRect.right() + padding);
		}

		if (size.isValid()) {
			r.setSize(size);
			r.translate(1, 1);
		}
	}
	return r;
}

/************************************/
/* ContactListViewDelegate::Private */
/************************************/

ContactListViewDelegate::Private::Private(ContactListViewDelegate *parent)
	: QObject()
	, q(parent)
	, contactList(nullptr)
	, horizontalMargin_(5)
	, verticalMargin_(3)
	, alertTimer_(new QTimer(this))
	, animTimer(new QTimer(this))
	, font_()
	, fontMetrics_(QFont())
	, statusSingle_(false)
	, rowHeight_(0)
	, showStatusMessages_(false)
	, slimGroup_(false)
	, outlinedGroup_(false)
	, showClientIcons_(false)
	, showMoodIcons_(false)
	, showActivityIcons_(false)
	, showGeolocIcons_(false)
	, showTuneIcons_(false)
	, showAvatars_(false)
	, useDefaultAvatar_(false)
	, avatarAtLeft_(false)
	, showStatusIcons_(false)
	, statusIconsOverAvatars_(false)
	, avatarSize_(0)
	, avatarRadius_(0)
	, enableGroups_(false)
	, allClients_(false)
	, alertingIndexes()
	, animIndexes()
	, statusIconSize_(0)
	, _nickIndent(0)
	, animPhase(false)
	, _awayColor(ColorOpt::instance()->color(AWAY_COLOR))
	, _dndColor(ColorOpt::instance()->color(DND_COLOR))
	, _offlineColor(ColorOpt::instance()->color(OFFLINE_COLOR))
	, _onlineColor(ColorOpt::instance()->color(ONLINE_COLOR))
	, _animation1Color(ColorOpt::instance()->color(ANIMATION1_COLOR))
	, _animation2Color(ColorOpt::instance()->color(ANIMATION2_COLOR))
	, _statusMessageColor(ColorOpt::instance()->color(STATUS_MESSAGE_COLOR))
	, _headerBackgroundColor(ColorOpt::instance()->color(HEADER_BACKGROUND_COLOR))
	, _headerForegroundColor(ColorOpt::instance()->color(HEADER_FOREGROUND_COLOR))
{
	alertTimer_->setInterval(ALERT_INTERVAL);
	alertTimer_->setSingleShot(false);
	connect(alertTimer_, SIGNAL(timeout()), SLOT(updateAlerts()));

	animTimer->setInterval(ANIM_INTERVAL);
	animTimer->setSingleShot(false);
	connect(animTimer, SIGNAL(timeout()), SLOT(updateAnim()));


	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	connect(PsiIconset::instance(), SIGNAL(rosterIconsSizeChanged(int)), SLOT(rosterIconsSizeChanged(int)));

	statusIconSize_ = PsiIconset::instance()->roster.value(PsiOptions::instance()->getOption(statusIconsetOptionPath).toString())->iconSize();
}

ContactListViewDelegate::Private::~Private()
{
}

void ContactListViewDelegate::Private::optionChanged(const QString &option)
{
	if (option == contactListFontOptionPath) {
		font_ = QFont();
		font_.fromString(PsiOptions::instance()->getOption(contactListFontOptionPath).toString());
		fontMetrics_ = QFontMetrics(font_);
		rowHeight_ = qMax(fontMetrics_.height()+2, statusIconSize_+2);
		contactList->viewport()->update();
	}
	else if (option == contactListBackgroundOptionPath) {
		QPalette p = contactList->palette();
		p.setColor(QPalette::Base, ColorOpt::instance()->color(contactListBackgroundOptionPath));
		const_cast<ContactListView*>(contactList)->setPalette(p);
		contactList->viewport()->update();
	}
	else if (option == showStatusMessagesOptionPath) {
		showStatusMessages_ = PsiOptions::instance()->getOption(showStatusMessagesOptionPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == showClientIconsPath) {
		showClientIcons_ = PsiOptions::instance()->getOption(showClientIconsPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == showMoodIconsPath) {
		showMoodIcons_ = PsiOptions::instance()->getOption(showMoodIconsPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == showActivityIconsPath) {
		showActivityIcons_ = PsiOptions::instance()->getOption(showActivityIconsPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == showTuneIconsPath) {
		showTuneIcons_ = PsiOptions::instance()->getOption(showTuneIconsPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == showGeolocIconsPath) {
		showGeolocIcons_ = PsiOptions::instance()->getOption(showGeolocIconsPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == showAvatarsPath) {
		showAvatars_ = PsiOptions::instance()->getOption(showAvatarsPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == useDefaultAvatarPath) {
		useDefaultAvatar_ = PsiOptions::instance()->getOption(useDefaultAvatarPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == avatarAtLeftOptionPath) {
		avatarAtLeft_ = PsiOptions::instance()->getOption(avatarAtLeftOptionPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == avatarSizeOptionPath) {
		avatarSize_ = PsiOptions::instance()->getOption(avatarSizeOptionPath).toInt();
		contactList->viewport()->update();
	}
	else if(option == avatarRadiusOptionPath) {
		avatarRadius_ = PsiOptions::instance()->getOption(avatarRadiusOptionPath).toInt();
		contactList->viewport()->update();
	}
	else if(option == showStatusIconsPath) {
		showStatusIcons_ = PsiOptions::instance()->getOption(showStatusIconsPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == statusIconsOverAvatarsPath) {
		statusIconsOverAvatars_ = PsiOptions::instance()->getOption(statusIconsOverAvatarsPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == allClientsOptionPath) {
		allClients_= PsiOptions::instance()->getOption(allClientsOptionPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == enableGroupsOptionPath) {
		enableGroups_ = PsiOptions::instance()->getOption(enableGroupsOptionPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == slimGroupsOptionPath) {
		slimGroup_ = PsiOptions::instance()->getOption(slimGroupsOptionPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == outlinedGroupsOptionPath) {
		outlinedGroup_ = PsiOptions::instance()->getOption(outlinedGroupsOptionPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == statusSingleOptionPath) {
		statusSingle_ = !PsiOptions::instance()->getOption(statusSingleOptionPath).toBool();
		contactList->viewport()->update();
	}
	else if(option == nickIndentPath) {
		_nickIndent = PsiOptions::instance()->getOption(nickIndentPath).toInt();
		contactList->viewport()->update();
	}
}


void ContactListViewDelegate::Private::updateAlerts()
{
	// Q_ASSERT(!alertingIndexes_.isEmpty());

	if (!contactList->isVisible())
		return; // needed?

	// prepare ranges for updating to reduce 'dataChanged' invoking
	QHash<QModelIndex, QPair<int, int>> ranges;

	// Remove invalid indexes
	QMutableSetIterator<QPersistentModelIndex> it(alertingIndexes);
	while (it.hasNext()) {
		QModelIndex index = it.next();

		// Clean invalid indexes
		if (!index.isValid()) {
			it.remove();
			continue;
		}

		QModelIndex parent = index.parent();
		int row = index.row();
		if (ranges.contains(parent)) {
			if (index.row() < ranges.value(parent).first)
				ranges[parent].first = row;
			else if (index.row() > ranges.value(parent).second)
				ranges[parent].second = row;
		}
		else {
			ranges.insert(parent, QPair<int, int>(row, row));
		}

	}

	QHashIterator<QModelIndex, QPair<int, int>> it2(ranges);
	while (it2.hasNext()) {
		it2.next();
		int row1 = it2.value().first;
		int row2 = it2.value().second;
		QModelIndex index = it2.key();

		// update contacts
		contactList->dataChanged(index.child(row1, 0), index.child(row2, 0));
	}
}

void ContactListViewDelegate::Private::updateAnim()
{
	animPhase = !animPhase;

	// Q_ASSERT(!alertingIndexes_.isEmpty());

	if (!contactList->isVisible())
		return; // needed?

	// prepare ranges for updating to reduce 'dataChanged' invoking
	QHash<QModelIndex, QPair<int, int>> ranges;

	// Remove invalid indexes
	QMutableSetIterator<QPersistentModelIndex> it(animIndexes);
	while (it.hasNext()) {
		QModelIndex index = it.next();

		// Clean invalid indexes
		if (!index.isValid()) {
			it.remove();
			continue;
		}

		QModelIndex parent = index.parent();
		int row = index.row();
		if (ranges.contains(parent)) {
			if (index.row() < ranges.value(parent).first)
				ranges[parent].first = row;
			else if (index.row() > ranges.value(parent).second)
				ranges[parent].second = row;
		}
		else {
			ranges.insert(parent, QPair<int, int>(row, row));
		}

	}

	QHashIterator<QModelIndex, QPair<int, int>> it2(ranges);
	while (it2.hasNext()) {
		it2.next();
		int row1 = it2.value().first;
		int row2 = it2.value().second;
		QModelIndex index = it2.key();

		// update contacts
		contactList->dataChanged(index.child(row1, 0), index.child(row2, 0));
	}
}

void ContactListViewDelegate::Private::rosterIconsSizeChanged(int size)
{
	statusIconSize_ = size;
	rowHeight_ = qMax(fontMetrics_.height() + 2, statusIconSize_ + 2);
	contactList->viewport()->update();
}

QPixmap ContactListViewDelegate::Private::statusPixmap(const QModelIndex &index)
{
	ContactListItem *item = qvariant_cast<ContactListItem*>(index.data(ContactListModel::ContactListItemRole));
	ContactListItem::Type type = item->type();

	if (type == ContactListItem::Type::ContactType ||
		type == ContactListItem::Type::AccountType)
	{
		bool alert = index.data(ContactListModel::IsAlertingRole).toBool();
		setAlertEnabled(index, alert);
		if (alert) {
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

	int s = index.data(ContactListModel::StatusTypeRole).toInt();

	if (type == ContactListItem::Type::ContactType) {
		if (!index.data(ContactListModel::PresenceErrorRole).toString().isEmpty())
			s = STATUS_ERROR;
		else if (index.data(ContactListModel::IsAgentRole).toBool())
			/* s = s */ ;
		else if (index.data(ContactListModel::AskingForAuthRole).toBool() && s == XMPP::Status::Offline)
			s = STATUS_ASK;
		else if (!index.data(ContactListModel::AuthorizesToSeeStatusRole).toBool() && s == XMPP::Status::Offline)
			s = STATUS_NOAUTH;
	}

	return PsiIconset::instance()->statusPtr(index.data(ContactListModel::JidRole).toString(), s)->pixmap();
}

QList<QPixmap> ContactListViewDelegate::Private::clientPixmap(const QModelIndex &index)
{
	QList<QPixmap> pixList;
	ContactListItem *item = qvariant_cast<ContactListItem*>(index.data(ContactListModel::ContactListItemRole));
	if (item->type() != ContactListItem::Type::ContactType)
		return pixList;

	QStringList vList = index.data(ContactListModel::ClientRole).toStringList();
	if(vList.isEmpty())
		return pixList;

	for (QString client: vList) {
		const QPixmap &pix = IconsetFactory::iconPixmap("clients/" + client);
		if(!pix.isNull())
			pixList.push_back(pix);
	}

	return pixList;

}

QPixmap ContactListViewDelegate::Private::avatarIcon(const QModelIndex &index)
{
	int avSize = showAvatars_ ? avatarSize_ : 0;
	QPixmap av = index.data(ContactListModel::IsMucRole).toBool()
				 ? QPixmap()
				 : index.data(ContactListModel::AvatarRole).value<QPixmap>();

	if(av.isNull() && useDefaultAvatar_)
		av = IconsetFactory::iconPixmap("psi/default_avatar");

	return AvatarFactory::roundedAvatar(av, avatarRadius_, avSize);

}

void ContactListViewDelegate::Private::drawContact(QPainter* painter, const QModelIndex& index)
{
	drawBackground(painter, opt, index);

	QRect r = opt.rect;

	QRect avatarRect(r);
	if (showAvatars_) {
		QPixmap avatarPixmap = avatarIcon(index);
		int size = avatarSize_;
		avatarRect.setSize(QSize(size,size));
		if (avatarAtLeft_) {
			avatarRect.translate(enableGroups_ ? -5 : -1, 1);
			r.setLeft(avatarRect.right() + 3);
		}
		else {
			avatarRect.moveTopRight(r.topRight());
			avatarRect.translate(-1, 1);
			r.setRight(avatarRect.left() - 3);
		}
		int row = (statusSingle_ && showStatusMessages_) ? rowHeight_ * 3 / 2 : rowHeight_; // height required for nick
		int h = (size - row) / 2; // padding from top to center it
		if (h > 0) {
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
	if (!statusPixmap.isNull()) {
		if (statusIconsOverAvatars_ && showAvatars_) {
			statusPixmap = statusPixmap.scaled(12, 12, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			statusRect.setSize(statusPixmap.size());
			statusRect.moveBottomRight(avatarRect.bottomRight());
			statusRect.translate(-1,-2);
			r.setLeft(r.left() + 3);
		}
		else {
			statusRect.setSize(statusPixmap.size());
			statusRect.translate(0, 1);
			if (opt.direction == Qt::RightToLeft) {
				statusRect.setRight(r.right() - 1);
				r.setRight(statusRect.right() - 3);
			}
			else {
				statusRect.setLeft(r.left() + 1);
				r.setLeft(statusRect.right() + 3);
			}
		}
		painter->drawPixmap(statusRect.topLeft(), statusPixmap);
	}
	else {
		r.setLeft(r.left() + 3);
	}

	QColor textColor;

	bool anim = index.data(ContactListModel::IsAnimRole).toBool();
	setAnimEnabled(index, anim);
	if (anim) {
		if(animPhase) {
			textColor = _animation2Color;
		}
		else {
			textColor = _animation1Color;
		}
	}
	else {
		int s = index.data(ContactListModel::StatusTypeRole).toInt();
		if (s == XMPP::Status::Away || s == XMPP::Status::XA)
			textColor = _awayColor;
		else if (s == XMPP::Status::DND)
			textColor = _dndColor;
		else if (s == XMPP::Status::Offline)
			textColor = _offlineColor;
		else
			textColor = _onlineColor;
	}

	QStyleOptionViewItemV2 o = opt;
	o.font = font_;
	o.font.setItalic(index.data(ContactListModel::BlockRole).toBool());
	o.fontMetrics = fontMetrics_;
	QPalette palette = o.palette;
	palette.setColor(QPalette::Text, textColor);
	o.palette = palette;

	r.setLeft(r.left() + _nickIndent);

	QString text = index.data(Qt::DisplayRole).toString();
	QString statusText = index.data(ContactListModel::StatusTextRole).toString();
	if (showStatusMessages_ && !statusText.isEmpty()) {
		if(!statusSingle_) {
			text = tr("%1 (%2)").arg(text).arg(statusText);
			drawText(painter, o, r, text);
		}
		else {
			QRect txtRect(r);
			txtRect.setHeight(r.height() * 2 / 3);
			drawText(painter, o, txtRect, text);
			palette.setColor(QPalette::Text, _statusMessageColor);
			o.palette = palette;
			txtRect.moveTopRight(txtRect.bottomRight());
			txtRect.setHeight(r.height() - txtRect.height());
			o.font.setPointSize(qMax(o.font.pointSize() - 2, 7));
			o.fontMetrics = QFontMetrics(o.font);
			painter->save();
			drawText(painter, o, txtRect, statusText);
			painter->restore();
		}
	}
	else {
		if(showStatusMessages_ && statusSingle_)
			r.setHeight(r.height() * 2 / 3);

		drawText(painter, o, r, text);
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
			QList<QPixmap> pixList = this->clientPixmap(index);

			for (QList<QPixmap>::ConstIterator it = pixList.begin(); it != pixList.end(); ++it) {
				const QPixmap &pix = *it;
				rightPixs.push_back(pix);
				rightWidths.push_back(pix.width());
				if (!allClients_)
					break;
			}
		}

		if (showMoodIcons_ && !index.data(ContactListModel::MoodRole).isNull()) {
			const QPixmap &pix = IconsetFactory::iconPixmap(QString("mood/%1").arg(index.data(ContactListModel::MoodRole).toString()));
			if(!pix.isNull()) {
				rightPixs.push_back(pix);
				rightWidths.push_back(pix.width());
			}
		}

		if (showActivityIcons_ && !index.data(ContactListModel::ActivityRole).isNull()) {
			const QPixmap &pix = IconsetFactory::iconPixmap(QString("activities/%1").arg(index.data(ContactListModel::ActivityRole).toString()));
			if(!pix.isNull()) {
				rightPixs.push_back(pix);
				rightWidths.push_back(pix.width());
			}
		}

		if (showTuneIcons_ && index.data(ContactListModel::TuneRole).toBool()) {
			const QPixmap &pix = IconsetFactory::iconPixmap("psi/notification_roster_tune");
			rightPixs.push_back(pix);
			rightWidths.push_back(pix.width());
		}

		if (showGeolocIcons_ && index.data(ContactListModel::GeolocationRole).toBool()) {
			const QPixmap &pix = IconsetFactory::iconPixmap("system/geolocation");
			rightPixs.push_back(pix);
			rightWidths.push_back(pix.width());
		}

		if (index.data(ContactListModel::IsSecureRole).toBool()) {
			const QPixmap &pix = IconsetFactory::iconPixmap("psi/pgp");
			rightPixs.push_back(pix);
			rightWidths.push_back(pix.width());
		}
	}

	if (rightPixs.isEmpty() && mucMessages.isEmpty()) {
		return;
	}

	int sumWidth = 0;
	if (isMuc)
		sumWidth = fontMetrics_.width(mucMessages);
	else {
		for (int w: rightWidths) {
			sumWidth += w;
		}
		sumWidth += rightPixs.count();
	}

	QColor bgc = (opt.state & QStyle::State_Selected)
				 ? palette.color(QPalette::Highlight)
				 : palette.color(QPalette::Base);
	QColor tbgc = bgc;
	tbgc.setAlpha(0);
	QLinearGradient grad(r.right() - sumWidth - 20, 0, r.right() - sumWidth, 0);
	grad.setColorAt(0, tbgc);
	grad.setColorAt(1, bgc);
	QBrush tbakBr(grad);
	QRect gradRect(r);
	gradRect.setLeft(gradRect.right() - sumWidth - 20);
	painter->fillRect(gradRect, tbakBr);

	if (isMuc) {
		iconRect.setLeft(iconRect.right() - sumWidth - 1);
		painter->drawText(iconRect, mucMessages);
	}
	else {
		for (int i = 0; i < rightPixs.size(); i++) {
			QPixmap pix = rightPixs[i];
			iconRect.setRight(iconRect.right() - pix.width() -1);
			painter->drawPixmap(iconRect.topRight(), pix);
		}
	}
}

void ContactListViewDelegate::Private::drawGroup(QPainter *painter, const QModelIndex &index)
{
	QStyleOptionViewItemV2 o = opt;
	o.font = font_;
	o.fontMetrics = fontMetrics_;
	QPalette palette = o.palette;
	if (!slimGroup_)
		palette.setColor(QPalette::Base, _headerBackgroundColor);
	palette.setColor(QPalette::Text, _headerForegroundColor);
	o.palette = palette;

	drawBackground(painter, o, index);

	QRect r = opt.rect;
	if (!slimGroup_ && outlinedGroup_) {
		painter->setPen(QPen(_headerForegroundColor));
		QRect gr(r);
		gr.setLeft(contactList->x());
		painter->drawRect(gr);
	}

	const QPixmap &pixmap = index.data(ContactListModel::ExpandedRole).toBool()
							? IconsetFactory::iconPtr("psi/groupOpen")->pixmap()
							: IconsetFactory::iconPtr("psi/groupClosed")->pixmap();

	QSize pixmapSize = pixmap.size();
	QRect pixmapRect = relativeRect(opt, pixmapSize, QRect());
	r = relativeRect(opt, QSize(), pixmapRect, 3);
	painter->drawPixmap(pixmapRect.topLeft(), pixmap);

	QString text = index.data(ContactListModel::DisplayGroupRole).toString();
	drawText(painter, o, r, text);

	if(slimGroup_ && !(opt.state & QStyle::State_Selected)) {
		int h = r.y() + (r.height() / 2);
		int x = r.left() + fontMetrics_.width(text) + 8;
		painter->setPen(QPen(_headerBackgroundColor, 2));
		painter->drawLine(x, h, r.right(), h);
	}
}

void ContactListViewDelegate::Private::drawAccount(QPainter *painter, const QModelIndex &index)
{
	QStyleOptionViewItemV2 o = opt;
	o.font = font_;
	o.fontMetrics = fontMetrics_;
	QPalette palette = o.palette;
	palette.setColor(QPalette::Base, _headerBackgroundColor);
	palette.setColor(QPalette::Text, _headerForegroundColor);
	o.palette = palette;

	drawBackground(painter, o, index);

	if (outlinedGroup_) {
		painter->setPen(QPen(_headerForegroundColor));
		painter->drawRect(opt.rect);
	}

	const QPixmap statusPixmap = this->statusPixmap(index);
	const QSize pixmapSize = statusPixmap.size();
	const QRect avatarRect = relativeRect(o, pixmapSize, QRect());
	QString text = index.data(Qt::DisplayRole).toString();
	QRect r = relativeRect(o, QSize(o.fontMetrics.width(text), o.rect.height()), avatarRect, 3);
	painter->drawPixmap(avatarRect.topLeft(), statusPixmap);

	drawText(painter, o, r, text);

	QPixmap sslPixmap = index.data(ContactListModel::UsingSSLRole).toBool()
						? IconsetFactory::iconPixmap("psi/cryptoYes")
						: IconsetFactory::iconPixmap("psi/cryptoNo");

	QSize sslPixmapSize = statusPixmap.size();
	QRect sslRect = relativeRect(o, sslPixmapSize, r, 3);
	painter->drawPixmap(sslRect.topLeft(), sslPixmap);
	r = relativeRect(opt, QSize(), sslRect, 3);

	text = QString("(%1/%2)")
		   .arg(index.data(ContactListModel::OnlineContactsRole).toInt())
		   .arg(index.data(ContactListModel::TotalContactsRole).toInt());
	drawText(painter, o, r, text);
}

void ContactListViewDelegate::Private::drawText(QPainter *painter, const QStyleOptionViewItem &opt, const QRect &rect, const QString &text)
{
	QRect rect2 = rect;
	rect2.moveTop(rect2.top() + (rect2.height() - opt.fontMetrics.height()) / 2);

	if (text.isEmpty())
		return;

	QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled
							  ? QPalette::Normal
							  : QPalette::Disabled;

	if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
		cg = QPalette::Inactive;
	if (opt.state & QStyle::State_Selected) {
		painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
	}
	else {
		painter->setPen(opt.palette.color(cg, QPalette::Text));
	}

	QString txt = text;
	bool isElided = rect2.width() < opt.fontMetrics.width(text);
	if (isElided) {
		txt = opt.fontMetrics.elidedText(text, opt.textElideMode, rect2.width());
		painter->save();
		QRect txtRect(rect2);
		txtRect.setHeight(opt.fontMetrics.height());
		painter->setClipRect(txtRect);
	}

	painter->setFont(opt.font);
	QTextOption to;
	if (opt.direction == Qt::RightToLeft)
		to.setAlignment(Qt::AlignRight);
	painter->drawText(rect2, txt, to);

	if (isElided) {
		painter->restore();
	}
}

void ContactListViewDelegate::Private::drawBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index)
{
	QStyleOptionViewItem opt = option;

	// otherwise we're not painting the left item margin sometimes
	// (for example when changing current selection by keyboard)
	opt.rect.setLeft(0);

	{
		if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
			painter->fillRect(opt.rect, backgroundColor(option, index));
		}
		else {
			QPointF oldBO = painter->brushOrigin();
			painter->setBrushOrigin(opt.rect.topLeft());
			painter->fillRect(opt.rect, backgroundColor(option, index));
			painter->setBrushOrigin(oldBO);
		}
	}
}

void ContactListViewDelegate::Private::setEditorCursorPosition(QWidget *editor, int cursorPosition)
{
	QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
	if (lineEdit) {
		if (cursorPosition == -1)
			cursorPosition = lineEdit->text().length();
		lineEdit->setCursorPosition(cursorPosition);
	}
}

// copied from void QItemDelegate::drawBackground(), Qt 4.3.4
QColor ContactListViewDelegate::Private::backgroundColor(const QStyleOptionViewItem &option, const QModelIndex &index)
{
	QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
							  ? QPalette::Normal
							  : QPalette::Disabled;

	if (cg == QPalette::Normal && !(option.state & QStyle::State_Active)) {
		cg = QPalette::Inactive;
	}

	if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
		return option.palette.brush(cg, QPalette::Highlight).color();
	}
	else {
		QVariant value = index.data(Qt::BackgroundRole);
#ifdef HAVE_QT5
		if (value.canConvert<QBrush>()) {
#else
		if (qVariantCanConvert<QBrush>(value)) {
#endif
			return qvariant_cast<QBrush>(value).color();
		}
		else {
			return option.palette.brush(cg, QPalette::Base).color();
		}
	}

	return Qt::white;
}

void ContactListViewDelegate::Private::doSetOptions(const QStyleOptionViewItem &option, const QModelIndex &index)
{
	opt = q->setOptions(index, option);
	const QStyleOptionViewItemV2 *v2 = qstyleoption_cast<const QStyleOptionViewItemV2 *>(&option);
	opt.features = v2 ? v2->features : QStyleOptionViewItemV2::ViewItemFeatures(QStyleOptionViewItemV2::None);

	const HoverableStyleOptionViewItem *hoverable = qstyleoption_cast<const HoverableStyleOptionViewItem*>(&option);
	opt.hovered = hoverable ? hoverable->hovered : false;
	opt.hoveredPosition = hoverable ? hoverable->hoveredPosition : QPoint();

	// see hoverabletreeview.cpp
	if ((opt.displayAlignment & Qt::AlignLeft)
		&& (opt.displayAlignment & Qt::AlignRight)
		&& (opt.displayAlignment & Qt::AlignHCenter)
		&& (opt.displayAlignment & Qt::AlignJustify)) {

		opt.hovered = true;
		opt.hoveredPosition = QPoint(opt.decorationSize.width(), opt.decorationSize.height());
	}
}

QRect ContactListViewDelegate::Private::getEditorGeometry(const QStyleOptionViewItem &option, const QModelIndex &index)
{
	QRect rect;

	ContactListItem *item = qvariant_cast<ContactListItem*>(index.data(ContactListModel::ContactListItemRole));
	ContactListItem::Type type = item->type();

	switch (type) {
	case ContactListItem::Type::ContactType:
		rect = option.rect.adjusted(-1, 0, 0, 1);
		break;

	case ContactListItem::Type::GroupType:
		rect = option.rect.adjusted(-1, -3, 0, 2);
		rect.setRight(option.rect.right() - horizontalMargin_ - 1);
		break;

	default:
		break;
	}

	return rect;
}

void ContactListViewDelegate::Private::setAlertEnabled(const QModelIndex &index, bool enable)
{
	if (enable && !alertingIndexes.contains(index)) {
		alertingIndexes << index;
		if (!alertTimer_->isActive()) {
			alertTimer_->start();
		}
	}
	else if (!enable && alertingIndexes.contains(index)) {
		alertingIndexes.remove(index);
		if (alertingIndexes.isEmpty()) {
			alertTimer_->stop();
		}
	}
}

void ContactListViewDelegate::Private::setAnimEnabled(const QModelIndex &index, bool enable)
{
	if (enable && !animIndexes.contains(index)) {
		animIndexes << index;
		if (!animTimer->isActive()) {
			animTimer->start();
		}
	}
	else if (!enable && animIndexes.contains(index)) {
		animIndexes.remove(index);
		if (alertingIndexes.isEmpty()) {
			animTimer->stop();
		}
	}
}

/***************************/
/* ContactListViewDelegate */
/***************************/


ContactListViewDelegate::ContactListViewDelegate(ContactListView *parent)
	: QItemDelegate(parent)
{
	d = new Private(this);
	d->contactList = parent;

	d->optionChanged(slimGroupsOptionPath);
	d->optionChanged(outlinedGroupsOptionPath);
	d->optionChanged(contactListFontOptionPath);
	d->optionChanged(contactListBackgroundOptionPath);
	d->optionChanged(showStatusMessagesOptionPath);
	d->optionChanged(statusSingleOptionPath);
	d->optionChanged(showClientIconsPath);
	d->optionChanged(showMoodIconsPath);
	d->optionChanged(showGeolocIconsPath);
	d->optionChanged(showActivityIconsPath);
	d->optionChanged(showTuneIconsPath);
	d->optionChanged(avatarSizeOptionPath);
	d->optionChanged(avatarRadiusOptionPath);
	d->optionChanged(showAvatarsPath);
	d->optionChanged(useDefaultAvatarPath);
	d->optionChanged(avatarAtLeftOptionPath);
	d->optionChanged(showStatusIconsPath);
	d->optionChanged(statusIconsOverAvatarsPath);
	d->optionChanged(allClientsOptionPath);
	d->optionChanged(enableGroupsOptionPath);
	d->optionChanged(nickIndentPath);
}

ContactListViewDelegate::~ContactListViewDelegate()
{
	delete d;
}

void ContactListViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	d->doSetOptions(option, index);

	d->iconMode  = !(d->opt.state & QStyle::State_Enabled)
				   ? QIcon::Disabled
				   : (d->opt.state & QStyle::State_Selected)
					 ? QIcon::Selected
					 : QIcon::Normal;

	d->iconState = d->opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;

	ContactListItem *item = qvariant_cast<ContactListItem*>(index.data(ContactListModel::ContactListItemRole));
	ContactListItem::Type type = item->type();

	switch (type) {
	case ContactListItem::Type::ContactType:  d->drawContact(painter, index);           break;
	case ContactListItem::Type::GroupType:    d->drawGroup(painter, index);             break;
	case ContactListItem::Type::AccountType:  d->drawAccount(painter, index);           break;
	case ContactListItem::Type::InvalidType:  painter->fillRect(option.rect, Qt::red);  break;
	default: QItemDelegate::paint(painter, option, index);                              break;
	}
}

int ContactListViewDelegate::avatarSize() const
{
	return d->showAvatars_ ? qMax(d->avatarSize_ + 2, d->rowHeight_) : d->rowHeight_;
}

QSize ContactListViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option);

	if (!index.isValid())
		return QSize(0, 0);

	if (qvariant_cast<ContactListItem::Type>(index.data(ContactListModel::TypeRole)) == ContactListItem::Type::ContactType) {
		if (!d->statusSingle_ || !d->showStatusMessages_) {
			return QSize(16, avatarSize());
		}
		else {
			return QSize(16, qMax(avatarSize(), d->rowHeight_ * 3 / 2));
		}
	}
	else {
		return QSize(16, d->rowHeight_);
	}
}

void ContactListViewDelegate::contactAlert(const QModelIndex &index)
{
	bool alerting = index.data(ContactListModel::IsAlertingRole).toBool();
	if (alerting)
		d->alertingIndexes << index;
	else
		d->alertingIndexes.remove(index);

	if (d->alertingIndexes.isEmpty())
		d->alertTimer_->stop();
	else
		d->alertTimer_->start();
}

void ContactListViewDelegate::animateContacts(const QModelIndexList &indexes, bool started)
{
	for (const QModelIndex &index: indexes) {
		if (started) {
			d->animIndexes << index;
		}
		else {
			d->animIndexes.remove(index);
		}
	}

	if (d->animIndexes.isEmpty())
		d->animTimer->stop();
	else if (!d->animTimer->isActive())
		d->animTimer->start();
}

void ContactListViewDelegate::clearAlerts()
{
	d->alertingIndexes.clear();
	d->alertTimer_->stop();
}

void ContactListViewDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QRect widgetRect = d->getEditorGeometry(option, index);
	if (!widgetRect.isEmpty()) {
		editor->setGeometry(widgetRect);
	}
}

// we're preventing modifications of QLineEdit while it's still being displayed,
// and the contact we're editing emits dataChanged()
void ContactListViewDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
	if (lineEdit) {
		if (lineEdit->text().isEmpty()) {
			lineEdit->setText(index.data(Qt::EditRole).toString());
			lineEdit->selectAll();
		}
		return;
	}

	QItemDelegate::setEditorData(editor, index);
}

void ContactListViewDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
	QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
	if (lineEdit) {
		if (index.data(Qt::EditRole).toString() != lineEdit->text()) {
			model->setData(index, lineEdit->text(), Qt::EditRole);
		}
	}
	else {
		QItemDelegate::setModelData(editor, model, index);
	}
}

// adapted from QItemDelegate::eventFilter()
bool ContactListViewDelegate::eventFilter(QObject *object, QEvent *event)
{
	QWidget *editor = qobject_cast<QWidget*>(object);
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Up) {
			d->setEditorCursorPosition(editor, 0);
			return true;
		}
		else if (keyEvent->key() == Qt::Key_Down) {
			d->setEditorCursorPosition(editor, -1);
			return true;
		}
		else if (keyEvent->key() == Qt::Key_PageUp ||
				 keyEvent->key() == Qt::Key_PageDown) {

			return true;
		}
		else if (keyEvent->key() == Qt::Key_Tab ||
				 keyEvent->key() == Qt::Key_Backtab) {

			return true;
		}
	}

	return QItemDelegate::eventFilter(object, event);
}
