#include "opt_toolbars.h"

#include "psicon.h"
#include "common.h"
#include "iconwidget.h"
#include "psitoolbar.h"
#include "iconaction.h"
#include "psiactionlist.h"

#include "ui_opt_lookfeel_toolbars.h"
#include "ui_ui_positiontoolbar.h"

#include <qlayout.h>
#include <qpushbutton.h>
#include <q3frame.h>
#include <q3listview.h>
#include <qcombobox.h>
#include <qaction.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qspinbox.h>
//Added by qt3to4:
#include <Q3PtrList>
#include <QEvent>
#include <QHBoxLayout>
#include <QList>
#include <QVBoxLayout>

class LookFeelToolbarsUI : public QWidget, public Ui::LookFeelToolbars
{
public:
	LookFeelToolbarsUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// PositionOptionsTabToolbars
//----------------------------------------------------------------------------

class PositionOptionsTabToolbars : public QDialog, public Ui::PositionToolbarUI
{
	Q_OBJECT
public:
	PositionOptionsTabToolbars(QWidget *parent, Options::ToolbarPrefs *, int);
	~PositionOptionsTabToolbars();

	int n();
	bool dirty;

	bool eventFilter(QObject *watched, QEvent *e);

signals:
	void applyPressed();

private slots:
	void dataChanged();
	void apply();

private:
	int id;
	Options::ToolbarPrefs *tb;
};

PositionOptionsTabToolbars::PositionOptionsTabToolbars(QWidget *parent, Options::ToolbarPrefs *_tb, int _id)
: QDialog(parent)
{
	setupUi(this);
	setModal(true);
	tb = _tb;
	id = _id;


	connect(pb_ok, SIGNAL(clicked()), SLOT(apply()));
	connect(pb_ok, SIGNAL(clicked()), SLOT(accept()));
	connect(pb_apply, SIGNAL(clicked()), SLOT(apply()));
	connect(pb_cancel, SIGNAL(clicked()), SLOT(reject()));

	connect(cb_dock, SIGNAL(highlighted(int)), SLOT(dataChanged()));
	sb_index->installEventFilter( this );
	sb_extraOffset->installEventFilter( this );
	connect(ck_nl, SIGNAL(toggled(bool)), SLOT(dataChanged()));

	le_name->setText( tb->name );
	if ( tb->dock >= Qt::DockUnmanaged && tb->dock <= Qt::DockTornOff ) {
		cb_dock->setCurrentItem( tb->dock + Qt::DockMinimized - Qt::DockTornOff );
	}
	else {
		cb_dock->setCurrentItem( tb->dock - Qt::DockTop );
	}
	sb_index->setValue( tb->index );
	sb_extraOffset->setValue( tb->extraOffset );
	ck_nl->setChecked( tb->nl );

	dirty = false;
	pb_apply->setEnabled(false);

	resize(sizeHint());
}

PositionOptionsTabToolbars::~PositionOptionsTabToolbars()
{
}

int PositionOptionsTabToolbars::n()
{
	return id;
}

void PositionOptionsTabToolbars::dataChanged()
{
	dirty = true;
	pb_apply->setEnabled(true);
}

void PositionOptionsTabToolbars::apply()
{
	tb->dirty = true;
	if ( cb_dock->currentItem() >= 0 && cb_dock->currentItem() < 5 ) {
		// Top, Bottom, Left, Right and Minimised
		tb->dock = (Qt::ToolBarDock)(cb_dock->currentItem() + Qt::DockTop);
	}
	else {
		// Unmanaged and TornOff
		tb->dock = (Qt::ToolBarDock)(cb_dock->currentItem() - (Qt::DockMinimized - Qt::DockTornOff));
	}

	tb->index = sb_index->value();
	tb->extraOffset = sb_extraOffset->value();
	tb->nl = ck_nl->isChecked();

	if ( dirty )
		emit applyPressed();
	dirty = false;
	pb_apply->setEnabled(false);
}

bool PositionOptionsTabToolbars::eventFilter(QObject *watched, QEvent *e)
{
	if ( watched->inherits("QSpinBox") && e->type() == QEvent::KeyRelease )
		dataChanged();
	return false;
}

//----------------------------------------------------------------------------
// OptionsTabToolbars
//----------------------------------------------------------------------------

class OptionsTabToolbars::Private
{
public:
	struct ToolbarItem {
		QString group;
		int index;
		ToolbarItem() {
			group = "";
			index = -1;
		}
		ToolbarItem( QString _w, int _i ) {
			group = _w;
			index = _i;
		}
	};

