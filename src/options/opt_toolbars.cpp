#include "opt_toolbars.h"

#include "common.h"
#include "iconaction.h"
#include "iconwidget.h"
#include "pluginmanager.h"
#include "psiactionlist.h"
#include "psicon.h"
#include "psioptions.h"
#include "psitoolbar.h"
#include "ui_opt_lookfeel_toolbars.h"

#include <QAction>
#include <QEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QRegExp>

#define CHAT_TOOLBAR 0
#define GROUPCHAT_TOOLBAR 1
#define ROSTER_TOOLBAR 2

class LookFeelToolbarsUI : public QWidget, public Ui::LookFeelToolbars {
public:
    LookFeelToolbarsUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabToolbars
//----------------------------------------------------------------------------

class OptionsTabToolbars::Private {
public:
    QList<ToolbarPrefs>       toolbars;
    class OptionsTabToolbars *q;

    PsiActionList::ActionsType class2id()
    {
        int ret = int(PsiActionList::Actions_Common);
        ret |= int(PsiActionList::Actions_MainWin);
        return static_cast<PsiActionList::ActionsType>(ret);
    }

    PsiActionList::ActionsType class2idChat()
    {
        int ret = int(PsiActionList::Actions_Common);
        ret |= int(PsiActionList::Actions_Chat);
        return static_cast<PsiActionList::ActionsType>(ret);
    }

    PsiActionList::ActionsType class2idGroupchat()
    {
        int ret = int(PsiActionList::Actions_Common);
        ret |= int(PsiActionList::Actions_Groupchat);
        return static_cast<PsiActionList::ActionsType>(ret);
    }

    PsiActionList::ActionsType currentType()
    {
        PsiActionList::ActionsType type;
        LookFeelToolbarsUI *       d = static_cast<LookFeelToolbarsUI *>(q->w);

        if (d->cb_toolbar->currentIndex() == CHAT_TOOLBAR) {
            type = class2idChat();
        } else if (d->cb_toolbar->currentIndex() == GROUPCHAT_TOOLBAR) {
            type = class2idGroupchat();
        } else {
            type = class2id();
        }
        return type;
    }

    QTreeWidgetItem *insertAction(QTreeWidgetItem *root, IconAction *action, QTreeWidgetItem *preceding)
    {
        if (!action->isVisible())
            return nullptr;
        QTreeWidgetItem *item = new QTreeWidgetItem(root, preceding);

        QObject::connect(action, &IconAction::destroyed, q, [this, root, item]() {
            QString actionName = item->data(0, Qt::UserRole).toString();
            delete root->takeChild(root->indexOfChild(item));
            LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(q->w);

            if (root->data(0, Qt::UserRole + 1) == d->cb_toolbar->currentIndex()) {
                for (int i = 0; i < d->lw_selectedActions->count(); i++) {
                    if (d->lw_selectedActions->item(i)->data(Qt::UserRole).toString() == actionName) {
                        delete d->lw_selectedActions->takeItem(i);
                        q->updateArrows();
                    }
                }
            }
        });

        QString n = q->actionName(static_cast<QAction *>(action));
        if (!action->toolTip().isEmpty() && action->toolTip() != n) {
            n += " - " + action->toolTip();
        }
        n.replace(" Plugin", "");
        item->setText(0, n);
        item->setIcon(0, action->icon());
        item->setData(0, Qt::UserRole, action->objectName());
        return item;
    }
};

OptionsTabToolbars::OptionsTabToolbars(QObject *parent) :
    OptionsTab(parent, "toolbars", "", tr("Toolbars"), tr("Configure Psi toolbars"), "psi/toolbars")
{
    p    = new Private();
    p->q = this;

    noDirty = false;
}

QWidget *OptionsTabToolbars::widget()
{
    if (w)
        return nullptr;

    w                     = new LookFeelToolbarsUI();
    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);

