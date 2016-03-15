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

PsiContactListViewDelegate::PsiContactListViewDelegate(ContactListView* parent) :
    ContactListViewDelegate(parent),
    fontMetrics_(0),
    statusFontMetrics_(0)
{
	alertTimer_.setInterval(100);
	alertTimer_.setSingleShot(false);
	connect(&alertTimer_, SIGNAL(timeout()), SLOT(updateAlerts()));

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
	contactList()->viewport()->update();
}

PsiContactListViewDelegate::~PsiContactListViewDelegate()
{
	delete fontMetrics_;
	delete statusFontMetrics_;
}

int PsiContactListViewDelegate::avatarSize() const
{
	return showAvatars_ ?
		qMax(avatarRect_.height() + 2 * ContactVMargin, firstLineRect_.height()) : firstLineRect_.height();
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
				alertTimer_.start();
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
		//qDebug("DRAW: %s FOR %s", qPrintable(client), qPrintable(index.data(ContactListModel::JidRole).toString()));
		const QPixmap &pix = IconsetFactory::iconPixmap("clients/" + client);
		if(!pix.isNull())
			pixList.push_back(pix);
	}

	return pixList;
}

QPixmap PsiContactListViewDelegate::avatarIcon(const QModelIndex& index) const
{
	int avSize = showAvatars_ ? avatarRect_.height() : 0;
	QPixmap av = index.data(ContactListModel::IsMucRole).toBool() ? QPixmap() : index.data(ContactListModel::AvatarRole).value<QPixmap>();
	if(av.isNull() && useDefaultAvatar_)
		av = IconsetFactory::iconPixmap("psi/default_avatar");

	return AvatarFactory::roundedAvatar(av, avatarRadius_, avSize);
}