	QMap<int, ToolbarItem> toolbars;

	PsiActionList::ActionsType class2id( QString name ) {
		int ret = (int)PsiActionList::Actions_Common;

		if ( name == "mainWin" )
			ret |= (int)PsiActionList::Actions_MainWin;

		return (PsiActionList::ActionsType)ret;
	}
};

OptionsTabToolbars::OptionsTabToolbars(QObject *parent) 
: OptionsTab(parent, "toolbars", "", tr("Toolbars"), tr("Configure Psi toolbars"), "psi/toolbars")
{
	w = 0;
	p = new Private();

	noDirty = false;
	opt = new Options;
}

QWidget *OptionsTabToolbars::widget()
{
	if (w)
		return 0;
	
	w = new LookFeelToolbarsUI();
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI*) w;
	
	connect(d->pb_addToolbar, SIGNAL(clicked()), SLOT(toolbarAdd()));
	connect(d->pb_deleteToolbar, SIGNAL(clicked()), SLOT(toolbarDelete()));
	connect(d->cb_toolbars, SIGNAL(activated(int)), SLOT(toolbarSelectionChanged(int)));
	connect(d->le_toolbarName, SIGNAL(textChanged(const QString &)), SLOT(toolbarNameChanged()));
	connect(d->pb_toolbarPosition, SIGNAL(clicked()), SLOT(toolbarPosition()));
	connect(d->tb_up, SIGNAL(clicked()), SLOT(toolbarActionUp()));
	connect(d->tb_down, SIGNAL(clicked()), SLOT(toolbarActionDown()));
	connect(d->tb_right, SIGNAL(clicked()), SLOT(toolbarAddAction()));
	connect(d->tb_left, SIGNAL(clicked()), SLOT(toolbarRemoveAction()));

