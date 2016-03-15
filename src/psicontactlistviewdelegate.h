/*
 * psicontactlistviewdelegate.h
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

#ifndef PSICONTACTLISTVIEWDELEGATE_H
#define PSICONTACTLISTVIEWDELEGATE_H

#include <QTimer>

#include "contactlistviewdelegate.h"

class PsiContactListViewDelegate : public ContactListViewDelegate
{
	Q_OBJECT
public:
	static const int ContactVMargin = 1;
	static const int ContacHMargin = 1;
	static const int AvatarToNickHMargin = 3; // a gap between avatar and remaining data
	static const int NickToStatusLinesVMargin = 2;
	static const int StatusIconToNickHMargin = 3; // space between status icon and nickname
	static const int NickConcealerWidth = 10;

	PsiContactListViewDelegate(ContactListView* parent);
	~PsiContactListViewDelegate();

	void contactAlert(const QModelIndex& index);
	void clearAlerts();

	// reimplemented
	virtual int avatarSize() const;
	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;

protected:
	// reimplemented
	virtual void drawContact(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	virtual void drawGroup(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	virtual void drawAccount(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

	// reimplemented
	virtual void drawText(QPainter* painter, const QStyleOptionViewItem& o, const QRect& rect, const QString& text, const QModelIndex& index) const;

	// reimplemented
	virtual QRect nameRect(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	virtual QRect groupNameRect(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	virtual QRect editorRect(const QRect& nameRect) const;

	virtual QPixmap statusPixmap(const QModelIndex& index) const;
	virtual QList<QPixmap> clientPixmap(const QModelIndex& index) const;
	virtual QPixmap avatarIcon(const QModelIndex& index) const;

	virtual void recomputeGeometry();

private slots:
	void optionChanged(const QString& option);
	void updateAlerts();
	void rosterIconsSizeChanged(int size);

private:
	mutable QTimer alertTimer_;
	mutable QHash<QModelIndex, bool> alertingIndexes_;
	bool bulkOptUpdate;

	// options
	int avatarSize_, avatarRadius_, statusIconSize_;
	bool useDefaultAvatar_, avatarAtLeft_, statusIconsOverAvatars_;
	bool slimGroup_, outlinedGroup_;
	bool statusSingle_; // status text on its own line
	bool showStatusMessages_, showClientIcons_, showMoodIcons_, showActivityIcons_, showGeolocIcons_, showTuneIcons_;
	bool showAvatars_, showStatusIcons_;
	bool enableGroups_, allClients_;
	// end of options

	// computed from options values
	int contactRowHeight_;
	QFont font_, statusFont_;
	QFontMetrics *fontMetrics_, *statusFontMetrics_;
	QRect contactBoundingRect_;
	QRect avatarStatusRect_; // for avatar and status icon[optional]
	QRect linesRect_; // contains first and second lines. locates side by side with avatarStatusRect_
	QRect firstLineRect_; // first line: pepIconsRect_, nickRect_, statusIconRect_[optional]
	QRect secondLineRect_; // second line: statusLineRect_, any not implemented buttons like voice call
	QRect avatarRect_; // just for avatar. most likely square
	QRect statusIconRect_; // just for status icon
	QRect statusLineRect_;
	QRect pepIconsRect_;
	QRect nickRect_;
};

#endif
