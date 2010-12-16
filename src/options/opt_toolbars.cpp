#include "opt_toolbars.h"

#include "psicon.h"
#include "common.h"
#include "iconwidget.h"
#include "psitoolbar.h"
#include "iconaction.h"
#include "psiactionlist.h"
#include "psioptions.h"

#include "ui_opt_lookfeel_toolbars.h"

#include <QLayout>
#include <QPushButton>
#include <QComboBox>
#include <QAction>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QHeaderView>
//Added by qt3to4:
#include <QEvent>
#include <QHBoxLayout>
#include <QList>
#include <QVBoxLayout>

class LookFeelToolbarsUI : public QWidget, public Ui::LookFeelToolbars
{
public:
	LookFeelToolbarsUI() : QWidget() {
		setupUi(this);
	}
};

//----------------------------------------------------------------------------
// OptionsTabToolbars
//----------------------------------------------------------------------------

class OptionsTabToolbars::Private
{
public:
	QMap<QString, ToolbarPrefs> toolbars;

	PsiActionList::ActionsType class2id() {
		int ret = (int)PsiActionList::Actions_Common;
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
	// connect(d->pb_toolbarPosition, SIGNAL(clicked()), SLOT(toolbarPosition()));
	connect(d->tb_up, SIGNAL(clicked()), SLOT(toolbarActionUp()));
	connect(d->tb_down, SIGNAL(clicked()), SLOT(toolbarActionDown()));
	connect(d->tb_right, SIGNAL(clicked()), SLOT(toolbarAddAction()));
	connect(d->tb_left, SIGNAL(clicked()), SLOT(toolbarRemoveAction()));

	connect(d->ck_toolbarOn, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
	connect(d->ck_toolbarLocked, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
	// connect(d->ck_toolbarStretch, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
	connect(d->lw_selectedActions, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), SLOT(selAct_selectionChanged(QListWidgetItem *)));
	connect(d->tw_availActions, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), SLOT(avaAct_selectionChanged(QTreeWidgetItem *)));

	connect(d->pb_deleteToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->tb_up, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->tb_down, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->tb_left, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->tb_right, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->pb_addToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->pb_deleteToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));

	d->tw_availActions->header()->hide();

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

void OptionsTabToolbars::applyOptions()
{
	if (!w)
		return;

	PsiOptions *o = PsiOptions::instance();
	o->removeOption("options.ui.contactlist.toolbars", true);
	QMap<QString, ToolbarPrefs>::Iterator it = p->toolbars.begin();
	for (; it != p->toolbars.end(); ++it) {
		PsiToolBar::structToOptions(o, it.value());
	}
}

void OptionsTabToolbars::restoreOptions()
{
	if (!w)
		return;

	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;

	PsiOptions *o = PsiOptions::instance();

	QStringList toolbarBases = o->getChildOptionNames("options.ui.contactlist.toolbars", true, true);

	foreach(QString base, toolbarBases) {
		ToolbarPrefs tb;
		tb.id = o->getOption(base + ".key").toString();
		tb.name = o->getOption(base + ".name").toString();
		tb.on = o->getOption(base + ".visible").toBool();
		tb.locked = o->getOption(base + ".locked").toBool();
		// tb.stretchable = o->getOption(base + ".stretchable").toBool();
		tb.dock = (Qt3Dock)o->getOption(base + ".dock.position").toInt(); //FIXME
		// tb.index = o->getOption(base + ".dock.index").toInt();
		tb.nl = o->getOption(base + ".dock.nl").toBool();
		// tb.extraOffset = o->getOption(base + ".dock.extra-offset").toInt();
		tb.keys = o->getOption(base + ".actions").toStringList();

		p->toolbars[base] = tb;
		d->cb_toolbars->addItem(tb.name, base);
	}

	if (d->cb_toolbars->count() > 0) {
		d->cb_toolbars->setCurrentIndex(0);
		toolbarSelectionChanged(0);
	}
	else
		toolbarSelectionChanged(-1);
}

//----------------------------------------------------------------------------

void OptionsTabToolbars::toolbarAdd()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;

