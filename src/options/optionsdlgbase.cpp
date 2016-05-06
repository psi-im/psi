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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "optionsdlgbase.h"
#include "optionstab.h"
#include "common.h"
#include "psicon.h"
#include "fancylabel.h"
#include "iconset.h"
#include "iconwidget.h"
#include "psiiconset.h"

#include <QLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QPen>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>
#include <QItemDelegate>
#include <QScrollBar>

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
#ifdef HAVE_QT5
		QStyleOptionViewItem opt(option);
#else
		QStyleOptionViewItemV4 opt(option);
#endif
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
		{
			int s = PsiIconset::instance()->system().iconSize();
			iconSize = QSize(s,s);
		}
		QRect iconRect = opt.rect;
		QRect textRect = opt.rect;
		iconRect.setWidth(iconSize.width());
		if (opt.direction == Qt::LeftToRight) {
			iconRect.moveLeft(4);
			textRect.setLeft(iconRect.right() + 8);
		}
		else {
			iconRect.moveRight(opt.rect.right() - 4);
			textRect.setRight(iconRect.left() - 8);
		}
		icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);

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
		{
			int s = PsiIconset::instance()->system().iconSize();
			iconSize = QSize(s,s);
		}

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

class OptionsDlgBase::Private : public QObject
{
	Q_OBJECT
public:
	Private(OptionsDlgBase *dlg, PsiCon *_psi);
	void setTabs(QList<OptionsTab*> t);

public slots:
	void doApply();
	void openTab(QString id);
	void enableCommonControls(bool enable);

private slots:
	void currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
	void dataChanged();
	void noDirtySlot(bool);
	void createChangedMap();

	//void addWidgetChangedSignal(QString widgetName, QCString signal);
	void connectDataChanged(QWidget *);

public:
	OptionsDlgBase *dlg;
	PsiCon *psi;
	bool dirty, noDirty;
	QHash<QString, QWidget*> id2widget;
	QList<OptionsTab*> tabs;

	QMap<QString, QByteArray> changedMap;
};

OptionsDlgBase::Private::Private(OptionsDlgBase *d, PsiCon *_psi)
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

	createChangedMap();

	connect(dlg->lv_tabs, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SLOT(currentItemChanged(QListWidgetItem*, QListWidgetItem*)));

	dirty = false;
	dlg->pb_apply->setEnabled(false);
}

void OptionsDlgBase::Private::setTabs(QList<OptionsTab*> t)
{
	int maxWidth = 0;
	QStyleOptionViewItem option;

	option.fontMetrics = dlg->lv_tabs->fontMetrics();
	tabs = t;
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
	if (tabs.count() == 1) {
		dlg->lv_tabs->setVisible(false);
	}
}


void OptionsDlgBase::Private::createChangedMap()
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

//void OptionsDlgBase::Private::addWidgetChangedSignal(QString widgetName, QCString signal)
//{
//	changedMap.insert(widgetName, signal);
//}

void OptionsDlgBase::Private::openTab(QString id)
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
			qWarning("OptionsDlgBase::Private::itemSelected(): could not create widget for id '%s'", qPrintable(id));
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

// enable/disable list widget and dialog buttons
void OptionsDlgBase::Private::enableCommonControls(bool enable)
{
	dlg->buttonBox->setEnabled(enable);
	dlg->lv_tabs->setEnabled(enable);
}

void OptionsDlgBase::Private::connectDataChanged(QWidget *widget)
{
	foreach(QWidget* w, widget->findChildren<QWidget*>()) {
		QVariant isOption = w->property("isOption"); // set to false for ignored widgets
		if (isOption.isValid() && !isOption.toBool()) {
			continue;
		}
		QMap<QString, QByteArray>::Iterator it2 = changedMap.find( w->metaObject()->className() );
		if ( it2 != changedMap.end() ) {
			connect(w, it2.value(), SLOT(dataChanged()), Qt::UniqueConnection);
		}
	}
}

void OptionsDlgBase::Private::currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
	Q_UNUSED(previous);
	if (!current)
		return;

	openTab(current->data(Qt::UserRole).toString());
}

void OptionsDlgBase::Private::dataChanged()
{
	if ( dirty )
		return;

	if ( !noDirty ) {
		dirty = true;
		dlg->pb_apply->setEnabled(true);
	}
}

void OptionsDlgBase::Private::noDirtySlot(bool d)
{
	noDirty = d;
}

void OptionsDlgBase::Private::doApply()
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
// OptionsDlgBase
//----------------------------------------------------------------------------

OptionsDlgBase::OptionsDlgBase(PsiCon *psi, QWidget *parent)
	: QDialog(parent)
{
	setupUi(this);
	pb_apply = buttonBox->button(QDialogButtonBox::Apply);

	setAttribute(Qt::WA_DeleteOnClose);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint | Qt::WindowContextHelpButtonHint);
	d = new Private(this, psi);
	setModal(false);

	connect(buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(doOk()));
	connect(pb_apply,SIGNAL(clicked()),SLOT(doApply()));
	connect(buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(reject()));

}

OptionsDlgBase::~OptionsDlgBase()
{
	d->psi->dialogUnregister(this);
	delete d;
}

PsiCon *OptionsDlgBase::psi() const
{
	return d->psi;
}

void OptionsDlgBase::openTab(const QString& id)
{
	d->openTab(id);
}

void OptionsDlgBase::setTabs(QList<OptionsTab *> tabs)
{
	d->setTabs(tabs);
}

void OptionsDlgBase::doOk()
{
	doApply();
	accept();
}

void OptionsDlgBase::doApply()
{
	d->doApply();
}

void OptionsDlgBase::enableCommonControls(bool enable)
{
	d->enableCommonControls(enable);
}

#include "optionsdlgbase.moc"
