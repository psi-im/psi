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

static const QString contactListFontOptionPath = "options.ui.look.font.contactlist";
static const QString slimGroupsOptionPath = "options.ui.look.contactlist.use-slim-group-headings";
static const QString outlinedGroupsOptionPath = "options.ui.look.contactlist.use-outlined-group-headings";
static const QString contactListBackgroundOptionPath = "options.ui.look.colors.contactlist.background";
static const QString showStatusMessagesOptionPath = "options.ui.contactlist.status-messages.show";
static const QString statusSingleOptionPath = "options.ui.contactlist.status-messages.single-line";

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
	optionChanged(slimGroupsOptionPath);
	optionChanged(outlinedGroupsOptionPath);
	optionChanged(contactListFontOptionPath);
	optionChanged(contactListBackgroundOptionPath);
	optionChanged(showStatusMessagesOptionPath);
	optionChanged(statusSingleOptionPath);
}

PsiContactListViewDelegate::~PsiContactListViewDelegate()
{
	delete font_;
	delete fontMetrics_;
}

int PsiContactListViewDelegate::avatarSize() const
{
	return rowHeight_;
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

void PsiContactListViewDelegate::drawContact(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	drawBackground(painter, option, index);

	QRect r = option.rect;

	QRect avatarRect(r);
	const QPixmap statusPixmap = this->statusPixmap(index);
	avatarRect.translate(1, 1);
	avatarRect.setSize(statusPixmap.size());
	painter->drawPixmap(avatarRect.topLeft(), statusPixmap);

	r.setLeft(avatarRect.right() + 3);

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
			drawText(painter, o, txtRect, statusMsg, index);
		}
	}
	else {
		if(showStatusMessages_ && statusSingle_)
			r.setHeight(r.height()*2/3);

		drawText(painter, o, r, text, index);
	}

#if 0
	int x;
	if (d->status_single)
		x = widthUsed();
	else {
		QFontMetrics fm(p->font());
		const QPixmap *pix = pixmap(column);
		x = fm.width(text(column)) + (pix ? pix->width() : 0) + 8;
	}

	if (d->u) {
		UserResourceList::ConstIterator it = d->u->priority();
		if (it != d->u->userResourceList().end()) {
			if (d->u->isSecure((*it).name())) {
				const QPixmap &pix = IconsetFactory::iconPixmap("psi/cryptoYes");
				int y = (height() - pix.height()) / 2;
				p->drawPixmap(x, y, pix);
				x += 24;
			}
		}
	}
#endif
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

	QRect pixmapRect(r);
	pixmapRect.translate(1, 1);
	pixmapRect.setSize(pixmap.size());
	painter->drawPixmap(pixmapRect.topLeft(), pixmap);

	r.setLeft(pixmapRect.right() + 3);

	QString text = index.data(Qt::ToolTipRole).toString();
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

	QRect r = option.rect;
	if (outlinedGroup_) {
		painter->setPen(QPen(foreground));
		painter->drawRect(r);
	}

	QRect avatarRect(r);
	const QPixmap statusPixmap = this->statusPixmap(index);
	avatarRect.translate(1, 1);
	avatarRect.setSize(statusPixmap.size());
	painter->drawPixmap(avatarRect.topLeft(), statusPixmap);

	r.setLeft(avatarRect.right() + 3);

	QString text = nameText(o, index);
	r.setRight(r.left() + o.fontMetrics.width(text));
	drawText(painter, o, r, text, index);

	QPixmap sslPixmap = index.data(ContactListModel::UsingSSLRole).toBool() ?
	                    IconsetFactory::iconPixmap("psi/cryptoYes") :
	                    IconsetFactory::iconPixmap("psi/cryptoNo");
	QRect sslRect(option.rect);
	sslRect.setLeft(r.right() + 3);
	sslRect.translate(1, 1);
	sslRect.setSize(sslPixmap.size());
	painter->drawPixmap(sslRect.topLeft(), sslPixmap);

	r.setLeft(sslRect.right() + 3);
	r.setRight(option.rect.right());

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
		rowHeight_ = qMax(fontMetrics_->height()+2, 18); // 18 - default row height
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