	connect(d->ck_toolbarOn, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
	connect(d->ck_toolbarLocked, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
	connect(d->ck_toolbarStretch, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
	connect(d->lv_selectedActions, SIGNAL(selectionChanged(Q3ListViewItem *)), SLOT(selAct_selectionChanged(Q3ListViewItem *)));
	connect(d->lv_availActions, SIGNAL(selectionChanged(Q3ListViewItem *)), SLOT(avaAct_selectionChanged(Q3ListViewItem *)));

	connect(d->pb_deleteToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->tb_up, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->tb_down, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->tb_left, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->tb_right, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->pb_addToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->pb_deleteToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));

	d->lv_selectedActions->header()->hide();
	d->lv_availActions->header()->hide();

	d->lv_selectedActions->setSorting(-1);
	d->lv_availActions->setSorting(-1);

	return w;
	// TODO: add QWhatsThis to all widgets
	/*
	QFrame *line = new QFrame( this );
	line->setFrameShape( QFrame::HLine );
	line->setFrameShadow( QFrame::Sunken );
	line->setFrameShape( QFrame::HLine );
	vbox->addWidget( line );

	QHBoxLayout *hbox = new QHBoxLayout( 0, 0, 6 );
	vbox->addLayout(hbox);

	QSpacerItem *spacer = new QSpacerItem( 40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
	hbox->addItem( spacer );

	IconButton *pb_ok = new IconButton( this );
	hbox->addWidget( pb_ok );
	pb_ok->setText( tr("&OK") );
	connect(pb_ok, SIGNAL(clicked()), SLOT(doApply()));
	connect(pb_ok, SIGNAL(clicked()), SLOT(accept()));

	//pb_apply = 0;
	pb_apply = new IconButton( this );
	hbox->addWidget( pb_apply );
	pb_apply->setText( tr("&Apply") );
	connect(pb_apply, SIGNAL(clicked()), SLOT(doApply()));
	pb_apply->setEnabled(false);

	IconButton *pb_cancel = new IconButton( this );
	hbox->addWidget( pb_cancel );
	pb_cancel->setText( tr("&Cancel") );
	connect(pb_cancel, SIGNAL(clicked()), SLOT(reject()));

	restoreOptions( &option );
	resize( minimumSize() );*/
}

OptionsTabToolbars::~OptionsTabToolbars()
{
	delete opt;
	delete p;
}

/**
 * setData is called by the OptionsDlg private, after calling
 * the constructor, to assign the PsiCon object and the parent window
 * to all tabs.
 * /par psi_: PsiCon* object to apply the changes when needed
 * /par parent_: QWidget which is parent from the current object
 */
void OptionsTabToolbars::setData(PsiCon * psi_, QWidget *parent_)
{
	// the Psi con object is needed to apply the changes
	// the parent object is needed to show some popups
	psi  = psi_;
	parent = parent_;
}

/*void OptionsTabToolbars::setCurrentToolbar(PsiToolBar *t)
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;

	if ( pb_apply->isEnabled() )
		return;

	QMap<int, Private::ToolbarItem>::Iterator it = p->toolbars.begin();
	for ( ; it != p->toolbars.end(); ++it ) {
		if ( it.data().group == t->group() && it.data().index == t->groupIndex() ) {
			d->cb_toolbars->setCurrentIndex( it.key() );
			toolbarSelectionChanged( it.key() );
			break;
		}
	}
}*/

void OptionsTabToolbars::applyOptions(Options *o)
{
	if ( !w )
		opt->toolbars = o->toolbars;

	// get current toolbars' positions
	QList<PsiToolBar*> toolbars = psi->toolbarList();
	for (int i = 0; i < opt->toolbars["mainWin"].count() && i < toolbars.count(); i++) {
		//if ( toolbarPositionInProgress && posTbDlg->n() == (int)i )
		//	continue;

		Options::ToolbarPrefs &tbPref = opt->toolbars["mainWin"][i];
		psi->getToolbarLocation(toolbars.at(i), tbPref.dock, tbPref.index, tbPref.nl, tbPref.extraOffset);
	}

	// apply options
	o->toolbars = opt->toolbars;

	doApply();
}

void OptionsTabToolbars::rebuildToolbarList()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;

	d->cb_toolbars->clear();
	p->toolbars.clear();

	QMap< QString, QList<Options::ToolbarPrefs> >::Iterator it = opt->toolbars.begin();
	for ( ; it != opt->toolbars.end(); ++it ) {
		if ( p->toolbars.count() )
			d->cb_toolbars->insertItem( "---" ); // TODO: think of better separator

		int i = 0;
		QList<Options::ToolbarPrefs>::Iterator it2 = it.data().begin();
		for ( ; it2 != it.data().end(); ++it2, ++i ) {
			d->cb_toolbars->insertItem( (*it2).name );
			p->toolbars[d->cb_toolbars->count()-1] = Private::ToolbarItem( it.key(), i );
		}
	}
}

void OptionsTabToolbars::restoreOptions(const Options *o)
{
	if ( !w )
		return;

	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	opt->toolbars = o->toolbars;
	opt->hideMenubar = o->hideMenubar;

	rebuildToolbarList();

	if(d->cb_toolbars->count() > 0) {
		d->cb_toolbars->setCurrentIndex( 0 );
		toolbarSelectionChanged( 0 );
	}
	else
		toolbarSelectionChanged( -1 );
}

//----------------------------------------------------------------------------

void OptionsTabToolbars::toolbarAdd()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;

	int i = d->cb_toolbars->count();

	Options::ToolbarPrefs tb;
	tb.name = QObject::tr("<unnamed>");
	tb.on = false;
	tb.locked = false;
	tb.stretchable = false;
	tb.keys.clear();

	tb.dock = Qt::DockTop;
	tb.index = i;
	tb.nl = true;
	tb.extraOffset = 0;

	tb.dirty = true;

	opt->toolbars["mainWin"].append(tb);

	rebuildToolbarList();
	d->cb_toolbars->setCurrentIndex( d->cb_toolbars->count()-1 );
	toolbarSelectionChanged( d->cb_toolbars->currentIndex() );

	d->le_toolbarName->setFocus();
}

void OptionsTabToolbars::toolbarDelete()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	int n = d->cb_toolbars->currentIndex();

	{
		int idx = p->toolbars[n].index;
		QList<Options::ToolbarPrefs>::Iterator it = opt->toolbars[p->toolbars[n].group].begin();
		for (int i = 0; i < idx; i++)
			++it;

		noDirty = true;
		opt->toolbars[p->toolbars[n].group].remove(it);

		toolbarSelectionChanged(-1);
		rebuildToolbarList();
		noDirty = false;
		toolbarSelectionChanged( d->cb_toolbars->currentIndex() );
	}
}

void OptionsTabToolbars::addToolbarAction(Q3ListView *parent, QString name, int toolbarId)
{
	ActionList actions = psi->actionList()->suitableActions( (PsiActionList::ActionsType)toolbarId );
	const QAction *action = (QAction *)actions.action( name );
	if ( !action )
		return;

	addToolbarAction(parent, action, name);
}

