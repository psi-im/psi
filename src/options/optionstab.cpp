#include "optionstab.h"
#include "iconset.h"

#include <QTabWidget>
#include <QLayout>
#include <QMap>
#include <QVBoxLayout>

//----------------------------------------------------------------------------
// OptionsTab
//----------------------------------------------------------------------------

OptionsTab::OptionsTab(QObject *parent, const char *name)
: QObject(parent)
{
	setObjectName(name);
}

OptionsTab::OptionsTab(QObject *parent, QByteArray _id, QByteArray _parentId, QString _name, QString _desc, QString _tabIconName, QString _iconName)
: QObject(parent)
{
	setObjectName(_name);
	v_id = _id;
	v_parentId = _parentId;
	v_name = _name;
	v_desc = _desc;
	v_tabIconName = _tabIconName;
	v_iconName = _iconName;
}

OptionsTab::~OptionsTab()
{
}

QByteArray OptionsTab::id() const
{
	return v_id;
}

QByteArray OptionsTab::parentId() const
{
	return v_parentId;
}

QString OptionsTab::tabName() const
{
	return v_name;
}

PsiIcon *OptionsTab::tabIcon() const
{
	if ( v_tabIconName.isEmpty() )
		return 0;

	return (PsiIcon *)IconsetFactory::iconPtr( v_tabIconName );
}

QString OptionsTab::name() const
{
	return v_name;
}

QString OptionsTab::desc() const
{
	return v_desc;
}

PsiIcon *OptionsTab::psiIcon() const
{
	if ( v_iconName.isEmpty() ) {
		//if ( tabIcon() )
		//	return tabIcon();

		return (PsiIcon *)IconsetFactory::iconPtr("psi/logo_32");
	}

	return (PsiIcon *)IconsetFactory::iconPtr( v_iconName );
}

void OptionsTab::applyOptions()
{
}

void OptionsTab::restoreOptions()
{
}

void OptionsTab::tabAdded(OptionsTab *)
{
}

bool OptionsTab::stretchable() const
{
	return false;
}

void OptionsTab::setData(PsiCon *, QWidget *)
{
}

//----------------------------------------------------------------------------
// OptionsTabWidget
//----------------------------------------------------------------------------

class OptionsTabWidget : public QTabWidget
{
	Q_OBJECT
public:
	OptionsTabWidget(QWidget *parent);
	void addTab(OptionsTab *);
	void restoreOptions();
	void enableOtherTabs(bool);

signals:
	void connectDataChanged(QWidget *);
	void noDirty(bool);

private slots:
	void updateCurrent(int);

private:
	struct TabData {
		TabData() { tab = 0; initialized = false; }
		TabData(OptionsTab *t) { tab = t; initialized = false; }
		OptionsTab *tab;
		bool initialized;
	};
	QMap<QWidget *, TabData> w2tab;
};

OptionsTabWidget::OptionsTabWidget(QWidget *parent)
: QTabWidget(parent)
{
	connect(this, SIGNAL(currentChanged(int)), SLOT(updateCurrent(int)));
}

void OptionsTabWidget::addTab(OptionsTab *tab)
{
	if ( tab->tabName().isEmpty() )
		return; // skip the dummy tabs

	// the widget will have no parent; it will be reparented
	// when inserting it with "addTab"
	QWidget *w = new QWidget(0);
	w->setObjectName(tab->name());

	if ( !tab->desc().isEmpty() )
		setTabToolTip(indexOf(w), tab->desc());

	w2tab[w] = TabData(tab);

	if ( tab->tabIcon() )
		QTabWidget::addTab(w, tab->tabIcon()->icon(), tab->tabName());
	else
		QTabWidget::addTab(w, tab->tabName());

	setCurrentIndex(0);
}

void OptionsTabWidget::updateCurrent(int index)
{
	Q_UNUSED(index)
	QWidget *w = currentWidget ();
	if ( !w2tab[w].initialized ) {
		QVBoxLayout *vbox = new QVBoxLayout(w);
		vbox->setMargin(5);
		OptionsTab *opttab = w2tab[w].tab;

		QWidget *tab = opttab->widget();
		if ( !tab )
			return;

		tab->setParent(w);
		vbox->addWidget(tab);
		if ( !opttab->stretchable() )
			vbox->addStretch();

		emit noDirty(true);
		opttab->restoreOptions();
		emit noDirty(false);

		emit connectDataChanged(tab);

		tab->show();
		w2tab[w].initialized = true;
	}
}

void OptionsTabWidget::restoreOptions()
{
	emit noDirty(true);
	w2tab[currentWidget()].tab->restoreOptions();
	emit noDirty(false);
}

void OptionsTabWidget::enableOtherTabs(bool enable)
{
	for (int i = 0; i < count(); i++)
	{
		if (i != currentIndex())
			setTabEnabled(i, enable);
	}
}

//----------------------------------------------------------------------------
// MetaOptionsTab
//----------------------------------------------------------------------------

MetaOptionsTab::MetaOptionsTab(QObject *parent, const char *name)
: OptionsTab(parent, name)
{
	init();
}

MetaOptionsTab::MetaOptionsTab(QObject *parent, QByteArray id, QByteArray parentId, QString name, QString desc, QString tabIconName, QString iconName)
: OptionsTab(parent, id, parentId, name, desc, tabIconName, iconName)
{
	init();
}

MetaOptionsTab::~MetaOptionsTab()
{
	qDeleteAll(tabs);

	//if ( w )   // it does not make much sense to delete it. since the widget will reparented and deleted automatically
	//	delete w;
}

void MetaOptionsTab::init()
{
	w = 0;
}

void MetaOptionsTab::addTab(OptionsTab *tab)
{
	connect(tab, SIGNAL(dataChanged()), SIGNAL(dataChanged()));
	//connect(tab, SIGNAL(addWidgetChangedSignal(QString, QCString)), SIGNAL(addWidgetChangedSignal(QString, QCString)));
	connect(tab, SIGNAL(noDirty(bool)), SIGNAL(noDirty(bool)));
	connect(tab, SIGNAL(connectDataChanged(QWidget *)), SIGNAL(connectDataChanged(QWidget *)));

	tabs.append(tab);
}

void MetaOptionsTab::enableOtherTabs(bool enable)
{
	if (!w)
		return;
	static_cast<OptionsTabWidget*>(w)->enableOtherTabs(enable);
}

QWidget *MetaOptionsTab::widget()
{
	if ( w )
		return w;

	OptionsTabWidget *t = new OptionsTabWidget(0);
	w = t;

	connect(w, SIGNAL(connectDataChanged(QWidget *)), SIGNAL(connectDataChanged(QWidget *)));
	connect(w, SIGNAL(noDirty(bool)), SIGNAL(noDirty(bool)));

	foreach(OptionsTab* tab, tabs) {
		t->addTab(tab);
	}

	// set the current widget to 0, otherwise qt4 will show no widget
	t->setCurrentIndex(0);

	return w;
}

void MetaOptionsTab::applyOptions()
{
	foreach(OptionsTab* tab, tabs) {
		tab->applyOptions();
	}
}

void MetaOptionsTab::restoreOptions()
{
	if ( w ) {
		OptionsTabWidget *d = (OptionsTabWidget *)w;
		d->restoreOptions();
	}

	foreach(OptionsTab* tab, tabs) {
		tab->restoreOptions();
	}
}

void MetaOptionsTab::setData(PsiCon *psi, QWidget *w)
{
	foreach(OptionsTab* tab, tabs) {
		tab->setData(psi, w);
	}
}

#include "optionstab.moc"