	ToolbarPrefs tb;
	int j = 0;
	bool ok;
	do {
		ok = true;
		tb.name = QObject::tr("<unnamed%1>").arg(j++);
		foreach(ToolbarPrefs other, p->toolbars) {
			if (other.name == tb.name) {
				ok = false;
				break;
			}
		}
	}
	while (!ok);

	tb.on = false;
	tb.locked = false;
	// tb.stretchable = false;
	tb.keys.clear();

	tb.dock = Qt3Dock_Top;
	// tb.index = i;
	tb.nl = true;
	// tb.extraOffset = 0;

	tb.dirty = true;

	QString base;
	j = 0;
	do {
		ok = true;
		base = ".." + QString::number(j++);
	}
	while (p->toolbars.keys().contains(base));

	p->toolbars[base] = tb;

	d->cb_toolbars->addItem(tb.name, base);

	d->cb_toolbars->setCurrentIndex(d->cb_toolbars->count() - 1);
	toolbarSelectionChanged(d->cb_toolbars->currentIndex());

	d->le_toolbarName->setFocus();
}

void OptionsTabToolbars::toolbarDelete()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	int n = d->cb_toolbars->currentIndex();

	QString base = d->cb_toolbars->itemData(n).toString();

	noDirty = true;
	toolbarSelectionChanged(-1);

	p->toolbars.remove(base);

	d->cb_toolbars->removeItem(d->cb_toolbars->findData(base));

	noDirty = false;
	toolbarSelectionChanged(d->cb_toolbars->currentIndex());
}

void OptionsTabToolbars::addToolbarAction(QListWidget *parent, QString name, int toolbarId)
{
	ActionList actions = psi->actionList()->suitableActions((PsiActionList::ActionsType)toolbarId);
	const QAction *action = (QAction *)actions.action(name);
	if (!action)
		return;
	addToolbarAction(parent, action, name);
}

void OptionsTabToolbars::addToolbarAction(QListWidget *parent, const QAction *action, QString name)
{
	QListWidgetItem *item = new QListWidgetItem(parent);

	QString n = actionName(action);
	if (!action->whatsThis().isEmpty())
		n += " - " + action->whatsThis();
	item->setText(n);
	item->setData(Qt::UserRole, name);
	item->setIcon(action->icon());
	item->setHidden(!action->isVisible());
}

