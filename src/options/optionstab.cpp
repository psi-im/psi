#include "optionstab.h"
#include "iconset.h"

#include <qtabwidget.h>
#include <qlayout.h>
#include <qmap.h>
#include <QVBoxLayout>

//----------------------------------------------------------------------------
// OptionsTab
//----------------------------------------------------------------------------

OptionsTab::OptionsTab(QObject *parent, const char *name)
: QObject(parent, name)
{
}

OptionsTab::OptionsTab(QObject *parent, QByteArray _id, QByteArray _parentId, QString _name, QString _desc, QString _tabIconName, QString _iconName)
: QObject(parent, _name.latin1())
{
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

Icon *OptionsTab::tabIcon() const
{
	if ( v_tabIconName.isEmpty() )
		return 0;

	return (Icon *)IconsetFactory::iconPtr( v_tabIconName );
}

QString OptionsTab::name() const
{
	return v_name;
}

QString OptionsTab::desc() const
{
	return v_desc;
}

Icon *OptionsTab::icon() const
{
	if ( v_iconName.isEmpty() ) {
		//if ( tabIcon() )
		//	return tabIcon();

		return (Icon *)IconsetFactory::iconPtr("psi/logo_32");
	}

	return (Icon *)IconsetFactory::iconPtr( v_iconName );
}

void OptionsTab::applyOptions(Options *)
{
}

void OptionsTab::restoreOptions(const Options *)
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
	void restoreOptions(const Options *);

signals:
	void connectDataChanged(QWidget *);
	void noDirty(bool);

private slots:
	void updateCurrent(QWidget *);

private:
	struct TabData {
		TabData() { tab = 0; initialized = false; }
		TabData(OptionsTab *t) { tab = t; initialized = false; }
		OptionsTab *tab;
		bool initialized;
	};
	QMap<QWidget *, TabData> w2tab;
	Options *opt;
};

OptionsTabWidget::OptionsTabWidget(QWidget *parent)
: QTabWidget(parent)
{
	connect(this, SIGNAL(currentChanged(QWidget *)), SLOT(updateCurrent(QWidget *)));
	opt = 0;
}

void OptionsTabWidget::addTab(OptionsTab *tab)
{
	if ( tab->tabName().isEmpty() )
		return; // skip the dummy tabs

	// the widget will have no parent; it will be reparented
	// when inserting it with "addTab"
	QWidget *w = new QWidget(NULL, tab->name().latin1());

	if ( tab->tabIcon() )
		QTabWidget::addTab(w, tab->tabIcon()->iconSet(), tab->tabName());
	else
		QTabWidget::addTab(w, tab->tabName());

	if ( !tab->desc().isEmpty() )
		setTabToolTip(w, tab->desc());

	w2tab[w] = TabData(tab);
	
	//FIXME: this is safe for our current use of addTab, but may
	//be inconvenient in the future (Qt circa 4.2 had a bug which stopped
	//setCurrentIndex(0); from working)
	setCurrentIndex(1);
	setCurrentIndex(0);
}

void OptionsTabWidget::updateCurrent(QWidget *w)
{
	if ( !w2tab[w].initialized ) {
		QVBoxLayout *vbox = new QVBoxLayout(w, 5);
		OptionsTab *opttab = w2tab[w].tab;

		QWidget *tab = opttab->widget();
		if ( !tab )
			return;

		tab->reparent(w, 0, QPoint(0, 0));
		vbox->addWidget(tab);
		if ( !opttab->stretchable() )
			vbox->addStretch();

		if ( opt ) {
			emit noDirty(true);
			opttab->restoreOptions(opt);
			emit noDirty(false);
		}
		emit connectDataChanged(tab);

		tab->show();
		w2tab[w].initialized = true;
	}
}

void OptionsTabWidget::restoreOptions(const Options *o)
{
	bool doRestore = !opt;
	opt = (Options *)o;

	if ( doRestore ) {
		emit noDirty(true);
		w2tab[currentPage()].tab->restoreOptions(opt);
		emit noDirty(false);
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
	if ( w )
		delete w;
}

void MetaOptionsTab::init()
{
	w = 0;
	tabs.setAutoDelete(true);
}

void MetaOptionsTab::addTab(OptionsTab *tab)
{
	connect(tab, SIGNAL(dataChanged()), SIGNAL(dataChanged()));
	//connect(tab, SIGNAL(addWidgetChangedSignal(QString, QCString)), SIGNAL(addWidgetChangedSignal(QString, QCString)));
	connect(tab, SIGNAL(noDirty(bool)), SIGNAL(noDirty(bool)));
	connect(tab, SIGNAL(connectDataChanged(QWidget *)), SIGNAL(connectDataChanged(QWidget *)));

	tabs.append(tab);
}

QWidget *MetaOptionsTab::widget()
{
	if ( w )
		return w;

	OptionsTabWidget *t = new OptionsTabWidget(0);
	w = t;

	connect(w, SIGNAL(connectDataChanged(QWidget *)), SIGNAL(connectDataChanged(QWidget *)));
	connect(w, SIGNAL(noDirty(bool)), SIGNAL(noDirty(bool)));

	Q3PtrListIterator<OptionsTab> it(tabs);
	for ( ; it.current(); ++it)
		t->addTab(it.current());

	// set the current widget to 0, otherwise qt4 will show no widget
	t->setCurrentIndex(0);

	return w;
}

void MetaOptionsTab::applyOptions(Options *opt)
{
	Q3PtrListIterator<OptionsTab> it(tabs);
	for ( ; it.current(); ++it)
		it.current()->applyOptions(opt);
}

void MetaOptionsTab::restoreOptions(const Options *opt)
{
	if ( w ) {
		OptionsTabWidget *d = (OptionsTabWidget *)w;
		d->restoreOptions(opt);
	}

	Q3PtrListIterator<OptionsTab> it(tabs);
	for ( ; it.current(); ++it)
		it.current()->restoreOptions(opt);
}

void MetaOptionsTab::setData(PsiCon *psi, QWidget *w)
{
	Q3PtrListIterator<OptionsTab> it(tabs);
	for ( ; it.current(); ++it)
		it.current()->setData(psi, w);
}

#include "optionstab.moc"
