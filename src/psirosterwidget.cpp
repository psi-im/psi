/*
 * psirosterwidget.cpp
 * Copyright (C) 2010  Yandex LLC (Michail Pishchagin)
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

#include "psirosterwidget.h"

#include "widgets/actionlineedit.h"
#include "widgets/iconaction.h"

#include <QVBoxLayout>
#include <QStackedWidget>
#include <QMessageBox>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QKeyEvent>
#include <QMimeData>

#include "contactlistdragmodel.h"
#include "psicontactlistview.h"
#include "psifilteredcontactlistview.h"
#include "contactlistproxymodel.h"
#include "psicontact.h"
#include "contactlistitem.h"
#ifdef MODELTEST
#include "modeltest.h"
#endif
#include "psioptions.h"
#include "psiaccount.h"
#include "debug.h"

static const QString contactSortStyleOptionPath = "options.ui.contactlist.contact-sort-style";
static const QString showOfflineOptionPath = "options.ui.contactlist.show.offline-contacts";
static const QString showHiddenOptionPath = "options.ui.contactlist.show.hidden-contacts-group";
static const QString showAgentsOptionPath = "options.ui.contactlist.show.agent-contacts";
static const QString showSelfOptionPath = "options.ui.contactlist.show.self-contact";
static const QString showStatusMessagesOptionPath = "options.ui.contactlist.status-messages.show";
static const QString allowAutoResizeOptionPath = "options.ui.contactlist.automatically-resize-roster";
static const QString showScrollBarOptionPath = "options.ui.contactlist.disable-scrollbar";
static const QString enableGroupsOptionPath = "options.ui.contactlist.enable-groups";

//----------------------------------------------------------------------------
// PsiRosterFilterProxyModel
//----------------------------------------------------------------------------

class PsiRosterFilterProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT
public:
	PsiRosterFilterProxyModel(QObject* parent)
		: QSortFilterProxyModel(parent)
	{
		sort(0, Qt::AscendingOrder);
		setFilterCaseSensitivity(Qt::CaseInsensitive);
		setSortLocaleAware(true);
	}

protected:
	// reimplemented
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
	{
		QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

		QStringList data;
		if (index.isValid()) {
			// TODO: also check for vCard value
			data
				<< index.data(Qt::DisplayRole).toString()
				<< index.data(ContactListModel::JidRole).toString();
		}

		foreach(const QString& str, data) {
			if (str.contains(filterRegExp()))
				return true;
		}

		return false;
	}

	// reimplemented
	bool lessThan(const QModelIndex& left, const QModelIndex& right) const
	{
		ContactListItem* item1 = static_cast<ContactListItem*>(left.internalPointer());
		ContactListItem* item2 = static_cast<ContactListItem*>(right.internalPointer());
		if (!item1 || !item2)
			return false;
		return item1->lessThan(item2);
	}
};

//----------------------------------------------------------------------------
// PsiRosterWidget
//----------------------------------------------------------------------------

PsiRosterWidget::PsiRosterWidget(QWidget* parent)
	: QWidget(parent)
	, stackedWidget_(0)
	, contactListPage_(0)
	, filterPage_(0)
	, contactListPageView_(0)
	, filterPageView_(0)
	, contactListModel_(0)
	, filterModel_(nullptr)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setMargin(0);
	layout->setSpacing(0);

	stackedWidget_ = new QStackedWidget(this);
	stackedWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(stackedWidget_);

	// contactListPage_
	contactListPage_ = new QWidget(0);
	contactListPage_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	stackedWidget_->addWidget(contactListPage_);

	QVBoxLayout* contactListPageLayout = new QVBoxLayout(contactListPage_);
	contactListPageLayout->setMargin(0);
	contactListPageView_ = new PsiContactListView(contactListPage_);
	contactListPageView_->installEventFilter(this);
	contactListPageView_->setObjectName("contactListView");
	contactListPageLayout->addWidget(contactListPageView_);

	// filterPage_
	filterPage_ = new QWidget(0);
	filterPage_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	filterPage_->installEventFilter(this);
	stackedWidget_->addWidget(filterPage_);

	QVBoxLayout* filterPageLayout = new QVBoxLayout(filterPage_);
	filterPageLayout->setMargin(0);
	filterPageLayout->setSpacing(0);

	filterEdit_ = new ActionLineEdit(filterPage_);
	QAction *clearAction = new IconAction("", "psi/clearChat", tr("Clear"), 0, this);
	connect(clearAction, SIGNAL(triggered()), SLOT(clearFilterEdit()));
	filterEdit_->addAction(clearAction);
	connect(filterEdit_, SIGNAL(textChanged(const QString&)), SLOT(filterEditTextChanged(const QString&)));
	filterEdit_->installEventFilter(this);
	filterPageLayout->addWidget(filterEdit_);

	filterPageView_ = new PsiFilteredContactListView(filterPage_);
	connect(filterPageView_, SIGNAL(quitFilteringMode()), SLOT(quitFilteringMode()));
	filterPageView_->installEventFilter(this);
	filterPageLayout->addWidget(filterPageView_);
}

PsiRosterWidget::~PsiRosterWidget()
{
}

void PsiRosterWidget::setContactList(PsiContactList* contactList)
{
	Q_ASSERT(contactList);
	Q_ASSERT(!contactList_);
	contactList_ = contactList;
	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));

	connect(contactList_, SIGNAL(showAgentsChanged(bool)), SLOT(showAgentsChanged(bool)));
	connect(contactList_, SIGNAL(showHiddenChanged(bool)), SLOT(showHiddenChanged(bool)));
	connect(contactList_, SIGNAL(showSelfChanged(bool)), SLOT(showSelfChanged(bool)));
	connect(contactList_, SIGNAL(showOfflineChanged(bool)), SLOT(showOfflineChanged(bool)));
	optionChanged(showAgentsOptionPath);
	optionChanged(showHiddenOptionPath);
	optionChanged(showSelfOptionPath);
	optionChanged(showOfflineOptionPath);
	optionChanged(contactSortStyleOptionPath);
	optionChanged(allowAutoResizeOptionPath);
	optionChanged(showScrollBarOptionPath);

	contactListModel_ = new ContactListDragModel(contactList_);
	contactListModel_->invalidateLayout();
	contactListModel_->setGroupsEnabled(PsiOptions::instance()->getOption(enableGroupsOptionPath).toBool());
	contactListModel_->setAccountsEnabled(true);
#ifdef MODELTEST
	new ModelTest(contactListModel_, this);
#endif

	ContactListProxyModel *contactListProxyModel = new ContactListProxyModel(this);
	contactListProxyModel->setSourceModel(contactListModel_);
#ifdef MODELTEST
	new ModelTest(contactListProxyModel, this);
#endif

	contactListPageView_->setModel(contactListProxyModel);
}

void PsiRosterWidget::optionChanged(const QString& option)
{
	if (!contactList_)
		return;
	if (option == contactSortStyleOptionPath) {
		contactList_->setContactSortStyle(PsiOptions::instance()->getOption(contactSortStyleOptionPath).toString());
	}
	else if (option == showAgentsOptionPath) {
		contactList_->setShowAgents(PsiOptions::instance()->getOption(showAgentsOptionPath).toBool());
	}
	else if (option == showHiddenOptionPath) {
		contactList_->setShowHidden(PsiOptions::instance()->getOption(showHiddenOptionPath).toBool());
	}
	else if (option == showSelfOptionPath) {
		contactList_->setShowSelf(PsiOptions::instance()->getOption(showSelfOptionPath).toBool());
	}
	else if (option == showOfflineOptionPath) {
		contactList_->setShowOffline(PsiOptions::instance()->getOption(showOfflineOptionPath).toBool());
	}
	else if (option == allowAutoResizeOptionPath) {
		contactListPageView_->setAutoResizeEnabled(PsiOptions::instance()->getOption(allowAutoResizeOptionPath).toBool());
	}
	else if (option == showScrollBarOptionPath) {
		contactListPageView_->setVerticalScrollBarPolicy(
			PsiOptions::instance()->getOption(showScrollBarOptionPath).toBool() ?
			Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded );
	}
	else if (option == enableGroupsOptionPath) {
		contactListModel_->setGroupsEnabled(PsiOptions::instance()->getOption(enableGroupsOptionPath).toBool());
	}
}

void PsiRosterWidget::showAgentsChanged(bool enabled)
{
	PsiOptions::instance()->setOption(showAgentsOptionPath, enabled);
}

void PsiRosterWidget::showHiddenChanged(bool enabled)
{
	PsiOptions::instance()->setOption(showHiddenOptionPath, enabled);
}

void PsiRosterWidget::showSelfChanged(bool enabled)
{
	PsiOptions::instance()->setOption(showSelfOptionPath, enabled);
}

void PsiRosterWidget::showOfflineChanged(bool enabled)
{
	PsiOptions::instance()->setOption(showOfflineOptionPath, enabled);
}

void PsiRosterWidget::setShowStatusMsg(bool enabled)
{
	PsiOptions::instance()->setOption(showStatusMessagesOptionPath, enabled);
}

void PsiRosterWidget::filterEditTextChanged(const QString& text)
{
	if (filterModel_)
		filterModel_->setFilterRegExp(QRegExp::escape(text));
}

void PsiRosterWidget::quitFilteringMode()
{
	setFilterModeEnabled(false);
}

void PsiRosterWidget::updateFilterMode()
{
	setFilterModeEnabled(!filterEdit_->text().isEmpty());
}

void PsiRosterWidget::setFilterModeEnabled(bool enabled)
{
	SLOW_TIMER(100);
	bool currentlyEnabled = stackedWidget_->currentWidget() == filterPage_;
	if (enabled == currentlyEnabled)
		return;

	PsiContactListView *selectionSource = 0;
	PsiContactListView *selectionDestination = 0;

	if (enabled) {
		Q_ASSERT(!filterModel_);
		filterModel_ = new PsiRosterFilterProxyModel(this);

		ContactListDragModel *clone = new ContactListDragModel(contactList_);
		clone->invalidateLayout();
		clone->setParent(filterModel_);

		filterModel_->setSourceModel(clone);
		filterPageView_->setModel(filterModel_);

		selectionSource = contactListPageView_;
		selectionDestination = filterPageView_;

		stackedWidget_->setCurrentWidget(filterPage_);

		filterEdit_->selectAll();
		filterEdit_->setFocus();
	}
	else {
		selectionSource = filterPageView_;
		selectionDestination = contactListPageView_;

		stackedWidget_->setCurrentWidget(contactListPage_);
		contactListPageView_->setFocus();

		delete filterModel_;
		filterModel_ = nullptr;
	}

	QMimeData* selection = selectionSource->selection();
	selectionDestination->restoreSelection(selection);
	delete selection;
}

void PsiRosterWidget::clearFilterEdit()
{
	if (filterEdit_->text().isEmpty())
		setFilterModeEnabled(false);
	else
		filterEdit_->setText("");
}

bool PsiRosterWidget::eventFilter(QObject* obj, QEvent* e)
{
	if (e->type() == QEvent::DeferredDelete || e->type() == QEvent::Destroy) {
		return false;
	}

#if 0
	// we probably don't want this behavior in Psi
	if (e->type() == QEvent::WindowDeactivate) {
		setFilterModeEnabled(false);
	}
#endif

	if (!isActiveWindow())
		return false;

	if (e->type() == QEvent::KeyPress) {
		QKeyEvent* ke = static_cast<QKeyEvent*>(e);
		QString text = ke->text().trimmed();
		if (!text.isEmpty() && (obj == contactListPageView_ || obj == contactListPage_)) {
			bool correctChar = (text[0].isLetterOrNumber() || text[0].isPunct()) &&
			                   (ke->modifiers() == Qt::NoModifier || ke->modifiers() == Qt::ShiftModifier);
			if (correctChar && !contactListPageView_->textInputInProgress()) {
				setFilterModeEnabled(true);
				filterEdit_->setText(text);
				return true;
			}
		}
		else if (ke->key() == Qt::Key_F3) {
			if (filterEdit_->isVisible() && filterEdit_->text().isEmpty()) {
				setFilterModeEnabled(false);
			}
			else {
				setFilterModeEnabled(true);
			}
			filterEdit_->setText("");
			return true;
		}

		if (obj == filterEdit_ || obj == filterPageView_ || obj == filterPage_) {
			if (ke->key() == Qt::Key_Escape) {
				setFilterModeEnabled(false);
				return true;
			}

			if (ke->key() == Qt::Key_Backspace && filterEdit_->text().isEmpty()) {
				setFilterModeEnabled(false);
				return true;
			}

			if(ke->key() == Qt::Key_End) {
				filterEdit_->setCursorPosition(filterEdit_->text().length());
				return true;
			}

			if(ke->key() == Qt::Key_Home) {
				filterEdit_->setCursorPosition(0);
				return true;
			}

			if (filterPageView_->handleKeyPressEvent(ke)) {
				return true;
			}
		}
	}

	return QObject::eventFilter(obj, e);
}

#include "psirosterwidget.moc"
