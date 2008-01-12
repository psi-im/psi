#ifndef OPT_ICONSET_H
#define OPT_ICONSET_H

#include "optionstab.h"
//Added by qt3to4:
#include <QEvent>

class QWidget;
class QListWidgetItem;
class IconsetLoadThread;
class QTreeWidgetItem;

class OptionsTabIconsetSystem : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabIconsetSystem(QObject *parent);
	~OptionsTabIconsetSystem();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();
	bool stretchable() const { return true; }

private slots:
	void setData(PsiCon *, QWidget *);
	void previewIconset();

protected:
	bool event(QEvent *);
	void cancelThread();

private:
	QWidget *w, *parentWidget;

	int numIconsets, iconsetsLoaded;
	IconsetLoadThread *thread;
};

class OptionsTabIconsetEmoticons : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabIconsetEmoticons(QObject *parent);
	~OptionsTabIconsetEmoticons();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();
	bool stretchable() const { return true; }

private slots:
	void setData(PsiCon *, QWidget *);
	void previewIconset();

protected:
	bool event(QEvent *);
	void cancelThread();

private:
	QWidget *w, *parentWidget;

	int numIconsets, iconsetsLoaded;
	IconsetLoadThread *thread;
};

class OptionsTabIconsetRoster : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabIconsetRoster(QObject *parent);
	~OptionsTabIconsetRoster();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();
	bool stretchable() const { return true; }

private slots:
	void setData(PsiCon *, QWidget *);

	void defaultDetails();
	void servicesDetails();
	void customDetails();

	void isServices_iconsetSelected(QListWidgetItem *current, QListWidgetItem *previous);
	void isServices_selectionChanged(QTreeWidgetItem *);

	void isCustom_iconsetSelected(QListWidgetItem *current, QListWidgetItem *previous);
	void isCustom_selectionChanged(QTreeWidgetItem *);
	void isCustom_textChanged();
	void isCustom_add();
	void isCustom_delete();
	QString clipCustomText(QString);

protected:
	bool event(QEvent *);
	void cancelThread();

private:
	QWidget *w, *parentWidget;

	int numIconsets, iconsetsLoaded;
	IconsetLoadThread *thread;
};

#endif
