#include "optionsdlg.h"
#include "optionstab.h"
#include "common.h"
#include "psicon.h"
#include "fancylabel.h"
#include "iconset.h"
#include "iconwidget.h"

#include <qlayout.h>
#include <qlabel.h>
#include <q3listview.h>
#include <QStackedWidget>
#include <qpen.h>
#include <qpainter.h>
#include <Q3Dict>
#include <QPixmap>
#include <Q3PtrList>
#include <QVBoxLayout>

// tabs
#include "opt_toolbars.h"
#include "opt_application.h"
#include "opt_appearance.h"
#include "opt_chat.h"
#include "opt_events.h"
#include "opt_status.h"
#include "opt_iconset.h"
#include "opt_groupchat.h"
#include "opt_sound.h"
#include "opt_advanced.h"
#include "opt_shortcuts.h"
#include "opt_tree.h"

#ifdef PSI_PLUGINS
#include "opt_plugins.h"
#endif

//----------------------------------------------------------------------------
// FancyItem
//----------------------------------------------------------------------------

class FancyItem : public Q3ListViewItem
{
public:
	FancyItem(Q3ListView *, Q3ListViewItem *after);

	void setup();
	int width(const QFontMetrics &, const Q3ListView *lv, int c) const;
	void paintFocus(QPainter *, const QColorGroup &, const QRect &);
	void paintCell(QPainter *p, const QColorGroup &, int c, int width, int align);
};

FancyItem::FancyItem(Q3ListView *lv, Q3ListViewItem *after)
: Q3ListViewItem(lv, after)
{
}

void FancyItem::setup()
{
	Q3ListView *lv = listView();
	int ph = 0;
	for(int i = 0; i < lv->columns(); ++i) {
		if(pixmap(i))
			ph = QMAX(ph, pixmap(i)->height());
	}
	int y = QMAX(ph, lv->fontMetrics().height());
	y += 8;
	setHeight(y);
}

int FancyItem::width(const QFontMetrics &fm, const Q3ListView *, int c) const
{
	int x = 0;
	const QPixmap *pix = pixmap(c);
	if(pix)
		x += pix->width();
	else
		x += 16;
	x += 8;
	x += fm.width(text(c));
	x += 8;
	return x;
}

void FancyItem::paintFocus(QPainter *, const QColorGroup &, const QRect &)
{
	// re-implimented to do nothing.  selection is enough of a focus
}

void FancyItem::paintCell(QPainter *p, const QColorGroup &cg, int c, int w, int)
{
	int h = height();
	QFontMetrics fm(p->font());
	if(isSelected())
		p->fillRect(0, 0, w, h-1, cg.highlight());
	else
		p->fillRect(0, 0, w, h, cg.base());

	int x = 0;
	const QPixmap *pix = pixmap(c);
	if(pix) {
		p->drawPixmap(4, (h - pix->height()) / 2, *pix);
		x += pix->width();
	}
	else
		x += 16;
	x += 8;
	int y = ((h - fm.height()) / 2) + fm.ascent();
	p->setPen(isSelected() ? cg.highlightedText() : cg.text());
	p->drawText(x, y, text(c));

	p->setPen(QPen(QColor(0xE0, 0xE0, 0xE0), 0, Qt::DotLine));
	p->drawLine(0, h-1, w-1, h-1);
}

//----------------------------------------------------------------------------
// OptionsTabBase
//----------------------------------------------------------------------------