    connect(d->pb_addToolbar, SIGNAL(clicked()), SLOT(toolbarAdd()));
    connect(d->pb_deleteToolbar, SIGNAL(clicked()), SLOT(toolbarDelete()));
    connect(d->cb_toolbar, SIGNAL(activated(int)), SLOT(toolbarSelectionChanged(int)));
    connect(d->le_toolbarName, SIGNAL(textChanged(const QString &)), SLOT(toolbarNameChanged()));
    // connect(d->pb_toolbarPosition, SIGNAL(clicked()), SLOT(toolbarPosition()));
    connect(d->tb_up, SIGNAL(clicked()), SLOT(toolbarActionUp()));
    connect(d->tb_down, SIGNAL(clicked()), SLOT(toolbarActionDown()));
    connect(d->tb_right, SIGNAL(clicked()), SLOT(toolbarAddAction()));
    connect(d->tb_left, SIGNAL(clicked()), SLOT(toolbarRemoveAction()));

    connect(d->ck_toolbarOn, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
    connect(d->ck_toolbarLocked, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
    // connect(d->ck_toolbarStretch, SIGNAL(toggled(bool)), SLOT(toolbarDataChanged()));
    connect(d->lw_selectedActions, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
            SLOT(selAct_selectionChanged(QListWidgetItem *)));
    connect(d->tw_availActions, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
            SLOT(avaAct_selectionChanged(QTreeWidgetItem *)));

    connect(d->pb_deleteToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));
    connect(d->tb_up, SIGNAL(clicked()), SIGNAL(dataChanged()));
    connect(d->tb_down, SIGNAL(clicked()), SIGNAL(dataChanged()));
    connect(d->tb_left, SIGNAL(clicked()), SIGNAL(dataChanged()));
    connect(d->tb_right, SIGNAL(clicked()), SIGNAL(dataChanged()));
    connect(d->pb_addToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));
    connect(d->pb_deleteToolbar, SIGNAL(clicked()), SIGNAL(dataChanged()));

    d->tw_availActions->header()->hide();

