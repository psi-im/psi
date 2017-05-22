#pragma once

#include "contactlistviewdelegate.h"

#include "contactlistview.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QList>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QTimer>
#include <QSet>

class ContactListViewDelegate::Private : public QObject
{
	Q_OBJECT

public:
    Private(ContactListViewDelegate *parent);
    ~Private();

public slots:
	void optionChanged(const QString &option);
	void updateAlerts();
	void updateAnim();
	void rosterIconsSizeChanged(int size);

public:
	virtual QPixmap statusPixmap(const QModelIndex &index);
	virtual QList<QPixmap> clientPixmap(const QModelIndex &index);
	virtual QPixmap avatarIcon(const QModelIndex &index);

	void drawContact(QPainter *painter, const QModelIndex &index);
	void drawGroup(QPainter *painter, const QModelIndex &index);
	void drawAccount(QPainter *painter, const QModelIndex &index);

	void drawText(QPainter *painter, const QStyleOptionViewItem &opt, const QRect &rect, const QString &text);
	void drawBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index);

	void setEditorCursorPosition(QWidget *editor, int cursorPosition);
	QColor backgroundColor(const QStyleOptionViewItem &option, const QModelIndex &index);

	void doSetOptions(const QStyleOptionViewItem &option, const QModelIndex &index);
	QRect getEditorGeometry(const QStyleOptionViewItem &option, const QModelIndex &index);

	void setAlertEnabled(const QModelIndex &index, bool enable);
	void setAnimEnabled(const QModelIndex &index, bool enable);

	ContactListViewDelegate *q;
	ContactListView *contactList;
	HoverableStyleOptionViewItem opt;
	QIcon::Mode  iconMode;
	QIcon::State iconState;

	int horizontalMargin_;
	int verticalMargin_;

	QTimer *alertTimer_;
	QTimer *animTimer;
	QFont font_;
	QFontMetrics fontMetrics_;
	bool statusSingle_;
	int rowHeight_;
	bool showStatusMessages_, slimGroup_, outlinedGroup_, showClientIcons_, showMoodIcons_, showActivityIcons_, showGeolocIcons_, showTuneIcons_;
	bool showAvatars_, useDefaultAvatar_, avatarAtLeft_, showStatusIcons_, statusIconsOverAvatars_;
	int avatarSize_, avatarRadius_;
	bool enableGroups_, allClients_;
	mutable QSet<QPersistentModelIndex> alertingIndexes;
	mutable QSet<QPersistentModelIndex> animIndexes;
	int statusIconSize_;
	int _nickIndent;
	bool animPhase;

	// Colors
	QColor _awayColor;
	QColor _dndColor;
	QColor _offlineColor;
	QColor _onlineColor;
	QColor _animation1Color;
	QColor _animation2Color;
	QColor _statusMessageColor;
	QColor _headerBackgroundColor;
	QColor _headerForegroundColor;
};