//class OptionsTabBase : public OptionsTab
//{
//	Q_OBJECT
//public:
//	OptionsTabBase(QObject *parent, QCString id, QCString parentId, QString iconName, QString name, QString desc)
//		: OptionsTab(parent, id, parentId, name, desc, iconName)
//	{
//		w = new QWidget();
//		QGridLayout *layout = new QGridLayout(w, 0, 2, 0, 5);
//		layout->setAutoAdd(true);
//	}
//	~OptionsTabBase()
//	{
//		w->deleteLater();
//	}
//
//	QWidget *widget() { return w; }
//
//public slots:
//	void tabAdded(OptionsTab *tab);
//
//private:
//	QWidget *w;
//};
//
//void OptionsTabBase::tabAdded(OptionsTab *tab)
//{
//	//qWarning("OptionsTabBase::tabAdded(): id = %s, tab_id = %s", (const char *)id(), (const char *)tab->id());
//	QLabel *name = new QLabel(w);
//	name->setText("<b>" + tab->name() + "</b>");
//	name->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
//
//	IconLabel *desc = new IconLabel(w);
//	desc->setText(tab->desc());
//	desc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
//}

//----------------------------------------------------------------------------
// OptionsDlg::Private
//----------------------------------------------------------------------------

class OptionsDlg::Private : public QObject
{
	Q_OBJECT
public:
	Private(OptionsDlg *dlg, PsiCon *_psi);

public slots:
	void doApply();
	void openTab(QString id);

private slots:
	void itemSelected(Q3ListViewItem *);
	void dataChanged();
	void noDirtySlot(bool);
	void createTabs();
	void createChangedMap();

	//void addWidgetChangedSignal(QString widgetName, QCString signal);
	void connectDataChanged(QWidget *);

public:
	OptionsDlg *dlg;
	PsiCon *psi;
	bool dirty, noDirty;
	Q3Dict<QWidget> id2widget;
	Q3PtrList<OptionsTab> tabs;

	QMap<QString, QByteArray> changedMap;
};

OptionsDlg::Private::Private(OptionsDlg *d, PsiCon *_psi)
{
	dlg = d;
	psi = _psi;
	noDirty = false;
	dirty = false;

	dlg->lb_pageTitle->setScaledContents(32, 32);

	dlg->lv_tabs->setSorting( -1 );
	dlg->lv_tabs->header()->hide();
	connect(dlg->lv_tabs, SIGNAL(selectionChanged(Q3ListViewItem *)), SLOT(itemSelected(Q3ListViewItem *)));

	createTabs();
	createChangedMap();

	// fill the QListView
	Q3PtrListIterator<OptionsTab> it ( tabs );
	OptionsTab *opttab;
	for ( ; it.current(); ++it) {
		opttab = it.current();
		//qWarning("Adding tab %s...", (const char *)opttab->id());
		opttab->setData(psi, dlg);
		connect(opttab, SIGNAL(dataChanged()), SLOT(dataChanged()));
		//connect(opttab, SIGNAL(addWidgetChangedSignal(QString, QCString)), SLOT(addWidgetChangedSignal(QString, QCString)));
		connect(opttab, SIGNAL(noDirty(bool)), SLOT(noDirtySlot(bool)));
		connect(opttab, SIGNAL(connectDataChanged(QWidget *)), SLOT(connectDataChanged(QWidget *)));

		// search for parent
		Q3ListViewItem *parent = 0, *prev = 0;
		QString parentId = opttab->parentId();
		if ( !parentId.isEmpty() ) {
			Q3ListViewItemIterator it2( dlg->lv_tabs );
			for ( ; it2.current(); ++it2) {
				//qWarning("Searching the QListView %s...", it2.current()->text(1).latin1());
				if ( it2.current()->text(1) == parentId ) {
					//qWarning("...done");
					parent = it2.current();

					// notify the parent about the child
					Q3PtrListIterator<OptionsTab> it3 ( tabs );
					OptionsTab *opttab2;
					for ( ; it3.current(); ++it3) {
						opttab2 = it3.current();
						//qWarning("Searching tabs %s...", (const char *)opttab2->id());
						if ( opttab2->id() == opttab->parentId() ) {
							//qWarning("...done");
							opttab2->tabAdded( opttab );
							break;
						}
					}

					parent->setOpen( true );
					break;
				}
			}
		}
		//qWarning("****************");

		// search for previous item
		Q3ListViewItem *top;
		if ( parent )
			top = parent->firstChild();
		else
			top = dlg->lv_tabs->firstChild();
		prev = top;
		while ( prev ) {
			if ( !prev->nextSibling() )
				break;
			prev = prev->nextSibling();
		}

		if ( opttab->id().isEmpty() )
			continue;

		// create tab
		Q3ListViewItem *item;
		//if ( parent )
		//	item = new FancyItem(parent, prev);
		//else
			item = new FancyItem(dlg->lv_tabs, prev);

		item->setText(0, opttab->tabName());
		if ( opttab->tabIcon() )
			item->setPixmap(0, opttab->tabIcon()->impix().pixmap());
		item->setText(1, opttab->id());

		// create separator
		//if ( !parent ) {
		//	new ListItemSeparator(dlg->lv_tabs, item);
		//}
	}

	// fix the width of the listview based on the largest item
	int largestWidth = 0;
	QFontMetrics fm(dlg->lv_tabs->font());
	for(Q3ListViewItem *i = dlg->lv_tabs->firstChild(); i; i = i->nextSibling())
		largestWidth = QMAX(largestWidth, i->width(fm, dlg->lv_tabs, 0));
	dlg->lv_tabs->setFixedWidth(largestWidth + 32);

	openTab( "application" );

	dirty = false;
	dlg->pb_apply->setEnabled(false);
}

