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
#include "mood.h"
#include "activity.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QMutableSetIterator>
#include <QSetIterator>
#include <QApplication>
#include <QDesktopWidget>

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

#define PSI_HIDPI computeScaleFactor(contactList)
//#define PSI_HIDPI (2) // for testing purposes

int computeScaleFactor(ContactListView *contactList) {
	static int factor = 0;
	if (!factor) {
		if (devicePixelRatio(contactList) > 1) {
			factor = 1; // It's autodetected by Qt. it will scale everything on it's own.
		} else {
			factor = qApp->desktop()->logicalDpiX() / 90;
			if (!factor) {
				factor = 1;
			}
		}
	}
	return factor;
}

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

ContactListViewDelegate::Private::Private(ContactListViewDelegate *parent, ContactListView *contactList)
	: QObject()
	, q(parent)
	, contactList(contactList)
	, horizontalMargin_(5)
	, verticalMargin_(3)
    , statusIconSize_(0)
	, avatarRadius_(0)
	, alertTimer_(new QTimer(this))
	, animTimer(new QTimer(this))
	, fontMetrics_(QFont())
    , statusFontMetrics_(QFont())
    , statusSingle_(false)
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
	, enableGroups_(false)
	, allClients_(false)
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
	bulkOptUpdate = true;
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
	bulkOptUpdate = false;
	recomputeGeometry();
	contactList->viewport()->update();
}

ContactListViewDelegate::Private::~Private()
{
}

