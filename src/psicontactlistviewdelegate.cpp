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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "psicontactlistviewdelegate.h"

#include <QPainter>

#include "psiiconset.h"
#include "psioptions.h"
#include "contactlistview.h"
#include "common.h"

static const QString contactListFontOptionPath = "options.ui.look.font.contactlist";
static const QString contactListBackgroundOptionPath = "options.ui.look.colors.contactlist.background";
static const QString showStatusMessagesOptionPath = "options.ui.contactlist.status-messages.show";

PsiContactListViewDelegate::PsiContactListViewDelegate(ContactListView* parent)
	: ContactListViewDelegate(parent)
	, font_(0)
	, fontMetrics_(0)
{
	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	optionChanged(contactListFontOptionPath);
	optionChanged(contactListBackgroundOptionPath);
	optionChanged(showStatusMessagesOptionPath);
}

PsiContactListViewDelegate::~PsiContactListViewDelegate()
{
	delete font_;
	delete fontMetrics_;
}

int PsiContactListViewDelegate::avatarSize() const
{
	return 18;
}

QPixmap PsiContactListViewDelegate::statusPixmap(const QModelIndex& index) const
{
	int s = statusType(index);
	ContactListModel::Type type = ContactListModel::indexType(index);
	if (type == ContactListModel::ContactType ||
	    type == ContactListModel::AccountType)
	{
		if (index.data(ContactListModel::IsAlertingRole).toBool()) {
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

	QColor textColor = PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.online").value<QColor>();
	if (statusType(index) == XMPP::Status::Away || statusType(index) == XMPP::Status::XA)
		textColor = PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.away").value<QColor>();
	else if (statusType(index) == XMPP::Status::DND)
		textColor = PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.do-not-disturb").value<QColor>();
	else if (statusType(index) == XMPP::Status::Offline)
		textColor = PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.offline").value<QColor>();

#if 0
	if (d->animatingNick) {
		textColor = d->animateNickColor ? PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color1").value<QColor>() : PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color2").value<QColor>();
		xcg.setColor(QColorGroup::HighlightedText, d->animateNickColor ? PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color1").value<QColor>() : PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color2").value<QColor>());
	}
#endif

	QStyleOptionViewItemV2 o = option;
	o.font = *font_;
	o.fontMetrics = *fontMetrics_;
	QPalette palette = o.palette;
	palette.setColor(QPalette::Text, textColor);
	o.palette = palette;

	QString text = nameText(o, index);
	if (showStatusMessages_ && !statusText(index).isEmpty()) {
		text = tr("%1 (%2)").arg(text).arg(statusText(index));
	}

	drawText(painter, o, r, text, index);

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
	palette.setColor(QPalette::Base, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>());
	palette.setColor(QPalette::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-foreground").value<QColor>());
	o.palette = palette;

	drawBackground(painter, o, index);

	QRect r = option.rect;

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
}

void PsiContactListViewDelegate::drawAccount(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItemV2 o = option;
	o.font = *font_;
	o.fontMetrics = *fontMetrics_;
	QPalette palette = o.palette;
	palette.setColor(QPalette::Base, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.profile.header-background").value<QColor>());
	palette.setColor(QPalette::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.profile.header-foreground").value<QColor>());
	o.palette = palette;

	drawBackground(painter, o, index);

	QRect r = option.rect;

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
		contactList()->viewport()->update();
	}
	else if (option == contactListBackgroundOptionPath) {
		QPalette p = contactList()->palette();
		p.setColor(QPalette::Base, PsiOptions::instance()->getOption(contactListBackgroundOptionPath).value<QColor>());
		const_cast<ContactListView*>(contactList())->setPalette(p);
	}
	else if (option == showStatusMessagesOptionPath) {
		showStatusMessages_ = PsiOptions::instance()->getOption(showStatusMessagesOptionPath).toBool();
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