void OptionsDlg::Private::createTabs()
{
	// tabs - base
	/*tabs.append( new OptionsTabGeneral(this) );
	//tabs.append( new OptionsTabBase(this, "general",  "", "psi/logo_16",	tr("General"),		tr("General preferences list")) );
	tabs.append( new OptionsTabEvents(this) );
	//tabs.append( new OptionsTabBase(this, "events",   "", "psi/system",	tr("Events"),		tr("Change the events behaviour")) );
	tabs.append( new OptionsTabPresence(this) );
	//tabs.append( new OptionsTabBase(this, "presence", "", "status/online",	tr("Presence"),		tr("Presence configuration")) );
	tabs.append( new OptionsTabLookFeel(this) );
	tabs.append( new OptionsTabIconset(this) );
	//tabs.append( new OptionsTabBase(this, "lookfeel", "", "psi/smile",	tr("Look and Feel"),	tr("Change the Psi's Look and Feel")) );
	tabs.append( new OptionsTabSound(this) );
	//tabs.append( new OptionsTabBase(this, "sound",    "", "psi/playSounds",	tr("Sound"),		tr("Configure how Psi sounds")) );
	*/

	tabs.append( new OptionsTabApplication(this) );
	tabs.append( new OptionsTabChat(this) );
	tabs.append( new OptionsTabEvents(this) );
	tabs.append( new OptionsTabStatus(this) );
	tabs.append( new OptionsTabAppearance(this) );
	//tabs.append( new OptionsTabIconsetSystem(this) );
	//tabs.append( new OptionsTabIconsetRoster(this) );
	//tabs.append( new OptionsTabIconsetEmoticons(this) );
	tabs.append( new OptionsTabGroupchat(this) );
	tabs.append( new OptionsTabSound(this) );
	tabs.append( new OptionsTabToolbars(this) );
#ifdef PSI_PLUGINS
	tabs.append( new OptionsTabPlugins(this) );
#endif
	tabs.append( new OptionsTabShortcuts(this) );
	tabs.append( new OptionsTabAdvanced(this) );
	tabs.append( new OptionsTabTree(this) );

	// tabs - general
	/*tabs.append( new OptionsTabGeneralRoster(this) );
	tabs.append( new OptionsTabGeneralDocking(this) );
	tabs.append( new OptionsTabGeneralNotifications(this) );
	tabs.append( new OptionsTabGeneralGroupchat(this) );
	tabs.append( new OptionsTabGeneralMisc(this) );*/

	// tabs - events
	/*tabs.append( new OptionsTabEventsReceive(this) );
	tabs.append( new OptionsTabEventsMisc(this) );*/

	// tabs - presence
	/*tabs.append( new OptionsTabPresenceAuto(this) );
	tabs.append( new OptionsTabPresencePresets(this) );
	tabs.append( new OptionsTabPresenceMisc(this) );*/

	// tabs - look and feel
	/*tabs.append( new OptionsTabLookFeelColors(this) );
	tabs.append( new OptionsTabLookFeelFonts(this) );
	tabs.append( new OptionsTabIconsetSystem(this) );
	tabs.append( new OptionsTabIconsetEmoticons(this) );
	tabs.append( new OptionsTabIconsetRoster(this) );
	tabs.append( new OptionsTabLookFeelToolbars(this) );
	tabs.append( new OptionsTabLookFeelMisc(this) );*/

	// tabs - sound
	/*tabs.append( new OptionsTabSoundPrefs(this) );
	tabs.append( new OptionsTabSoundEvents(this) );*/
}

