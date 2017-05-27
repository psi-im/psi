/*
 * psicontactlistview.cpp - Psi-specific ContactListView-subclass
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#include "psicontactlistview.h"

#include "contactlistitem.h"
#include "contactlistmodel.h"
#include "contactlistproxymodel.h"
#include "contactlistviewdelegate.h"
#include "debug.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "psioptions.h"
#include "psitooltip.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QHelpEvent>
#include <QLayout>
#include <QMimeData>
#include <QTimer>

static const int recalculateTimerTimeout = 500;
static const QLatin1String groupIndentOption("options.ui.contactlist.group-indent");

class PsiContactListView::Private : public QObject
{
	Q_OBJECT
public:
	Private(PsiContactListView* p)
		: QObject(p)
		, allowAutoresize(false)
		, lv(p)
	{
		recalculateSizeTimer = new QTimer(this);
		recalculateSizeTimer->setInterval(recalculateTimerTimeout);
		connect(recalculateSizeTimer, SIGNAL(timeout()), SLOT(doRecalculateSize()));
	}

	bool allowResize() const
	{
		if ( !allowAutoresize )
			return false;

		if ( lv->window()->isMaximized() )
			return false;

		return true;
	}

	int calculateHeight(const QModelIndex &parent) const
	{
		int height = 0;
		int count = lv->model()->rowCount(parent);
		for(int i = 0; i < count; i++) {
			QModelIndex in = lv->model()->index(i, 0, parent);
			if(!lv->isIndexHidden(in)) {
				height += lv->sizeHintForIndex(in).height();
				if(lv->isExpanded(in)) {
					height += calculateHeight(in);
				}
			}
		}
		return height;
	}

private slots:
	void doRecalculateSize()
	{
		recalculateSizeTimer->stop();

		if( !allowResize() || !lv->updatesEnabled() || !lv->isVisible() ) {
			return;
		}

		int dh = lv->sizeHint().height() - lv->size().height();

		if ( dh != 0 ) {
			QWidget *topParent = lv->window();
			topParent->layout()->setEnabled( false ); // try to reduce some flicker

			const QRect topParentRect = topParent->frameGeometry();
			const QRect desktop = qApp->desktop()->availableGeometry(topParent);

			int newHeight = topParent->height() + dh;
			if( newHeight > desktop.height() ) {
				const int diff = newHeight - desktop.height();
				newHeight -= diff;
				dh -= diff;
			}

			if ( (topParentRect.bottom() + dh) > desktop.bottom() ) {
				int dy = desktop.bottom() - topParentRect.height() - dh;
				if ( dy < desktop.top() ) {
					newHeight -= abs( dy - desktop.top() );
					topParent->move( topParent->x(), desktop.top() );
				}
			}
			if ( determineAutoRosterSizeGrowSide()
				 && topParentRect.top() > desktop.top()
				 && topParentRect.bottom() < desktop.bottom() ) {
				topParent->move( topParent->x(), topParent->y() - dh );
			}
			if ( topParent->frameGeometry().top() < desktop.top() ) {
				topParent->move(topParent->x(), desktop.top());
			}
			topParent->resize( topParent->width(), newHeight );

			topParent->layout()->setEnabled( true );

			// issue a layout update
			lv->parentWidget()->layout()->update();
		}
	}

public slots:
	void recalculateSize()
	{
		recalculateSizeTimer->start();
	}

private:
	bool determineAutoRosterSizeGrowSide()
	{
		const QRect topParent = lv->window()->frameGeometry();
		const QRect desktop = qApp->desktop()->availableGeometry(lv->window());

		int top_offs    = abs( desktop.top()    - topParent.top() );
		int bottom_offs = abs( desktop.bottom() - topParent.bottom() );

		return (bottom_offs < top_offs);
	}

public:
	bool allowAutoresize;
	PsiContactListView* lv;
	QTimer* recalculateSizeTimer;
};

PsiContactListView::PsiContactListView(QWidget* parent)
	: ContactListDragView(parent)
{
	setIndentation(PsiOptions::instance()->getOption(groupIndentOption, 4).toInt());
	auto delegate = new ContactListViewDelegate(this);
	setItemDelegate(delegate);

	d = new Private(this);

	connect(PsiOptions::instance(), SIGNAL(optionChanged(QString)), SLOT(optionChanged(QString)));
	connect(delegate, SIGNAL(geometryUpdated()), d, SLOT(recalculateSize()));
	connect(this, SIGNAL(expanded(QModelIndex)), d, SLOT(recalculateSize()));
	connect(this, SIGNAL(collapsed(QModelIndex)), d, SLOT(recalculateSize()));
}

ContactListViewDelegate *PsiContactListView::itemDelegate() const
{
	return qobject_cast<ContactListViewDelegate*>(ContactListDragView::itemDelegate());
}

void PsiContactListView::optionChanged(const QString &option)
{
	if (option == groupIndentOption) {
		setIndentation(PsiOptions::instance()->getOption(groupIndentOption, 4).toInt());
		itemDelegate()->recomputeGeometry();
	}
}

void PsiContactListView::showToolTip(const QModelIndex& index, const QPoint& globalPos) const
{
	Q_UNUSED(globalPos);
	QString text = index.data(Qt::ToolTipRole).toString();
	PsiToolTip::showText(globalPos, text, this);
}

bool PsiContactListView::acceptableDragOperation(QDropEvent *e)
{
	ContactListItem *item = itemProxy(indexAt(e->pos()));

	if (!item)
		return false;

	PsiContact *contact = item->contact();

	if (!contact)
		return false;

	for (const QUrl& url: e->mimeData()->urls()) {
		const QFileInfo fi(url.toLocalFile());
		if (!fi.isDir() && fi.exists()) {
			return true;
		}
	}

	return false;
}

void PsiContactListView::dragEnterEvent(QDragEnterEvent *e)
{
	if(acceptableDragOperation(e)) {
		setCurrentIndex(indexAt(e->pos()));
		e->acceptProposedAction();
		return;
	}

	ContactListDragView::dragEnterEvent(e);
}

void PsiContactListView::dragMoveEvent(QDragMoveEvent *e)
{
	if(acceptableDragOperation(e)) {
		setCurrentIndex(indexAt(e->pos()));
		e->acceptProposedAction();
		return;
	}

	ContactListDragView::dragMoveEvent(e);
}

void PsiContactListView::dropEvent(QDropEvent *e)
{
	ContactListItem *item = itemProxy(indexAt(e->pos()));

	if (!item)
		return;

	PsiContact *contact = item->contact();

	if (!contact)
		return;

	QStringList files;
	foreach(const QUrl& url, e->mimeData()->urls()) {
		const QFileInfo fi(url.toLocalFile());
		if (!fi.isDir() && fi.exists()) {
			const QString fileName = QFileInfo(fi.isSymLink() ?
												   fi.symLinkTarget() : fi.absoluteFilePath()
												   ).canonicalFilePath();
			files.append(fileName);
		}
	}

	if(!files.isEmpty()) {
		e->acceptProposedAction();
		contact->account()->sendFiles(contact->jid(), files, true);
		return;
	}

	ContactListDragView::dropEvent(e);
}

void PsiContactListView::setModel(QAbstractItemModel *model)
{
	ContactListDragView::setModel(model);

	if (qobject_cast<ContactListProxyModel*>(model)) {
		connect(model, SIGNAL(recalculateSize()), d, SLOT(recalculateSize()));
	}
}

void PsiContactListView::alertContacts(const QModelIndexList &indexes)
{
	SLOW_TIMER(100);

	QModelIndex alertingIndex;

	for (const auto &index: indexes) {
		QModelIndex proxyIndex = this->proxyIndex(index);

		itemDelegate()->contactAlert(proxyIndex);

		if (index.data(ContactListModel::IsAlertingRole).toBool()) {
			alertingIndex = proxyIndex;
		}
	}

	if (alertingIndex.isValid() && PsiOptions::instance()->getOption("options.ui.contactlist.ensure-contact-visible-on-event").toBool()) {
		ensureVisible(alertingIndex);
	}
}

void PsiContactListView::animateContacts(const QModelIndexList &indexes, bool started)
{
	SLOW_TIMER(100);

	QModelIndexList proxyIndexes;
	for (const auto &index: indexes) {
		proxyIndexes << proxyIndex(index);
	}

	itemDelegate()->animateContacts(proxyIndexes, started);
}

void PsiContactListView::setAutoResizeEnabled(bool enabled)
{
	d->allowAutoresize = enabled;
}

QSize PsiContactListView::minimumSizeHint() const
{
	return QSize( minimumWidth(), minimumHeight() );
}

QSize PsiContactListView::sizeHint() const
{
	// save some CPU
	if ( !d->allowResize() )
		return minimumSizeHint();

	QSize s(QTreeView::sizeHint().width(), 0);
	const int border = 8;
	int h = border + d->calculateHeight(rootIndex());

	int minH = minimumSizeHint().height();
	if ( h < minH )
		h = minH + border;
	s.setHeight( h );
	return s;
}

#include "psicontactlistview.moc"