void OptionsTabToolbars::toolbarSelectionChanged(int item)
{
	if (noDirty)
		return;

	int n = item;
//	PsiToolBar *toolBar = 0;
//	if ( item != -1 )
//		toolBar = psi->findToolBar( p->toolbars[n].group, p->toolbars[n].index );

	bool customizeable = true;
	bool moveable      = true;

	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	bool enable = (item == -1) ? false : true;
	d->le_toolbarName->setEnabled(enable);
	// d->pb_toolbarPosition->setEnabled(enable && moveable);
	d->ck_toolbarOn->setEnabled(enable);
	d->ck_toolbarLocked->setEnabled(enable && moveable);
	// d->ck_toolbarStretch->setEnabled(enable && moveable);
	d->lw_selectedActions->setEnabled(enable && customizeable);
	d->tw_availActions->setEnabled(enable && customizeable);
	d->tb_up->setEnabled(enable && customizeable);
	d->tb_down->setEnabled(enable && customizeable);
	d->tb_left->setEnabled(enable && customizeable);
	d->tb_right->setEnabled(enable && customizeable);
	d->pb_deleteToolbar->setEnabled(enable);
	d->cb_toolbars->setEnabled(enable);

	d->tw_availActions->clear();
	d->lw_selectedActions->clear();

	if (!enable) {
		d->le_toolbarName->setText("");
		return;
	}

	noDirty = true;

	QString base = d->cb_toolbars->itemData(n).toString();
	ToolbarPrefs tb;
	tb = p->toolbars[base];

	d->le_toolbarName->setText(tb.name);
	d->ck_toolbarOn->setChecked(tb.on);
	d->ck_toolbarLocked->setChecked(tb.locked || !moveable);
	// d->ck_toolbarStretch->setChecked(tb.stretchable);

	{
		// Fill the TreeWidget with toolbar-specific actions
		QTreeWidget *tw = d->tw_availActions;
		QTreeWidgetItem *lastRoot = 0;

		foreach(ActionList* actionList, psi->actionList()->actionLists(p->class2id())) {
			QTreeWidgetItem *root = new QTreeWidgetItem(tw, lastRoot);
			lastRoot = root;
			root->setText(0, actionList->name());
			root->setData(0, Qt::UserRole, QString(""));
			root->setExpanded(true);

			QTreeWidgetItem *last = 0;
			QStringList actionNames = actionList->actions();
			QStringList::Iterator it2 = actionNames.begin();
			for (; it2 != actionNames.end(); ++it2) {
				IconAction *action = actionList->action(*it2);
				if (!action->isVisible())
					continue;
				QTreeWidgetItem *item = new QTreeWidgetItem(root, last);
				last = item;

				QString n = actionName((QAction *)action);
				if (!action->whatsThis().isEmpty()) {
					n += " - " + action->whatsThis();
				}
				item->setText(0, n);
				item->setIcon(0, action->icon());
				item->setData(0, Qt::UserRole, action->objectName());
			}
		}
		tw->resizeColumnToContents(0);
	}

	QStringList::Iterator it = tb.keys.begin();
	for (; it != tb.keys.end(); ++it) {
		addToolbarAction(d->lw_selectedActions, *it, p->class2id());
	}
	updateArrows();

	noDirty = false;
}

void OptionsTabToolbars::rebuildToolbarKeys()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	if (!d->cb_toolbars->count())
		return;
	int n = d->cb_toolbars->currentIndex();

	QStringList keys;

	int count = d->lw_selectedActions->count();
	for (int i = 0; i < count; i++) {
		keys << d->lw_selectedActions->item(i)->data(Qt::UserRole).toString();
	}

	QString base = d->cb_toolbars->itemData(n).toString();
	p->toolbars[base].keys = keys;

	emit dataChanged();
}

void OptionsTabToolbars::updateArrows()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	bool up = false, down = false, left = false, right = false;

	if (d->tw_availActions->currentItem() && !d->tw_availActions->currentItem()->data(0, Qt::UserRole).toString().isEmpty())
		right = true;
	QListWidgetItem *item = d->lw_selectedActions->currentItem();
	if (item) {
		left = true;

		// get numeric index of item
		int n = d->lw_selectedActions->row(item);

		int i = n;
		while (--i >= 0) {
			if (!d->lw_selectedActions->item(i)->isHidden()) {
				up = true;
				break;
			}
		}

		i = n;
		while (++i < d->lw_selectedActions->count()) {
			if (!d->lw_selectedActions->item(i)->isHidden()) {
				down = true;
				break;
			}
		}
	}

	d->tb_up->setEnabled(up);
	d->tb_down->setEnabled(down);
	d->tb_left->setEnabled(left);
	d->tb_right->setEnabled(right);
}

void OptionsTabToolbars::toolbarNameChanged()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	if (!d->cb_toolbars->count())
		return;

	QString name = d->le_toolbarName->text();

	int n = d->cb_toolbars->currentIndex();
	QString base = d->cb_toolbars->itemData(n).toString();
	p->toolbars[base].name = name;

	d->cb_toolbars->setItemText(d->cb_toolbars->findData(base), name);

	emit dataChanged();
}