void OptionsTabToolbars::addToolbarAction(Q3ListView *parent, const QAction *action, QString name)
{
	Q3ListViewItem *item = new Q3ListViewItem(parent, parent->lastItem());

	QString n = actionName(action);
	if ( !action->whatsThis().isEmpty() )
		n += " - " + action->whatsThis();
	item->setText(0, n);
	item->setText(1, name);
	item->setPixmap(0, action->iconSet().pixmap());
}

void OptionsTabToolbars::toolbarSelectionChanged(int item)
{
	if ( noDirty )
		return;

	int n = item;
	PsiToolBar *toolBar = 0;
	if ( item != -1 )
		toolBar = psi->findToolBar( p->toolbars[n].group, p->toolbars[n].index );

	bool customizeable = toolBar ? toolBar->isCustomizeable() : true;
	bool moveable      = toolBar ? toolBar->isMoveable() : true;

	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	bool enable = (item == -1) ? false: true;
	d->le_toolbarName->setEnabled( enable );
	d->pb_toolbarPosition->setEnabled( enable && moveable );
	d->ck_toolbarOn->setEnabled( enable );
	d->ck_toolbarLocked->setEnabled( enable && moveable );
	d->ck_toolbarStretch->setEnabled( enable && moveable );
	d->lv_selectedActions->setEnabled( enable && customizeable );
	d->lv_availActions->setEnabled( enable && customizeable );
	d->tb_up->setEnabled( enable && customizeable );
	d->tb_down->setEnabled( enable && customizeable );
	d->tb_left->setEnabled( enable && customizeable );
	d->tb_right->setEnabled( enable && customizeable );
	d->pb_deleteToolbar->setEnabled( enable && p->toolbars[n].group == "mainWin" );
	d->cb_toolbars->setEnabled( enable );

	d->lv_availActions->clear();
	d->lv_selectedActions->clear();

	if ( !enable ) {
		d->le_toolbarName->setText( "" );
		return;
	}

	noDirty = true;

	Options::ToolbarPrefs tb = opt->toolbars[p->toolbars[n].group][p->toolbars[n].index];
	d->le_toolbarName->setText( tb.name );
	d->ck_toolbarOn->setChecked( tb.on );
	d->ck_toolbarLocked->setChecked( tb.locked || !moveable );
	d->ck_toolbarStretch->setChecked( tb.stretchable );

	{
		// Fill the ListView with toolbar-specific actions
		Q3ListView *lv = d->lv_availActions;
		Q3ListViewItem *lastRoot = 0;
		foreach(ActionList* actionList, psi->actionList()->actionLists( p->class2id( p->toolbars[n].group ) )) {
			Q3ListViewItem *root = new Q3ListViewItem(lv, lastRoot);
			lastRoot = root;
			root->setText( 0, actionList->name() );
			root->setOpen( true );

			Q3ListViewItem *last = 0;
			QStringList actionNames = actionList->actions();
			QStringList::Iterator it2 = actionNames.begin();
			for ( ; it2 != actionNames.end(); ++it2 ) {
				IconAction *action = actionList->action( *it2 );
		        	Q3ListViewItem *item = new Q3ListViewItem( root, last );
				last = item;

				QString n = actionName((QAction *)action);
				if ( !action->whatsThis().isEmpty() )
					n += " - " + action->whatsThis();
				item->setText(0, n);
				item->setText(1, action->name());
				item->setPixmap(0, action->iconSet().pixmap());
			}
		}
	}

	QStringList::Iterator it = tb.keys.begin();
	for ( ; it != tb.keys.end(); ++it) {
		addToolbarAction(d->lv_selectedActions, *it, p->class2id( p->toolbars[n].group ) );
	}
	updateArrows();

	noDirty = false;
}

void OptionsTabToolbars::rebuildToolbarKeys()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	if ( !d->cb_toolbars->count() )
		return;
	int n = d->cb_toolbars->currentIndex();

	QStringList keys;
	Q3ListViewItemIterator it( d->lv_selectedActions );
        for ( ; it.current(); ++it) {
		Q3ListViewItem *item = it.current();

		keys << item->text(1);
        }

	opt->toolbars["mainWin"][n].keys  = keys;
	opt->toolbars["mainWin"][n].dirty = true;
	emit dataChanged();
}