void OptionsDlg::Private::createChangedMap()
{
	// NOTE about commented out signals:
	//   Do NOT call addWidgetChangedSignal() for them.
	//   Instead, connect the widget's signal to your tab own dataChaged() signal
	changedMap.insert("QButton", SIGNAL(stateChanged(int)));
	changedMap.insert("QCheckBox", SIGNAL(stateChanged(int)));
	//qt4 port: there are no stateChangedSignals anymore
	//changedMap.insert("QPushButton", SIGNAL(stateChanged(int)));
	//changedMap.insert("QRadioButton", SIGNAL(stateChanged(int)));
	changedMap.insert("QRadioButton",SIGNAL(toggled (bool)));
	changedMap.insert("QComboBox", SIGNAL(activated (int)));
	//changedMap.insert("QComboBox", SIGNAL(textChanged(const QString &)));
	changedMap.insert("QDateEdit", SIGNAL(valueChanged(const QDate &)));
	changedMap.insert("QDateTimeEdit", SIGNAL(valueChanged(const QDateTime &)));
	changedMap.insert("QDial", SIGNAL(valueChanged (int)));
	changedMap.insert("QLineEdit", SIGNAL(textChanged(const QString &)));
	changedMap.insert("QSlider", SIGNAL(valueChanged(int)));
	changedMap.insert("QSpinBox", SIGNAL(valueChanged(int)));
	changedMap.insert("QTimeEdit", SIGNAL(valueChanged(const QTime &)));
	changedMap.insert("QTextEdit", SIGNAL(textChanged()));
	changedMap.insert("QTextBrowser", SIGNAL(sourceChanged(const QString &)));
	changedMap.insert("QMultiLineEdit", SIGNAL(textChanged()));
	//changedMap.insert("QListBox", SIGNAL(selectionChanged()));
	//changedMap.insert("QTabWidget", SIGNAL(currentChanged(QWidget *)));
}

//void OptionsDlg::Private::addWidgetChangedSignal(QString widgetName, QCString signal)
//{
//	changedMap.insert(widgetName, signal);
//}