QSize PsiContactListViewDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
	if (index.isValid()) {
		if(index.data(ContactListModel::TypeRole) == ContactListModel::ContactType) {
			return contactBoundingRect_.size() + QSize(2*ContacHMargin, 2*ContactVMargin);
		} else {
			return QSize(16, qMax(showStatusIcons_? statusIconRect_.height() : 0, nickRect_.height() + 2 * ContactVMargin));
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
	/* We have few possible ways to draw contact
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


	drawBackground(painter, option, index);

	QRect r = option.rect; // our full contact space to draw

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
	if (contactBoundingRect.width() + 2 * ContacHMargin < r.width()) {
		// our previously computed minimal rect is too small for this roster. so expand
		int diff = r.width() - (contactBoundingRect.width() + 2 * ContacHMargin);
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
			if (option.direction == Qt::RightToLeft) {
				statusIconRect.setRight(firstLineRect.right());
				nickRect.setRight(statusIconRect.right() - StatusIconToNickHMargin);
				secondLineRect.setRight(nickRect.right()); // we don't want status under icon
			} else {
				statusIconRect.setLeft(firstLineRect.left());
				nickRect.setLeft(statusIconRect.right() + StatusIconToNickHMargin);
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
	o.font = font_;
	o.font.setItalic(index.data(ContactListModel::BlockRole).toBool());
	o.fontMetrics = *fontMetrics_;
	QPalette palette = o.palette;
	palette.setColor(QPalette::Text, textColor);
	o.palette = palette;

	QString text = nameText(o, index);
	QString status = statusText(index);
	if (showStatusMessages_ && !status.isEmpty() && !statusSingle_) {
		text = tr("%1 (%2)").arg(text).arg(statusText(index));
	}
	drawText(painter, o, nickRect, text, index);

	if (showStatusMessages_ && !status.isEmpty() && statusSingle_) {
		palette.setColor(QPalette::Text, ColorOpt::instance()->color("options.ui.look.colors.contactlist.status-messages"));
		o.palette = palette;
		o.font = statusFont_;
		o.fontMetrics = *statusFontMetrics_;
		painter->save();
		drawText(painter, o, statusLineRect, status, index);
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

	QRect gradRect(firstLineRect);
	pepIconsRect.setWidth(sumWidth);
	if (option.direction == Qt::RightToLeft) {
		pepIconsRect.moveLeft(firstLineRect.left());
		gradRect.setRight(pepIconsRect.right() + NickConcealerWidth);
	} else {
		pepIconsRect.moveRight(firstLineRect.right());
		gradRect.setLeft(pepIconsRect.left() - NickConcealerWidth);
	}
	pepIconsRect &= firstLineRect;

	QColor bgc = (option.state & QStyle::State_Selected) ? palette.color(QPalette::Highlight) : palette.color(QPalette::Base);
	QColor tbgc = bgc;
	tbgc.setAlpha(0);
	QLinearGradient grad;
	if (option.direction == Qt::RightToLeft) {
		grad = QLinearGradient(gradRect.right(), 0, gradRect.right() - NickConcealerWidth, 0);
	} else {
		grad = QLinearGradient(gradRect.left(), 0, gradRect.left() + NickConcealerWidth, 0);
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
				pepIconsRect.setRight(pepIconsRect.right() - pix.width() -1);
				painter->drawPixmap(pepIconsRect.topRight(), pix);
				//qDebug() << r << pepIconsRect.topRight() << pix.size();
			}
		}
	}
}

void PsiContactListViewDelegate::drawGroup(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItemV2 o = option;
	o.font = font_;
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
	o.font = font_;
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

void PsiContactListViewDelegate::recomputeGeometry()
{
	// this function recompute just some parameters. others will be computed during rendering
	// when bounding rect is known. For now main unknown parameter is available width,
	// so compute for something small like 16px.

	bool haveSecondLine = showStatusMessages_ && statusSingle_;

	// lets starts from sizes of everything
	nickRect_.setSize(QSize(16, fontMetrics_->height()));

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
	pepIconsRect_.setSize(QSize(0, pepSize)); // no icons for offline. so 0-width y default
	statusIconRect_.setSize(QSize(statusIconSize_, statusIconSize_));
	// avatarRect_.setSize(avatarSize_, avatarSize_); // not needed. set by other functions

	// .. and sizes of a little more complex stuff
	firstLineRect_.setSize(QSize(
	    pepIconsRect_.width() + nickRect_.width() + (statusIconsOverAvatars_? 0 : StatusIconToNickHMargin + statusIconRect_.width()),
	    qMax(qMax(pepSize, nickRect_.height()), statusIconsOverAvatars_? 0: statusIconRect_.height())
	));

	if (haveSecondLine) {
		statusLineRect_.setSize(QSize(16, statusFontMetrics_->height()));
		secondLineRect_.setHeight(statusLineRect_.height());
		secondLineRect_.setWidth(firstLineRect_.width()); // first line is wider y algo above. so use it
		linesRect_.setSize(QSize(firstLineRect_.width(), firstLineRect_.height() + NickToStatusLinesVMargin + secondLineRect_.height()));
	} else {
		secondLineRect_.setSize(QSize(0, 0));
		linesRect_.setSize(firstLineRect_.size());
	}

	if (showAvatars_) {
		if (statusIconsOverAvatars_) {
			statusIconRect_.setSize(QSize(12, 12));
		}
		avatarStatusRect_.setSize(avatarRect_.size());
		// if we want status icon to a little go beyond the avatar then use QRect::united instead for avatarStatusRect_
		contactBoundingRect_.setSize(QSize(avatarStatusRect_.width() + AvatarToNickHMargin + linesRect_.width(),
		                                 avatarStatusRect_.height() > linesRect_.height()? avatarStatusRect_.height() : linesRect_.height()));
	} else {
		avatarStatusRect_.setSize(QSize(0, 0));
		contactBoundingRect_.setSize(linesRect_.size());
	}
	// all minimal sizes a known now

	// align everything vertical
	contactBoundingRect_.setTopLeft(QPoint(ContacHMargin, ContactVMargin));
	int firstLineTop = 0;
	int secondLineGap = NickToStatusLinesVMargin;
	if (showAvatars_) {
		// we have to do some vertical align for avatar and lines to look nice
		int avatarStatusTop = 0;
		if (avatarStatusRect_.height() > linesRect_.height()) {
			// big avatar. try to center lines
			firstLineTop = (avatarStatusRect_.height() - linesRect_.height()) / 2;
			if (haveSecondLine) {
				int m = (avatarStatusRect_.height() - linesRect_.height()) / 3;
				if (m > NickToStatusLinesVMargin) { // if too much free space slide apart the lines as well
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
}

void PsiContactListViewDelegate::optionChanged(const QString& option)
{
	bool updateGeometry = false;
	bool updateViewport = false;

	if (option == contactListFontOptionPath) {
		font_.fromString(PsiOptions::instance()->getOption(contactListFontOptionPath).toString());
		delete fontMetrics_;
		delete statusFontMetrics_;
		fontMetrics_ = new QFontMetrics(font_);
		statusFont_.setPointSize(qMax(font_.pointSize()-2, 7));
		statusFontMetrics_ = new QFontMetrics(statusFont_);

		updateGeometry = true;
	}
	else if (option == contactListBackgroundOptionPath) {
		QPalette p = contactList()->palette();
		p.setColor(QPalette::Base, ColorOpt::instance()->color(contactListBackgroundOptionPath));
		const_cast<ContactListView*>(contactList())->setPalette(p);
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
		updateViewport = true;
	}
	else if(option == avatarSizeOptionPath) {
	    int s = PsiOptions::instance()->getOption(avatarSizeOptionPath).toInt();
		avatarRect_.setSize(QSize(s, s));
		updateGeometry = true;
	}
	else if(option == avatarRadiusOptionPath) {
		avatarRadius_ = PsiOptions::instance()->getOption(avatarRadiusOptionPath).toInt();
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
			contactList()->viewport()->update();
		}
	}
}

void PsiContactListViewDelegate::rosterIconsSizeChanged(int size)
{
	statusIconSize_ = size;
	recomputeGeometry();
	contactList()->viewport()->update();
}

void PsiContactListViewDelegate::drawText(QPainter* painter, const QStyleOptionViewItem& o, const QRect& rect, const QString& text, const QModelIndex& index) const
{
	QRect r(rect);
	r.moveTop(r.top() + (r.height() - o.fontMetrics.height()) / 2);
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
		alertTimer_.stop();
	else
		alertTimer_.start();
}

void PsiContactListViewDelegate::clearAlerts()
{
	alertingIndexes_.clear();
	alertTimer_.stop();
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
