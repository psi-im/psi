#ifndef OPTIONSTAB_H
#define OPTIONSTAB_H

#include <QString>
#include <QObject>
#include <QByteArray>

class PsiIcon;
class QWidget;
class PsiCon;

class OptionsTab : public QObject
{
	Q_OBJECT
public:
	OptionsTab(QObject *parent, const char *name = 0);
	OptionsTab(QObject *parent, QByteArray id, QByteArray parentId, QString name, QString desc, QString tabIconName = QString::null, QString iconName = QString::null);
	~OptionsTab();

	virtual QByteArray id() const;		// Unique identifier, i.e. "plugins_misha's_cool-plugin"
	virtual QByteArray parentId() const;	// Identifier of parent tab, i.e. "general"

	virtual QString tabName() const;	// "General"
	virtual PsiIcon *tabIcon() const;		// default implementation returns 0

	virtual QString name() const;		// "Roster"
	virtual QString desc() const;		// "You can configure your roster here"
	virtual PsiIcon *psiIcon() const;		// default implementation returns 0

	virtual QWidget *widget() = 0;		// Actual widget that contains checkboxes, pushbuttons, etc.
						// the widget is reparented after this call
	virtual bool stretchable() const;	// return 'true' if widget() is stretchable and wants a lot of space

signals:
	void dataChanged();
	//void addWidgetChangedSignal(QString widgetName, QCString signal);
	void noDirty(bool);
	void connectDataChanged(QWidget *);

public slots:
	virtual void setData(PsiCon *, QWidget *parentDialog);
	virtual void applyOptions();
	virtual void restoreOptions();
	virtual void tabAdded(OptionsTab *tab);	// called when tab 'tab' specifies this tab as parent

private:
	QByteArray v_id, v_parentId;
	QString v_name, v_desc, v_tabIconName, v_iconName;
};

class MetaOptionsTab : public OptionsTab
{
	Q_OBJECT
public:
	MetaOptionsTab(QObject *parent, const char *name = 0);
	MetaOptionsTab(QObject *parent, QByteArray id, QByteArray parentId, QString name, QString desc, QString tabIconName = QString::null, QString iconName = QString::null);
	~MetaOptionsTab();

	QWidget *widget();
	void applyOptions();
	void restoreOptions();

	void setData(PsiCon *, QWidget *);
	bool stretchable() const { return true; }

	void addTab(OptionsTab *);

public slots:
	void enableOtherTabs(bool);

private:
	void init();
	QWidget *w;
	QList<OptionsTab*> tabs;
};

#endif