void ContactListViewDelegate::Private::optionChanged(const QString &option)
{
	bool updateGeometry = false;
	bool updateViewport = false;

	if (option == contactListFontOptionPath) {
		font_.fromString(PsiOptions::instance()->getOption(contactListFontOptionPath).toString());
		fontMetrics_ = QFontMetrics(font_);
		statusFont_.setPointSize(qMax(font_.pointSize()-2, 7));
		statusFontMetrics_ = QFontMetrics(statusFont_);

		updateGeometry = true;
	}
	else if (option == contactListBackgroundOptionPath) {
		QPalette p = contactList->palette();
		p.setColor(QPalette::Base, ColorOpt::instance()->color(contactListBackgroundOptionPath));
		contactList->setPalette(p);
		updateViewport = true;
	}
	else if (option == showStatusMessagesOptionPath) {
		showStatusMessages_ = PsiOptions::instance()->getOption(showStatusMessagesOptionPath).toBool();
		updateGeometry = true;
	}
	else if(option == showClientIconsPath) {
		showClientIcons_ = PsiOptions::instance()->getOption(showClientIconsPath).toBool();
		updateGeometry = true;
	}
	else if(option == showMoodIconsPath) {
		showMoodIcons_ = PsiOptions::instance()->getOption(showMoodIconsPath).toBool();
		updateGeometry = true;
	}
	else if(option == showActivityIconsPath) {
		showActivityIcons_ = PsiOptions::instance()->getOption(showActivityIconsPath).toBool();
		updateGeometry = true;
	}
	else if(option == showTuneIconsPath) {
		showTuneIcons_ = PsiOptions::instance()->getOption(showTuneIconsPath).toBool();
		updateGeometry = true;
	}
	else if(option == showGeolocIconsPath) {
		showGeolocIcons_ = PsiOptions::instance()->getOption(showGeolocIconsPath).toBool();
		updateGeometry = true;
	}
	else if(option == showAvatarsPath) {
		showAvatars_ = PsiOptions::instance()->getOption(showAvatarsPath).toBool();
		updateGeometry = true;
	}
	else if(option == useDefaultAvatarPath) {
		useDefaultAvatar_ = PsiOptions::instance()->getOption(useDefaultAvatarPath).toBool();
		updateViewport = true;
	}
	else if(option == avatarAtLeftOptionPath) {
		avatarAtLeft_ = PsiOptions::instance()->getOption(avatarAtLeftOptionPath).toBool();
		updateGeometry = true;
	}
	else if(option == avatarSizeOptionPath) {
		int s = pointToPixel(PsiOptions::instance()->getOption(avatarSizeOptionPath).toInt());
		avatarRect_.setSize(QSize(s, s));
		updateGeometry = true;
	}
	else if(option == avatarRadiusOptionPath) {
		avatarRadius_ = pointToPixel(PsiOptions::instance()->getOption(avatarRadiusOptionPath).toInt());
		updateViewport = true;
	}
	else if(option == showStatusIconsPath) {
		showStatusIcons_ = PsiOptions::instance()->getOption(showStatusIconsPath).toBool();
		updateGeometry = true;
	}
	else if(option == statusIconsOverAvatarsPath) {
		statusIconsOverAvatars_ = PsiOptions::instance()->getOption(statusIconsOverAvatarsPath).toBool();
		updateGeometry = true;
	}
	else if(option == allClientsOptionPath) {
		allClients_= PsiOptions::instance()->getOption(allClientsOptionPath).toBool();
		updateViewport = true;
	}
	else if(option == enableGroupsOptionPath) {
		enableGroups_ = PsiOptions::instance()->getOption(enableGroupsOptionPath).toBool();
		updateViewport = true;
	}
	else if(option == slimGroupsOptionPath) {
		slimGroup_ = PsiOptions::instance()->getOption(slimGroupsOptionPath).toBool();
		updateViewport = true;
	}
	else if(option == outlinedGroupsOptionPath) {
		outlinedGroup_ = PsiOptions::instance()->getOption(outlinedGroupsOptionPath).toBool();
		updateViewport = true;
	}
	else if(option == statusSingleOptionPath) {
		statusSingle_ = !PsiOptions::instance()->getOption(statusSingleOptionPath).toBool();
		updateGeometry = true;
	}

	if (!bulkOptUpdate) {
		if (updateGeometry) {
			recomputeGeometry();
			updateViewport = true;
		}
		if (updateViewport) {
			contactList->viewport()->update();
		}
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
	recomputeGeometry();
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
	int avSize = showAvatars_ ? avatarRect_.height()  : 0;
	QPixmap av = index.data(ContactListModel::IsMucRole).toBool()
				 ? QPixmap()
				 : index.data(ContactListModel::AvatarRole).value<QPixmap>();

	if(av.isNull() && useDefaultAvatar_)
		av = IconsetFactory::iconPixmap("psi/default_avatar");

	return AvatarFactory::roundedAvatar(av, avatarRadius_, avSize);

}

void ContactListViewDelegate::Private::drawContact(QPainter* painter, const QModelIndex& index)
{
	/* We have a few possible ways to draw contact
	 * 1) Avatar is hidden or on the left or on the right
	 * 2) remaining space near avatar can be splitten into two lines if we want to draw status text in second line
	 * 3) first line contains all possible icons (status icon may sometimes be drawn over avatar) and nick name.
	 * 4) nick name considers RTL so icons position depends on it too (left/right)
	 *
	 * Algo:
	 * 1) Devide space in 3 rectangles: avatar, nickname with icons and status text if any
	 * 2) Calculate full height as MAX(avatar height, MAX(nick text height, highest icon) + status text height) + gaps.
	 * 3) Align avatar to the center of its rect and draw it.
	 * 4) Align status text to one side(consider RTL) and vertical center and draw it
	 * 5) If status icon should be shown over avatar (corresponding option is enabled and avatars enabled too),
	 *    The draw it over avatar
	 * 6) Calculate space required for remaining icons
	 * 7) Divide nickname/icons rectangle into two for icons and for nickname/status_icon. (icons are in favor for space)
	 * 8) If nickname rectangle has zero size just skip nickname/status icon drawing and go to p.13
	 * 9) If status icon is not over avatar then align status based on RTL settings and vertically and draw it
	 * 10) Recalculate rectangle for nickname and other icons (status outside)
	 * 11) Align nick name with respect to RTL and vertically in its rectangle and draw it
	 * 12) on the other side of nickname rectangle draw transparent gradient if it intersects nick space to hide nickname softly
	 * 13) Draw icons in its rectangle aligned vertically starting from opposite side on nickname start
	 */


	drawBackground(painter, opt, index);

	QRect r = opt.rect; // our full contact space to draw

	QRect contactBoundingRect(contactBoundingRect_);
	QRect avatarStatusRect(avatarStatusRect_);
	QRect linesRect(linesRect_);
	QRect firstLineRect(firstLineRect_);
	QRect secondLineRect(secondLineRect_);
	QRect avatarRect(avatarRect_);
	QRect statusIconRect(statusIconRect_);
	QRect statusLineRect(statusLineRect_);
	QRect pepIconsRect(pepIconsRect_);
	QRect nickRect(nickRect_);

	// first align to current rect
	contactBoundingRect.translate(r.topLeft());
	avatarStatusRect.translate(r.topLeft());
	linesRect.translate(r.topLeft());
	firstLineRect.translate(r.topLeft());
	secondLineRect.translate(r.topLeft());
	avatarRect.translate(r.topLeft());
	statusIconRect.translate(r.topLeft());
	statusLineRect.translate(r.topLeft());
	pepIconsRect.translate(r.topLeft());
	nickRect.translate(r.topLeft());

	// next expand to r.width
	// first check if we need expand at all
	if (contactBoundingRect.width() + 2 * ContactHMargin*PSI_HIDPI < r.width()) {
		// our previously computed minimal rect is too small for this roster. so expand
		int diff = r.width() - (contactBoundingRect.width() + 2 * ContactHMargin*PSI_HIDPI);
		if (!avatarAtLeft_) {
			avatarStatusRect.translate(diff, 0);
			avatarRect.translate(diff, 0);
			if (statusIconsOverAvatars_) {
				statusIconRect.translate(diff, 0);
			}
		}
		linesRect.setRight(linesRect.right() + diff);
		firstLineRect.setRight(linesRect.right());
		secondLineRect.setRight(linesRect.right());
	}
	// expanded. now align internals

	nickRect.setLeft(firstLineRect.left());
	nickRect.setRight(firstLineRect.right());

	// start drawing
	if(showAvatars_ && r.intersects(avatarRect)) {
		const QPixmap avatarPixmap = avatarIcon(index);
		if(!avatarPixmap.isNull()) {
			painter->drawPixmap(avatarRect, avatarPixmap);
		}
	}

	QPixmap statusPixmap = this->statusPixmap(index);
	if(!statusPixmap.isNull()) {
		if(statusIconsOverAvatars_ && showAvatars_) {
			statusPixmap = statusPixmap.scaled(statusIconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		} else {
			if (opt.direction == Qt::RightToLeft) {
				statusIconRect.moveRight(firstLineRect.right());
				nickRect.setRight(statusIconRect.left() - StatusIconToNickHMargin*PSI_HIDPI);
				secondLineRect.setRight(nickRect.right()); // we don't want status under icon
			} else {
				statusIconRect.moveLeft(firstLineRect.left());
				nickRect.setLeft(statusIconRect.right() + StatusIconToNickHMargin*PSI_HIDPI);
				secondLineRect.setLeft(nickRect.left()); // we don't want status under icon
			}
		}
		if (r.intersects(statusIconRect)) {
			painter->drawPixmap(statusIconRect, statusPixmap);
		}
	}
	statusLineRect.setLeft(secondLineRect.left());
	statusLineRect.setRight(secondLineRect.right());

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

	opt.font = font_;
	opt.font.setItalic(index.data(ContactListModel::BlockRole).toBool());
	opt.fontMetrics = fontMetrics_;
	QPalette palette = opt.palette;
	palette.setColor(QPalette::Text, textColor);
	opt.palette = palette;

	QString text = index.data(Qt::DisplayRole).toString();
	QString statusText = index.data(ContactListModel::StatusTextRole).toString();
	if (showStatusMessages_ && !statusText.isEmpty() && !statusSingle_) {
		text = tr("%1 (%2)").arg(text).arg(statusText);
	}
	drawText(painter, opt, nickRect, text);

	if (showStatusMessages_ && !statusText.isEmpty() && statusSingle_) {
		palette.setColor(QPalette::Text, ColorOpt::instance()->color("options.ui.look.colors.contactlist.status-messages"));
		opt.palette = palette;
		opt.font = statusFont_;
		opt.fontMetrics = statusFontMetrics_;
		painter->save();
		drawText(painter, opt, statusLineRect, statusText);
		painter->restore();
	}

	bool isMuc = index.data(ContactListModel::IsMucRole).toBool();
	QString mucMessages;
	if(isMuc)
		mucMessages = index.data(ContactListModel::MucMessagesRole).toString();

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

		if (showMoodIcons_) {
			Mood m = index.data(ContactListModel::MoodRole).value<Mood>();
			if (m.type() != Mood::Unknown) {
				const QPixmap &pix = IconsetFactory::iconPixmap(QString("mood/%1").arg(m.typeValue()));
				if(!pix.isNull()) {
					rightPixs.push_back(pix);
					rightWidths.push_back(pix.width());
				}
			}
		}

		if (showActivityIcons_) {
			QString icon = activityIconName(index.data(ContactListModel::ActivityRole).value<Activity>());
			if (!icon.isNull()) {
				const QPixmap &pix = IconsetFactory::iconPixmap(icon);
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
		sumWidth = fontMetrics_.width(mucMessages);
	else {
		foreach (int w, rightWidths) {
			sumWidth += w;
		}
		sumWidth = sumWidth*PSI_HIDPI;
		sumWidth+=rightPixs.count(); // gap 1px?
	}

	QRect gradRect(firstLineRect);
	pepIconsRect.setWidth(sumWidth);
	if (opt.direction == Qt::RightToLeft) {
		pepIconsRect.moveLeft(firstLineRect.left());
		gradRect.setRight(pepIconsRect.right() + NickConcealerWidth*PSI_HIDPI);
	} else {
		pepIconsRect.moveRight(firstLineRect.right());
		gradRect.setLeft(pepIconsRect.left() - NickConcealerWidth*PSI_HIDPI);
	}
	pepIconsRect &= firstLineRect;

	QColor bgc = (opt.state & QStyle::State_Selected) ? palette.color(QPalette::Highlight) : palette.color(QPalette::Base);
	QColor tbgc = bgc;
	tbgc.setAlpha(0);
	QLinearGradient grad;
	if (opt.direction == Qt::RightToLeft) {
		grad = QLinearGradient(gradRect.right(), 0, gradRect.right() - NickConcealerWidth*PSI_HIDPI, 0);
	} else {
		grad = QLinearGradient(gradRect.left(), 0, gradRect.left() + NickConcealerWidth*PSI_HIDPI, 0);
	}
	grad.setColorAt(0, tbgc);
	grad.setColorAt(1, bgc);
	QBrush tbakBr(grad);
	gradRect &= firstLineRect;
	if (gradRect.intersects(r)) {
		painter->fillRect(gradRect, tbakBr);
	}
	if (pepIconsRect.intersects(r)) {
		if(isMuc) {
			painter->drawText(pepIconsRect, mucMessages);
		}
		else {
			for (int i=0; i<rightPixs.size(); i++) {
				const QPixmap pix = rightPixs[i];
				pepIconsRect.setRight(pepIconsRect.right() - pix.width()*PSI_HIDPI -1); // 1 pep gap?
				QRect targetRect(pepIconsRect.topRight(),pix.size()*PSI_HIDPI);
				painter->drawPixmap(targetRect, pix, pix.rect());
				//qDebug() << r << pepIconsRect.topRight() << pix.size();
			}
		}
	}
}

void ContactListViewDelegate::Private::recomputeGeometry()
{
	// this function recompute just some parameters. others will be computed during rendering
	// when bounding rect is known. For now main unknown parameter is available width,
	// so compute for something small like 16px.

	bool haveSecondLine = showStatusMessages_ && statusSingle_;

	// lets starts from sizes of everything
	nickRect_.setSize(QSize(16, fontMetrics_.height()));

	int pepSize = 0;
	if (showMoodIcons_ && PsiIconset::instance()->moods.iconSize() > pepSize) {
		pepSize = PsiIconset::instance()->moods.iconSize();
	}
	if (showActivityIcons_ && PsiIconset::instance()->activities.iconSize() > pepSize) {
		pepSize = PsiIconset::instance()->activities.iconSize();
	}
	if (showClientIcons_ && PsiIconset::instance()->clients.iconSize() > pepSize) {
		pepSize = PsiIconset::instance()->clients.iconSize();
	}
	if ((showGeolocIcons_ || showTuneIcons_)  && PsiIconset::instance()->system().iconSize() > pepSize) {
		pepSize = PsiIconset::instance()->system().iconSize();
	}
	pepIconsRect_.setSize(QSize(0, pepSize*PSI_HIDPI)); // no icons for offline. so 0-width y default
	statusIconRect_.setSize(QSize(statusIconSize_, statusIconSize_)*PSI_HIDPI);

	// .. and sizes of a little more complex stuff
	firstLineRect_.setSize(QSize(
	    pepIconsRect_.width() + nickRect_.width() + (statusIconsOverAvatars_? 0 : StatusIconToNickHMargin*PSI_HIDPI + statusIconRect_.width()),
	    qMax(qMax(pepSize, nickRect_.height()), statusIconsOverAvatars_? 0: statusIconRect_.height())
	));

	if (haveSecondLine) {
		statusLineRect_.setSize(QSize(16, statusFontMetrics_.height())); // 16? I forgot why
		secondLineRect_.setHeight(statusLineRect_.height());
		secondLineRect_.setWidth(firstLineRect_.width()); // first line is wider y algo above. so use it
		linesRect_.setSize(QSize(firstLineRect_.width(), firstLineRect_.height() + NickToStatusLinesVMargin*PSI_HIDPI + secondLineRect_.height()));
	} else {
		secondLineRect_.setSize(QSize(0, 0));
		linesRect_.setSize(firstLineRect_.size());
	}

	if (showAvatars_) {
		if (statusIconsOverAvatars_) {
			statusIconRect_.setSize(QSize(12, 12)*PSI_HIDPI);
		}
		avatarStatusRect_.setSize(avatarRect_.size());
		// if we want status icon to a little go beyond the avatar then use QRect::united instead for avatarStatusRect_
		contactBoundingRect_.setSize(QSize(avatarStatusRect_.width() + AvatarToNickHMargin*PSI_HIDPI + linesRect_.width(),
		                                 avatarStatusRect_.height() > linesRect_.height()? avatarStatusRect_.height() : linesRect_.height()));
	} else {
		avatarStatusRect_.setSize(QSize(0, 0));
		contactBoundingRect_.setSize(linesRect_.size());
	}
	// all minimal sizes a known now

	// align everything vertical
	contactBoundingRect_.setTopLeft(QPoint(ContactHMargin*PSI_HIDPI,
	                                       ContactVMargin*PSI_HIDPI));
	int firstLineTop = 0;
	int secondLineGap = NickToStatusLinesVMargin*PSI_HIDPI;
	if (showAvatars_) {
		// we have to do some vertical align for avatar and lines to look nice
		int avatarStatusTop = 0;
		if (avatarStatusRect_.height() > linesRect_.height()) {
			// big avatar. try to center lines
			firstLineTop = (avatarStatusRect_.height() - linesRect_.height()) / 2;
			if (haveSecondLine) {
				int m = (avatarStatusRect_.height() - linesRect_.height()) / 3;
				if (m > NickToStatusLinesVMargin*PSI_HIDPI) { // if too much free space slide apart the lines as well
					firstLineTop = m;
					secondLineGap = m;
					linesRect_.setHeight(firstLineRect_.height() + m + secondLineRect_.height());
				}
			}
		} else if (avatarStatusRect_.height() < linesRect_.height()) {
			// big lines. center avatar
			avatarStatusTop = (linesRect_.height() - avatarStatusRect_.height()) / 2;
		}
		avatarStatusRect_.moveTop(contactBoundingRect_.top() + avatarStatusTop);
	}
	linesRect_.moveTop(contactBoundingRect_.top() + firstLineTop);
	firstLineRect_.moveTop(linesRect_.top());
	secondLineRect_.moveTop(firstLineRect_.bottom() + secondLineGap);

	// top-level containers are now aligned vertically. continue with horizontal
	if (showAvatars_) {
		if (avatarAtLeft_) {
			linesRect_.moveRight(contactBoundingRect_.right());
			avatarStatusRect_.moveLeft(contactBoundingRect_.left());
			if (statusIconsOverAvatars_) {
				statusIconRect_.moveBottomRight(avatarStatusRect_.bottomRight());
			}
		} else {
			linesRect_.moveLeft(contactBoundingRect_.left());
			avatarStatusRect_.moveRight(contactBoundingRect_.right()); // lines are the same width. so it does not matter which
			if (statusIconsOverAvatars_) {
				statusIconRect_.moveBottomLeft(avatarStatusRect_.bottomLeft());
			}
		}
		avatarRect_.moveTopLeft(avatarStatusRect_.topLeft());
	} else {
		linesRect_.moveLeft(contactBoundingRect_.left());
	}

	firstLineRect_.moveLeft(linesRect_.left());
	secondLineRect_.moveLeft(linesRect_.left());

	// top-level containers are now on their positions in our small emulation. align remaining internals now
	// We don't know anything about RTL atm so just align vertically.
	if (!showAvatars_ || !statusIconsOverAvatars_) {
		statusIconRect_.moveTop(firstLineRect_.top() + (firstLineRect_.height() - statusIconRect_.height()) / 2);
	}
	pepIconsRect_.moveTop(firstLineRect_.top() + (firstLineRect_.height() - pepIconsRect_.height()) / 2);
	nickRect_.moveTop(firstLineRect_.top() + (firstLineRect_.height() - nickRect_.height()) / 2);
	statusLineRect_.moveTop(secondLineRect_.top() + (secondLineRect_.height() - statusLineRect_.height()) / 2);

	emit geometryUpdated();
}

QSize ContactListViewDelegate::Private::sizeHint(const QModelIndex &index) const
{
	auto role = qvariant_cast<ContactListItem::Type>(index.data(ContactListModel::TypeRole));
	if (role == ContactListItem::Type::ContactType) {
		return contactBoundingRect_.size() + QSize(2*ContactHMargin, 2*ContactVMargin)*PSI_HIDPI;
	}
	int contentHeight;
	if (role == ContactListItem::Type::GroupType) {
		contentHeight = qMax(IconsetFactory::iconPtr("psi/groupOpen")->pixmap().height() * PSI_HIDPI,
		                     nickRect_.height());
	} else {
		contentHeight = qMax(showStatusIcons_? statusIconSize_ * PSI_HIDPI : 0, nickRect_.height());
	}
	return QSize(16, contentHeight + 2 * ContactVMargin*PSI_HIDPI);
}

int ContactListViewDelegate::Private::avatarSize() const
{
	return showAvatars_ ?
			qMax(avatarRect_.height() + 2 * ContactVMargin*PSI_HIDPI,
	             firstLineRect_.height()) : firstLineRect_.height();
}

void ContactListViewDelegate::Private::drawGroup(QPainter *painter, const QModelIndex &index)
{
#ifdef HAVE_QT5
	QStyleOptionViewItem o = opt;
#else
	QStyleOptionViewItemV2 o = opt;
#endif
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
		int s = painter->pen().width();
		gr.adjust(0,0,-s,-s);
		painter->drawRect(gr);
	}

	const QPixmap &pixmap = index.data(ContactListModel::ExpandedRole).toBool()
							? IconsetFactory::iconPtr("psi/groupOpen")->pixmap()
							: IconsetFactory::iconPtr("psi/groupClosed")->pixmap();

	QSize pixmapSize = pixmap.size()*PSI_HIDPI;
	QRect pixmapRect = relativeRect(opt, pixmapSize, QRect());
	r = relativeRect(opt, QSize(), pixmapRect, 3);
	pixmapRect.moveTop(opt.rect.top() + (opt.rect.height() - pixmapRect.height()) / 2);
	painter->drawPixmap(pixmapRect, pixmap);

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
#ifdef HAVE_QT5
	QStyleOptionViewItem o = opt;
#else
	QStyleOptionViewItemV2 o = opt;
#endif
	o.font = font_;
	o.fontMetrics = fontMetrics_;
	QPalette palette = o.palette;
	palette.setColor(QPalette::Base, _headerBackgroundColor);
	palette.setColor(QPalette::Text, _headerForegroundColor);
	o.palette = palette;

	drawBackground(painter, o, index);

	if (outlinedGroup_) {
		painter->setPen(QPen(_headerForegroundColor));
		QRect r(opt.rect);
		int s = painter->pen().width();
		r.adjust(0,0,-s,-s);
		painter->drawRect(r);
	}

	const QPixmap statusPixmap = this->statusPixmap(index);
	const QSize pixmapSize = statusPixmap.size() * PSI_HIDPI;
	QRect statusIconRect = relativeRect(o, pixmapSize, QRect());
	statusIconRect.moveTop(opt.rect.top() + (opt.rect.height() - statusIconRect.height()) / 2);
	QString text = index.data(Qt::DisplayRole).toString();
	QRect r = relativeRect(o, QSize(o.fontMetrics.width(text), o.rect.height()), statusIconRect, 3);
	painter->drawPixmap(statusIconRect, statusPixmap);

	drawText(painter, o, r, text);

	QPixmap sslPixmap = index.data(ContactListModel::UsingSSLRole).toBool()
						? IconsetFactory::iconPixmap("psi/cryptoYes")
						: IconsetFactory::iconPixmap("psi/cryptoNo");

	QSize sslPixmapSize = statusPixmap.size() * PSI_HIDPI;
	QRect sslRect = relativeRect(o, sslPixmapSize, r, 3);
	sslRect.moveTop(opt.rect.top() + (opt.rect.height() - sslRect.height()) / 2);
	painter->drawPixmap(sslRect, sslPixmap);
	r = relativeRect(opt, QSize(), sslRect, 3);

	text = QString("(%1/%2)")
		   .arg(index.data(ContactListModel::OnlineContactsRole).toInt())
		   .arg(index.data(ContactListModel::TotalContactsRole).toInt());
	drawText(painter, o, r, text);
}

void ContactListViewDelegate::Private::drawText(QPainter *painter, const QStyleOptionViewItem &opt, const QRect &rect, const QString &text)
{
	if (text.isEmpty())
		return;

	QRect rect2 = rect;
	rect2.moveTop(rect2.top() + (rect2.height() - opt.fontMetrics.height()) / 2);

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
#if 0 // we have gradient fadeout. So it seems unnecessary
	bool isElided = rect2.width() < opt.fontMetrics.width(text);
	if (isElided) {
		txt = opt.fontMetrics.elidedText(text, opt.textElideMode, rect2.width());
		painter->save();
		QRect txtRect(rect2);
		txtRect.setHeight(opt.fontMetrics.height());
		painter->setClipRect(txtRect);
	}
#endif
	painter->setFont(opt.font);
	QTextOption to;
	to.setWrapMode(QTextOption::NoWrap);
	if (opt.direction == Qt::RightToLeft)
		to.setAlignment(Qt::AlignRight);
	painter->drawText(rect2, txt, to);
#if 0
	if (isElided) {
		painter->restore();
	}
#endif
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
#ifdef HAVE_QT5
	opt.features = option.features;
#else
	const QStyleOptionViewItemV2 *v2 = qstyleoption_cast<const QStyleOptionViewItemV2 *>(&option);
	opt.features = v2 ? v2->features : QStyleOptionViewItemV2::ViewItemFeatures(QStyleOptionViewItemV2::None);
#endif

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
	d = new Private(this, parent);
	connect(d, SIGNAL(geometryUpdated()), SIGNAL(geometryUpdated()));
}

ContactListViewDelegate::~ContactListViewDelegate()
{
	delete d;
}

void ContactListViewDelegate::recomputeGeometry()
{
	d->recomputeGeometry();
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
	return d->avatarSize();
}

QSize ContactListViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option);

	if (!index.isValid())
		return QSize(0, 0);

	return d->sizeHint(index);
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