void OptionsTabToolbars::updateArrows()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	bool up = false, down = false, left = false, right = false;

	if(d->lv_availActions->selectedItem() && !d->lv_availActions->selectedItem()->text(1).isEmpty())
		right = true;
	Q3ListViewItem *i = d->lv_selectedActions->selectedItem();
	if(i) {
		left = true;

		// get numeric index of item
		int n = 0;
		for(Q3ListViewItem *it = d->lv_selectedActions->firstChild(); it != i; it = it->nextSibling()) {
			++n;
		}

		if(n > 0)
			up = true;
		if(n < d->lv_selectedActions->childCount()-1)
			down = true;
	}

	d->tb_up->setEnabled(up);
	d->tb_down->setEnabled(down);
	d->tb_left->setEnabled(left);
	d->tb_right->setEnabled(right);
}

void OptionsTabToolbars::toolbarNameChanged()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	if ( !d->cb_toolbars->count() )
		return;

	int n = d->cb_toolbars->currentIndex();
	d->cb_toolbars->changeItem(d->le_toolbarName->text(), n);
	opt->toolbars["mainWin"][n].name = d->le_toolbarName->text();

	emit dataChanged();
}

void OptionsTabToolbars::toolbarActionUp()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	Q3ListViewItem *item = d->lv_selectedActions->selectedItem();
	if ( !item )
		return;

	if ( item->itemAbove() )
		item->itemAbove()->moveItem(item);
	rebuildToolbarKeys();
	updateArrows();
}

void OptionsTabToolbars::toolbarActionDown()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	Q3ListViewItem *item = d->lv_selectedActions->selectedItem();
	if ( !item )
		return;

	if ( item->itemBelow() )
		item->moveItem( item->itemBelow() );
	rebuildToolbarKeys();
	updateArrows();
}

void OptionsTabToolbars::toolbarAddAction()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	Q3ListViewItem *item = d->lv_availActions->selectedItem();
	if ( !item || item->text(1).isEmpty() )
		return;

	addToolbarAction(d->lv_selectedActions, item->text(1), p->class2id( p->toolbars[d->cb_toolbars->currentIndex()].group ) );
	rebuildToolbarKeys();
	updateArrows();
}

void OptionsTabToolbars::toolbarRemoveAction()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	Q3ListViewItem *item = d->lv_selectedActions->selectedItem();
	if ( !item )
		return;

	delete item;

	if(d->lv_selectedActions->currentItem())
		d->lv_selectedActions->setSelected(d->lv_selectedActions->currentItem(), true);

	rebuildToolbarKeys();
	updateArrows();
}

void OptionsTabToolbars::toolbarDataChanged()
{
	if ( noDirty )
		return;

	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	if ( !d->cb_toolbars->count() )
		return;
	int n = d->cb_toolbars->currentIndex();

	opt->toolbars["mainWin"][n].dirty = true;
	opt->toolbars["mainWin"][n].name = d->le_toolbarName->text();
	opt->toolbars["mainWin"][n].on = d->ck_toolbarOn->isChecked();
	opt->toolbars["mainWin"][n].locked = d->ck_toolbarLocked->isChecked();
	opt->toolbars["mainWin"][n].stretchable = d->ck_toolbarStretch->isChecked();

	emit dataChanged();
}

QString OptionsTabToolbars::actionName(const QAction *a)
{
	QString n = a->menuText(), n2;
	for (int i = 0; i < (int)n.length(); i++) {
		if ( n[i] == '&' && n[i+1] != '&' )
			continue;
		else if ( n[i] == '&' && n[i+1] == '&' )
			n2 += '&';
		else
			n2 += n[i];
	}

	return n2;
}

void OptionsTabToolbars::toolbarPosition()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	if ( !d->cb_toolbars->count() )
		return;
	int n = d->cb_toolbars->currentIndex();

	PositionOptionsTabToolbars *posTbDlg = new PositionOptionsTabToolbars(w, &opt->toolbars["mainWin"][n], n);
	connect(posTbDlg, SIGNAL(applyPressed()), SLOT(toolbarPositionApply()));

	posTbDlg->exec();
	delete posTbDlg;
}

void OptionsTabToolbars::toolbarPositionApply()
{
	emit dataChanged();

	option.toolbars = opt->toolbars;
	psi->buildToolbars();
}

void OptionsTabToolbars::doApply()
{
	option.toolbars = opt->toolbars;
	psi->buildToolbars();
}

void OptionsTabToolbars::selAct_selectionChanged(Q3ListViewItem *)
{
	updateArrows();
}

void OptionsTabToolbars::avaAct_selectionChanged(Q3ListViewItem *)
{
	updateArrows();
}

#include "opt_toolbars.moc"