void OptionsDlg::Private::openTab(QString id)
{
	if ( id.isEmpty() )
		return;

	QWidget *tab = id2widget[id];
	if ( !tab ) {
		bool found = false;
		Q3PtrListIterator<OptionsTab> it ( tabs );
		OptionsTab *opttab;
		for ( ; it.current(); ++it) {
			opttab = it.current();

			if ( opttab->id() == id.latin1() ) {
				tab = opttab->widget(); // create the widget
				if ( !tab )
					continue;

				// TODO: how about QScrollView for large tabs?
				// idea: maybe do it only for those, whose sizeHint is bigger than ws_tabs'
				QWidget *w = new QWidget(dlg->ws_tabs, "QWidgetStack/tab");
				QVBoxLayout *vbox = new QVBoxLayout(w);
				vbox->setSpacing(0);
				vbox->setMargin(0);

				/*FancyLabel *toplbl = new FancyLabel(w, "QWidgetStack/tab/FancyLabel");
				toplbl->setText( opttab->name() );
				toplbl->setHelp( opttab->desc() );
				toplbl->setIcon( opttab->icon() );
				vbox->addWidget( toplbl );
				vbox->addSpacing( 5 );*/

				tab->reparent(w, 0, QPoint(0, 0));
				vbox->addWidget(tab);
				if ( !opttab->stretchable() )
					vbox->addStretch();

				dlg->ws_tabs->addWidget(w);
				id2widget.insert( id, w );
				connectDataChanged( tab ); // no need to connect to dataChanged() slot by hands anymore

				bool d = dirty;

				opttab->restoreOptions(); // initialize widgets' values

				dirty = d;
				dlg->pb_apply->setEnabled( dirty );

				tab = w;
				found = true;
				break;
			}
		}

		if ( !found ) {
			qWarning("OptionsDlg::Private::itemSelected(): could not create widget for id '%s'", id.latin1());
			return;
		}
	}

	{
		Q3PtrListIterator<OptionsTab> it ( tabs );
		OptionsTab *opttab;
		for ( ; it.current(); ++it) {
			opttab = it.current();

			if ( opttab->id() == id.latin1() ) {
				dlg->lb_pageTitle->setText( opttab->name() );
				dlg->lb_pageTitle->setHelp( opttab->desc() );
				dlg->lb_pageTitle->setPsiIcon( opttab->psiIcon() );

				break;
			}
		}
	}

	dlg->ws_tabs->raiseWidget( tab );

	// and select item in lv_tabs...
	Q3ListViewItemIterator it( dlg->lv_tabs );
	while ( it.current() ) {
		it.current()->setSelected( it.current()->text(1) == id );
		++it;
	}
}

void OptionsDlg::Private::connectDataChanged(QWidget *widget)
{
	QObjectList l = widget->queryList( "QWidget", 0, false, true ); // search for all QWidget children of widget
	for ( QObjectList::Iterator it = l.begin(); it != l.end(); ++it) {
		QWidget *w = (QWidget*) (*it);
		QMap<QString, QByteArray>::Iterator it2 = changedMap.find( w->className() );
		if ( it2 != changedMap.end() ) {
			disconnect(w, changedMap[w->className()], this, SLOT(dataChanged()));
			connect(w, changedMap[w->className()], SLOT(dataChanged()));
		}
	}
}

void OptionsDlg::Private::itemSelected(Q3ListViewItem *item)
{
	if ( !item )
		return;

	openTab( item->text(1) );
}

void OptionsDlg::Private::dataChanged()
{
	if ( dirty )
		return;

	if ( !noDirty ) {
		dirty = true;
		dlg->pb_apply->setEnabled(true);
	}
}

void OptionsDlg::Private::noDirtySlot(bool d)
{
	noDirty = d;
}

void OptionsDlg::Private::doApply()
{
	if ( !dirty )
		return;

	Q3PtrListIterator<OptionsTab> it ( tabs );
	OptionsTab *opttab;
	for ( ; it.current(); ++it) {
		opttab = it.current();

		opttab->applyOptions();
	}

	emit dlg->applyOptions();

	dirty = false;
	dlg->pb_apply->setEnabled(false);
}

//----------------------------------------------------------------------------
// OptionsDlg
//----------------------------------------------------------------------------

OptionsDlg::OptionsDlg(PsiCon *psi, QWidget *parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setupUi(this);
	d = new Private(this, psi);
	setModal(false);
	d->psi->dialogRegister(this);

	setWindowTitle(CAP(caption()));
	resize(640, 480);

	connect(pb_ok, SIGNAL(clicked()), SLOT(doOk()));
	connect(pb_apply,SIGNAL(clicked()),SLOT(doApply()));
	connect(pb_cancel, SIGNAL(clicked()), SLOT(reject()));

}

OptionsDlg::~OptionsDlg()
{
	d->psi->dialogUnregister(this);
	delete d;
}

void OptionsDlg::openTab(const QString& id)
{
	d->openTab(id);
}

void OptionsDlg::doOk()
{
	doApply();
	accept();
}

void OptionsDlg::doApply()
{
	d->doApply();
}

#include "optionsdlg.moc"
