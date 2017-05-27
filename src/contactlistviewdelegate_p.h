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
    Private(ContactListViewDelegate *parent, ContactListView *contactList);
    ~Private();

signals:
	void geometryUpdated();

public slots:
	void optionChanged(const QString &option);
	void updateAlerts();
	void updateAnim();
	void rosterIconsSizeChanged(int size);

public:
	void recomputeGeometry();
	QSize sizeHint(const QModelIndex &index) const;
	int avatarSize() const;

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

public:
	static const int ContactVMargin = 2;
	static const int ContactHMargin = 2;
	static const int AvatarToNickHMargin = 6; // a gap between avatar and remaining data
	static const int NickToStatusLinesVMargin = 2;
	static const int StatusIconToNickHMargin = 3; // space between status icon and nickname
	static const int NickConcealerWidth = 10;
	static const int PepIconsGap = 1;

	ContactListViewDelegate *q;
	ContactListView *contactList;
	HoverableStyleOptionViewItem opt;
	QIcon::Mode  iconMode;
	QIcon::State iconState;

	int horizontalMargin_;
	int verticalMargin_;
	int statusIconSize_;
	int avatarRadius_;

	QTimer *alertTimer_;
	QTimer *animTimer;
	QFont font_, statusFont_;
	QFontMetrics fontMetrics_, statusFontMetrics_;
	bool statusSingle_;
	bool showStatusMessages_, slimGroup_, outlinedGroup_, showClientIcons_, showMoodIcons_, showActivityIcons_, showGeolocIcons_, showTuneIcons_;
	bool showAvatars_, useDefaultAvatar_, avatarAtLeft_, showStatusIcons_, statusIconsOverAvatars_;
	bool enableGroups_, allClients_;
	bool animPhase;
	bool bulkOptUpdate = false;
	mutable QSet<QPersistentModelIndex> alertingIndexes;
	mutable QSet<QPersistentModelIndex> animIndexes;

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

	// Geometry
	QRect contactBoundingRect_;
	QRect avatarStatusRect_;
	QRect linesRect_;
	QRect firstLineRect_;
	QRect secondLineRect_;
	QRect avatarRect_;
	QRect statusIconRect_;
	QRect statusLineRect_;
	QRect pepIconsRect_;
	QRect nickRect_;
};