void OptionsTabToolbars::toolbarActionUp()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	QListWidgetItem *item = d->lw_selectedActions->currentItem();
	if (!item)
		return;

	int row = d->lw_selectedActions->row(item);
	if (row > 0) {
		d->lw_selectedActions->takeItem(row);
		--row;
		while (row > 0 && d->lw_selectedActions->item(row)->isHidden()) {
			--row;
		}
		d->lw_selectedActions->insertItem(row, item);
		d->lw_selectedActions->setCurrentItem(item);
	}

	rebuildToolbarKeys();
	updateArrows();
}

void OptionsTabToolbars::toolbarActionDown()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	QListWidgetItem *item = d->lw_selectedActions->currentItem();
	if (!item)
		return;

	int row = d->lw_selectedActions->row(item);
	if (row < d->lw_selectedActions->count()) {
		d->lw_selectedActions->takeItem(row);
		++row;
		while (row < d->lw_selectedActions->count() && d->lw_selectedActions->item(row)->isHidden()) {
			++row;
		}
		d->lw_selectedActions->insertItem(row, item);
		d->lw_selectedActions->setCurrentItem(item);
	}

	rebuildToolbarKeys();
	updateArrows();
}

void OptionsTabToolbars::toolbarAddAction()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	QTreeWidgetItem *item = d->tw_availActions->currentItem();
	if (!item || item->data(0, Qt::UserRole).toString().isEmpty())
		return;

	addToolbarAction(d->lw_selectedActions, item->data(0, Qt::UserRole).toString(), p->class2id());
	rebuildToolbarKeys();
	updateArrows();
}

void OptionsTabToolbars::toolbarRemoveAction()
{
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	QListWidgetItem *item = d->lw_selectedActions->currentItem();
	if (!item)
		return;

	delete item;

	rebuildToolbarKeys();
	updateArrows();
}

void OptionsTabToolbars::toolbarDataChanged()
{
	if (noDirty)
		return;

	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	if (!d->cb_toolbars->count())
		return;
	int n = d->cb_toolbars->currentIndex();

	QString base = d->cb_toolbars->itemData(n).toString();
	ToolbarPrefs tb;
	tb = p->toolbars[base];

	tb.dirty = true;
	tb.name = d->le_toolbarName->text();
	tb.on = d->ck_toolbarOn->isChecked();
	tb.locked = d->ck_toolbarLocked->isChecked();
	// tb.stretchable = d->ck_toolbarStretch->isChecked();

	p->toolbars[base] = tb;

	emit dataChanged();
}

QString OptionsTabToolbars::actionName(const QAction *a)
{
	QString n = a->text(), n2;
	for (int i = 0; i < (int)n.length(); i++) {
		if (n[i] == '&' && n[i+1] != '&')
			continue;
		else if (n[i] == '&' && n[i+1] == '&')
			n2 += '&';
		else
			n2 += n[i];
	}

	return n2;
}

void OptionsTabToolbars::toolbarPosition()
{
#if 0
	LEGOPTFIXME
	LookFeelToolbarsUI *d = (LookFeelToolbarsUI *)w;
	if (!d->cb_toolbars->count())
		return;
	int n = d->cb_toolbars->currentIndex();

	PositionOptionsTabToolbars *posTbDlg = new PositionOptionsTabToolbars(w, &LEGOPTS.toolbars["mainWin"][n], n);
	connect(posTbDlg, SIGNAL(applyPressed()), SLOT(toolbarPositionApply()));

	posTbDlg->exec();
	delete posTbDlg;
#endif
}

void OptionsTabToolbars::toolbarPositionApply()
{
#if 0
	LEGOPTFIXME
	emit dataChanged();

	LEGOPTS.toolbars = LEGOPTS.toolbars;
	psi->buildToolbars();
#endif
}

void OptionsTabToolbars::selAct_selectionChanged(QListWidgetItem *)
{
	updateArrows();
}

void OptionsTabToolbars::avaAct_selectionChanged(QTreeWidgetItem *)
{
	updateArrows();
}
