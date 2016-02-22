#ifndef OPT_ICONSET_H
#define OPT_ICONSET_H

#include "optionstab.h"
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
	PsiCon *psi;

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
	PsiCon *psi;

	int numIconsets, iconsetsLoaded;
	IconsetLoadThread *thread;
};

class OptionsTabIconsetMoods : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabIconsetMoods(QObject *parent);
	~OptionsTabIconsetMoods();

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
	PsiCon *psi;

	int numIconsets, iconsetsLoaded;
	IconsetLoadThread *thread;
};

class OptionsTabIconsetActivity : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabIconsetActivity(QObject *parent);
	~OptionsTabIconsetActivity();

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
	PsiCon *psi;

	int numIconsets, iconsetsLoaded;
	IconsetLoadThread *thread;
};

class OptionsTabIconsetClients : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabIconsetClients(QObject *parent);
	~OptionsTabIconsetClients();

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
	PsiCon *psi;

	int numIconsets, iconsetsLoaded;
	IconsetLoadThread *thread;
};

class OptionsTabIconsetAffiliations : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabIconsetAffiliations(QObject *parent);
	~OptionsTabIconsetAffiliations();

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
	PsiCon *psi;

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
	QWidget* w;
	QWidget* parentWidget;
	PsiCon *psi;
	int numIconsets, iconsetsLoaded;
	IconsetLoadThread *thread;

	enum {
		IconsetRole = Qt::UserRole + 0,
		ServiceRole = Qt::UserRole + 1,
		RegexpRole  = Qt::UserRole + 2
	};

	void addService(const QString& id, const QString& name);
};

#endif