    return w;
    // TODO: add ToolTip for earch widget
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

OptionsTabToolbars::~OptionsTabToolbars() { delete p; }

/**
 * setData is called by the OptionsDlg private, after calling
 * the constructor, to assign the PsiCon object and the parent window
 * to all tabs.
 * /par psi_: PsiCon* object to apply the changes when needed
 * /par parent_: QWidget which is parent from the current object
 */
void OptionsTabToolbars::setData(PsiCon *psi_, QWidget *parent_)
{
    // the Psi con object is needed to apply the changes
    // the parent object is needed to show some popups
    psi    = psi_;
    parent = parent_;
}

/*void OptionsTabToolbars::setCurrentToolbar(PsiToolBar *t)
{
    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);

    if ( pb_apply->isEnabled() )
        return;

    QMap<int, Private::ToolbarItem>::Iterator it = p->toolbars.begin();
    for ( ; it != p->toolbars.end(); ++it ) {
        if ( it.data().group == t->group() && it.data().index == t->groupIndex() ) {
            d->cb_toolbar->setCurrentIndex( it.key() );
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
    for (const ToolbarPrefs &toolbar : qAsConst(p->toolbars)) {
        PsiToolBar::structToOptions(o, toolbar);
    }
}

void OptionsTabToolbars::restoreOptions()
{
    if (!w)
        return;

    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);

    PsiOptions *o = PsiOptions::instance();

    QStringList toolbarBases;
    toolbarBases = o->getChildOptionNames("options.ui.contactlist.toolbars", true, true);

    // toolbarBases is chaotic. Need to sort before do anything.
    QStringList sortedToolbarBases;
    for (int i = 0; !toolbarBases.isEmpty(); ++i) {
        int index = toolbarBases.indexOf(QRegExp(QString(".*m%1$").arg(i)));
        if (index >= 0) {
            sortedToolbarBases << toolbarBases.takeAt(index);
        }
    }

    QString chatToolbarName      = tr("Chat");
    QString groupchatToolbarName = tr("Groupchat");

    for (const QString &base : qAsConst(sortedToolbarBases)) {
        ToolbarPrefs tb;

        tb.id = o->getOption(base + ".key").toString();
        if (tb.id.isEmpty()) {
            tb.id = ToolbarPrefs().id;
            qWarning("broken toolbar setting: %s", qPrintable(base));
        }
        tb.name   = o->getOption(base + ".name").toString();
        tb.on     = o->getOption(base + ".visible").toBool();
        tb.locked = o->getOption(base + ".locked").toBool();
        // tb.stretchable = o->getOption(base + ".stretchable").toBool();
        tb.dock = static_cast<Qt3Dock>(o->getOption(base + ".dock.position").toInt()); // FIXME
        // tb.index = o->getOption(base + ".dock.index").toInt();
        tb.nl = o->getOption(base + ".dock.nl").toBool();
        // tb.extraOffset = o->getOption(base + ".dock.extra-offset").toInt();
        tb.keys = o->getOption(base + ".actions").toStringList();

        p->toolbars << tb;
        if (tb.name == "Chat") {
            d->cb_toolbar->addItem(chatToolbarName);
        } else if (tb.name == "Groupchat") {
            d->cb_toolbar->addItem(groupchatToolbarName);
        } else {
            d->cb_toolbar->addItem(tb.name);
        }
    }

    d->cb_toolbar->setCurrentIndex(0);
    toolbarSelectionChanged(0);
}

//----------------------------------------------------------------------------

void OptionsTabToolbars::toolbarAdd()
{
    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);

    ToolbarPrefs tb;
    int          j = 0;
    bool         ok;
    do {
        ok      = true;
        tb.name = QObject::tr("<unnamed%1>").arg(j++);
        for (const ToolbarPrefs &other : qAsConst(p->toolbars)) {
            if (other.name == tb.name) {
                ok = false;
                break;
            }
        }
    } while (!ok);

    tb.on     = false;
    tb.locked = false;
    // tb.stretchable = false;
    tb.keys.clear();

    tb.dock = Qt3Dock_Top;
    // tb.index = i;
    tb.nl = true;
    // tb.extraOffset = 0;

    // tb.dirty = true;

    p->toolbars << tb;

    d->cb_toolbar->addItem(tb.name);

    d->cb_toolbar->setCurrentIndex(d->cb_toolbar->count() - 1);
    toolbarSelectionChanged(d->cb_toolbar->currentIndex());

    d->le_toolbarName->setFocus();
}

void OptionsTabToolbars::toolbarDelete()
{
    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);
    int                 n = d->cb_toolbar->currentIndex();

    noDirty = true;
    toolbarSelectionChanged(-1);

    p->toolbars.removeAt(n);

    d->cb_toolbar->removeItem(n);

    noDirty = false;
    toolbarSelectionChanged(d->cb_toolbar->currentIndex());
}

void OptionsTabToolbars::addToolbarAction(QListWidget *parent, QString name, int toolbarId)
{
    ActionList     actions = psi->actionList()->suitableActions(static_cast<PsiActionList::ActionsType>(toolbarId));
    const QAction *action  = static_cast<QAction *>(actions.action(name));
    if (!action)
        return;
    addToolbarAction(parent, action, name);
}

void OptionsTabToolbars::addToolbarAction(QListWidget *parent, const QAction *action, QString name, int pos)
{
    QListWidgetItem *item = new QListWidgetItem();

    QString n = actionName(action);
    if (!action->toolTip().isEmpty() && action->toolTip() != n) {
        n += " - " + action->toolTip();
    }
    n.replace(" Plugin", "");
    item->setText(n);
    item->setData(Qt::UserRole, name);
    item->setIcon(action->icon());
    item->setHidden(!action->isVisible());

    if (pos == -1) {
        parent->addItem(item);
    } else {
        parent->insertItem(pos, item);
    }
}

void OptionsTabToolbars::toolbarSelectionChanged(int item)
{
    if (noDirty)
        return;

    int n = item;
    //    PsiToolBar *toolBar = 0;
    //    if ( item != -1 )
    //        toolBar = psi->findToolBar( p->toolbars[n].group, p->toolbars[n].index );

    bool customizeable = true;
    bool moveable      = true;

    LookFeelToolbarsUI *d      = static_cast<LookFeelToolbarsUI *>(w);
    bool                enable = (item != -1);
    d->le_toolbarName->setEnabled(enable);
    // d->pb_toolbarPosition->setEnabled(enable && moveable);
    d->ck_toolbarOn->setEnabled(enable);
    d->ck_toolbarLocked->setEnabled(enable);
    d->ck_toolbarLocked->setVisible(item >= ROSTER_TOOLBAR || item < CHAT_TOOLBAR);
    // d->ck_toolbarStretch->setEnabled(enable && moveable);
    d->lw_selectedActions->setEnabled(enable && customizeable);
    d->tw_availActions->setEnabled(enable && customizeable);
    d->tb_up->setEnabled(enable && customizeable);
    d->tb_down->setEnabled(enable && customizeable);
    d->tb_left->setEnabled(enable && customizeable);
    d->tb_right->setEnabled(enable && customizeable);
    d->pb_deleteToolbar->setEnabled((item >= CHAT_TOOLBAR && item < ROSTER_TOOLBAR) ? false : enable);
    d->cb_toolbar->setEnabled(enable);
    d->w_toolbarName->setVisible(item >= ROSTER_TOOLBAR || item < CHAT_TOOLBAR);

    d->tw_availActions->clear();
    d->lw_selectedActions->clear();

    if (!enable) {
        d->le_toolbarName->setText("");
        return;
    }

    noDirty = true;

    ToolbarPrefs tb;
    tb = p->toolbars[n];

    if (item > 1) {
        d->le_toolbarName->setText(tb.name);
        d->ck_toolbarLocked->setChecked(tb.locked || !moveable);
    }
    d->ck_toolbarOn->setChecked(tb.on);
    // d->ck_toolbarStretch->setChecked(tb.stretchable);

    {
        // Fill the TreeWidget with toolbar-specific actions
        QTreeWidget *    tw       = d->tw_availActions;
        QTreeWidgetItem *lastRoot = nullptr;

        QList<ActionList *> lists = psi->actionList()->actionLists(p->currentType());

        for (ActionList *actionList : lists) {
            QTreeWidgetItem *root = new QTreeWidgetItem(tw, lastRoot);
            lastRoot              = root;
            root->setText(0, actionList->name());
            root->setData(0, Qt::UserRole, QString(""));
            root->setData(0, Qt::UserRole + 1, n);
            root->setExpanded(true);

            connect(actionList, &ActionList::actionAdded, this, &OptionsTabToolbars::onActionAdded,
                    Qt::UniqueConnection);

            QTreeWidgetItem *last        = nullptr;
            QStringList      actionNames = actionList->actions();
            for (auto const &name : actionNames) {
                IconAction *action = actionList->action(name);
                if (!action) {
                    continue;
                }
                auto item = p->insertAction(root, action, last);
                if (item)
                    last = item;
            }
        }
        tw->resizeColumnToContents(0);
    }

    for (auto const &name : qAsConst(tb.keys)) {
        addToolbarAction(d->lw_selectedActions, name, p->currentType());
    }
    updateArrows();

    noDirty = false;
}

void OptionsTabToolbars::onActionAdded(IconAction *action)
{
    auto                listName = static_cast<ActionList *>(sender())->name();
    LookFeelToolbarsUI *d        = static_cast<LookFeelToolbarsUI *>(w);

    QTreeWidgetItemIterator it(d->tw_availActions);
    bool                    available = false;
    while (*it) {
        if ((*it)->text(0) == listName) {
            p->insertAction(*it, action, nullptr);
            available = true;
            break;
        }
        ++it;
    }
    if (!available)
        return;

    ToolbarPrefs tb;
    int          n = d->cb_toolbar->currentIndex();
    tb             = p->toolbars[n];
    int pos        = tb.keys.indexOf(action->objectName());
    if (pos == -1)
        return; // not preferred

    addToolbarAction(d->lw_selectedActions, action, action->objectName(), pos);
    updateArrows();
}

void OptionsTabToolbars::rebuildToolbarKeys()
{
    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);
    if (!d->cb_toolbar->count())
        return;
    int n = d->cb_toolbar->currentIndex();

    QStringList keys;

    int count = d->lw_selectedActions->count();
    for (int i = 0; i < count; i++) {
        keys << d->lw_selectedActions->item(i)->data(Qt::UserRole).toString();
    }

    p->toolbars[n].keys = keys;

    emit dataChanged();
}

void OptionsTabToolbars::updateArrows()
{
    LookFeelToolbarsUI *d  = static_cast<LookFeelToolbarsUI *>(w);
    bool                up = false, down = false, left = false, right = false;

    if (d->tw_availActions->currentItem()
        && !d->tw_availActions->currentItem()->data(0, Qt::UserRole).toString().isEmpty())
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
    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);
    if (!d->cb_toolbar->count())
        return;

    QString name = d->le_toolbarName->text();

    int n               = d->cb_toolbar->currentIndex();
    p->toolbars[n].name = name;

    d->cb_toolbar->setItemText(n, name);

    emit dataChanged();
}

void OptionsTabToolbars::toolbarActionUp()
{
    LookFeelToolbarsUI *d    = static_cast<LookFeelToolbarsUI *>(w);
    QListWidgetItem *   item = d->lw_selectedActions->currentItem();
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
    LookFeelToolbarsUI *d    = static_cast<LookFeelToolbarsUI *>(w);
    QListWidgetItem *   item = d->lw_selectedActions->currentItem();
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
    LookFeelToolbarsUI *d    = static_cast<LookFeelToolbarsUI *>(w);
    QTreeWidgetItem *   item = d->tw_availActions->currentItem();
    if (!item || item->data(0, Qt::UserRole).toString().isEmpty())
        return;

    addToolbarAction(d->lw_selectedActions, item->data(0, Qt::UserRole).toString(), p->currentType());
    rebuildToolbarKeys();
    updateArrows();
}

void OptionsTabToolbars::toolbarRemoveAction()
{
    LookFeelToolbarsUI *d    = static_cast<LookFeelToolbarsUI *>(w);
    QListWidgetItem *   item = d->lw_selectedActions->currentItem();
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

    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);
    if (!d->cb_toolbar->count())
        return;
    int n = d->cb_toolbar->currentIndex();

    ToolbarPrefs tb = p->toolbars[n];

    // tb.dirty = true;
    if (n > 1) {
        tb.name   = d->le_toolbarName->text();
        tb.locked = d->ck_toolbarLocked->isChecked();
    }
    tb.on = d->ck_toolbarOn->isChecked();
    // tb.stretchable = d->ck_toolbarStretch->isChecked();

    p->toolbars[n] = tb;

    emit dataChanged();
}

QString OptionsTabToolbars::actionName(const QAction *a)
{
    QString n = a->text(), n2;
    for (int i = 0; i < int(n.length()); i++) {
        if (n[i] == '&' && n[i + 1] != '&')
            continue;
        else if (n[i] == '&' && n[i + 1] == '&')
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
    LookFeelToolbarsUI *d = static_cast<LookFeelToolbarsUI *>(w);
    if (!d->cb_toolbar->count())
        return;
    int n = d->cb_toolbar->currentIndex();

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

void OptionsTabToolbars::selAct_selectionChanged(QListWidgetItem *) { updateArrows(); }

void OptionsTabToolbars::avaAct_selectionChanged(QTreeWidgetItem *) { updateArrows(); }
