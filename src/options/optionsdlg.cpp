/*
 * optionsdlg.cpp
 * Copyright (C) 2003-2009  Michail Pishchagin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "optionsdlg.h"
#include "optionstab.h"
#include "common.h"
#include "psicon.h"
#include "fancylabel.h"
#include "iconset.h"
#include "iconwidget.h"

#include <QLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QPen>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>
#include <QItemDelegate>
#include <QScrollBar>

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
#include "opt_avcall.h"
#include "opt_advanced.h"
#include "opt_shortcuts.h"
#include "opt_tree.h"

#ifdef PSI_PLUGINS
#include "opt_plugins.h"
#endif

#include "../avcall/avcall.h"

//----------------------------------------------------------------------------
// OptionsTabsDelegate
//----------------------------------------------------------------------------

class OptionsTabsDelegate : public QItemDelegate
{
public:
	OptionsTabsDelegate(QObject* parent)
		: QItemDelegate(parent)
	{}

	// reimplemented
	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		QStyleOptionViewItemV4 opt(option);
		opt.showDecorationSelected = true;
		opt.rect.adjust(0, 0, 0, -1);

		painter->save();
		QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
		style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

		QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
		QSize iconSize;
#if 0
		if (icon.availableSizes().isEmpty())
			iconSize = icon.availableSizes().first();
		else
#endif
			iconSize = QSize(16, 16);
		QRect iconRect = opt.rect;
		iconRect.setLeft(4);
		iconRect.setWidth(iconSize.width());
		icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);

		QRect textRect = opt.rect;
		textRect.setLeft(iconRect.right() + 8);
		QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
		                          ? QPalette::Normal : QPalette::Disabled;
		if (cg == QPalette::Normal && !(option.state & QStyle::State_Active)) {
			cg = QPalette::Inactive;
		}
		if (option.state & QStyle::State_Selected) {
			painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
		}
		else {
			painter->setPen(option.palette.color(cg, QPalette::Text));
		}
		painter->drawText(textRect, index.data(Qt::DisplayRole).toString(), Qt::AlignLeft | Qt::AlignVCenter);
		drawFocus(painter, option, option.rect); // not opt, because drawFocus() expects normal rect
		painter->restore();

		painter->save();
		QPen pen(QColor(0xE0, 0xE0, 0xE0));
		QVector<qreal> dashes;
		dashes << 1.0 << 1.0;
		pen.setCapStyle(Qt::FlatCap);
		pen.setDashPattern(dashes);

		painter->setPen(pen);
		painter->drawLine(option.rect.left(), option.rect.bottom(), option.rect.right(), option.rect.bottom());
		painter->restore();
	}

	// reimplemented
	void drawFocus(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect) const
	{
		if (option.state & QStyle::State_HasFocus && rect.isValid()) {
			QStyleOptionFocusRect o;
			o.QStyleOption::operator=(option);
			o.rect = rect.adjusted(0, 0, 0, -1);
			QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled)
				? QPalette::Normal : QPalette::Disabled;
			o.backgroundColor = option.palette.color(cg, (option.state & QStyle::State_Selected)
				? QPalette::Highlight : QPalette::Window);
			QApplication::style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter);
		}
	}

	// reimplemented
	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
		QSize iconSize;
#if 0
		if (!icon.availableSizes().isEmpty())
			iconSize = icon.availableSizes().first();
		else
#endif
			iconSize = QSize(16, 16);

		int width = iconSize.width();
		width += 8;
		width += option.fontMetrics.width(index.data(Qt::DisplayRole).toString());
		width += 8;

		int height = iconSize.height();
		height = qMax(height, option.fontMetrics.height());
		height += 8;

		return QSize(width, height);
	}
};

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
	void currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
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
	QHash<QString, QWidget*> id2widget;
	QList<OptionsTab*> tabs;

	QMap<QString, QByteArray> changedMap;
};

OptionsDlg::Private::Private(OptionsDlg *d, PsiCon *_psi)
{
	dlg = d;
	psi = _psi;
	noDirty = false;
	dirty = false;

	dlg->lb_pageTitle->setScaledContents(32, 32);

	// FancyLabel stinks
	dlg->lb_pageTitle->hide();

	QAbstractItemDelegate* previousDelegate = dlg->lv_tabs->itemDelegate();
	delete previousDelegate;
	dlg->lv_tabs->setItemDelegate(new OptionsTabsDelegate(dlg->lv_tabs));

	createTabs();
	createChangedMap();

	QStyleOptionViewItem option;
	option.fontMetrics = dlg->lv_tabs->fontMetrics();
	int maxWidth = 0;
	foreach(OptionsTab* opttab, tabs) {
		//qWarning("Adding tab %s...", (const char *)opttab->id());
		opttab->setData(psi, dlg);
		connect(opttab, SIGNAL(dataChanged()), SLOT(dataChanged()));
		//connect(opttab, SIGNAL(addWidgetChangedSignal(QString, QCString)), SLOT(addWidgetChangedSignal(QString, QCString)));
		connect(opttab, SIGNAL(noDirty(bool)), SLOT(noDirtySlot(bool)));
		connect(opttab, SIGNAL(connectDataChanged(QWidget *)), SLOT(connectDataChanged(QWidget *)));

		if ( opttab->id().isEmpty() )
			continue;

		dlg->lv_tabs->addItem(opttab->tabName());
		QListWidgetItem* item = dlg->lv_tabs->item(dlg->lv_tabs->count() - 1);
		dlg->lv_tabs->setCurrentItem(item);
		QModelIndex index = dlg->lv_tabs->currentIndex();
		if (opttab->tabIcon())
			item->setData(Qt::DecorationRole, opttab->tabIcon()->icon());
		item->setData(Qt::UserRole, opttab->id());

		int width = dlg->lv_tabs->itemDelegate()->sizeHint(option, index).width() +
		            dlg->lv_tabs->verticalScrollBar()->sizeHint().width();
		maxWidth = qMax(width, maxWidth);
	}

	dlg->lv_tabs->setFixedWidth(maxWidth);

	openTab( "application" );
	connect(dlg->lv_tabs, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SLOT(currentItemChanged(QListWidgetItem*, QListWidgetItem*)));

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

	OptionsTabApplication* applicationTab = new OptionsTabApplication(this);
	applicationTab->setHaveAutoUpdater(psi->haveAutoUpdater());
	tabs.append( applicationTab );
	tabs.append( new OptionsTabChat(this) );
	tabs.append( new OptionsTabEvents(this) );
	tabs.append( new OptionsTabStatus(this) );
	tabs.append( new OptionsTabAppearance(this) );
	//tabs.append( new OptionsTabIconsetSystem(this) );
	//tabs.append( new OptionsTabIconsetRoster(this) );
	//tabs.append( new OptionsTabIconsetEmoticons(this) );
	tabs.append( new OptionsTabGroupchat(this) );
	tabs.append( new OptionsTabSound(this) );
	if(AvCallManager::isSupported())
		tabs.append( new OptionsTabAvCall(this) );
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
		foreach(OptionsTab* opttab, tabs) {
			if ( opttab->id() == id ) {
				tab = opttab->widget(); // create the widget
				if ( !tab )
					continue;

				// TODO: how about QScrollView for large tabs?
				// idea: maybe do it only for those, whose sizeHint is bigger than ws_tabs'
				QWidget *w = new QWidget(dlg->ws_tabs);
				w->setObjectName("QWidgetStack/tab");
				QVBoxLayout *vbox = new QVBoxLayout(w);
				vbox->setSpacing(0);
				vbox->setMargin(0);

				tab->setParent(w);
				vbox->addWidget(tab);
				if ( !opttab->stretchable() )
					vbox->addStretch();

				dlg->ws_tabs->addWidget(w);
				id2widget[id] = w;
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
			qWarning("OptionsDlg::Private::itemSelected(): could not create widget for id '%s'", qPrintable(id));
			return;
		}
	}

	foreach(OptionsTab* opttab, tabs) {
		if ( opttab->id() == id ) {
			dlg->lb_pageTitle->setText( opttab->name() );
			dlg->lb_pageTitle->setHelp( opttab->desc() );
			dlg->lb_pageTitle->setPsiIcon( opttab->psiIcon() );

			break;
		}
	}

	dlg->ws_tabs->setCurrentWidget( tab );

	for (int i = 0; i < dlg->lv_tabs->count(); ++i) {
		QListWidgetItem* item = dlg->lv_tabs->item(i);
		if (item->data(Qt::UserRole).toString() == id) {
			dlg->lv_tabs->setCurrentItem(item);
			break;
		}
	}
}

void OptionsDlg::Private::connectDataChanged(QWidget *widget)
{
	foreach(QWidget* w, widget->findChildren<QWidget*>()) {
		QVariant isOption = w->property("isOption");
		if (isOption.isValid() && !isOption.toBool()) {
			continue;
		}
		QMap<QString, QByteArray>::Iterator it2 = changedMap.find( w->metaObject()->className() );
		if ( it2 != changedMap.end() ) {
			disconnect(w, changedMap[w->metaObject()->className()], this, SLOT(dataChanged()));
			connect(w, changedMap[w->metaObject()->className()], SLOT(dataChanged()));
		}
	}
}

void OptionsDlg::Private::currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	Q_UNUSED(previous);
	if (!current)
		return;

	openTab(current->data(Qt::UserRole).toString());
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

	foreach(OptionsTab* opttab, tabs) {
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
	setupUi(this);
	pb_apply = buttonBox->button(QDialogButtonBox::Apply);

	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private(this, psi);
	setModal(false);
	d->psi->dialogRegister(this);

	setWindowTitle(CAP(windowTitle()));
	resize(640, 480);

	connect(buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(doOk()));
	connect(pb_apply,SIGNAL(clicked()),SLOT(doApply()));
	connect(buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(reject()));

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
