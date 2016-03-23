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

#include <QHelpEvent>
#include <QLayout>
#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>
#include <QFileInfo>
#include <QMimeData>

#include "psicontactlistviewdelegate.h"
#include "psitooltip.h"
#include "psioptions.h"
#include "contactlistmodel.h"
#include "contactlistproxymodel.h"
#include "contactlistgroup.h"
#include "contactlistitemproxy.h"
#include "psicontact.h"
#include "psiaccount.h"

static const int recalculateTimerTimeout = 2000;

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
	//setLayoutDirection(Qt::RightToLeft);
	setIndentation(0); // bigger values make roster look weird
	setItemDelegate(new PsiContactListViewDelegate(this));

	d = new Private(this);

	connect(this, SIGNAL(expanded(QModelIndex)), d, SLOT(recalculateSize()));
	connect(this, SIGNAL(collapsed(QModelIndex)), d, SLOT(recalculateSize()));
}

PsiContactListViewDelegate* PsiContactListView::itemDelegate() const
{
	return static_cast<PsiContactListViewDelegate*>(ContactListDragView::itemDelegate());
}

void PsiContactListView::showToolTip(const QModelIndex& index, const QPoint& globalPos) const
{
	Q_UNUSED(globalPos);
	QString text = index.data(Qt::ToolTipRole).toString();
	PsiToolTip::showText(globalPos, text, this);
}

bool PsiContactListView::acceptableDragOperation(QDropEvent *e)
{
	ContactListItemProxy* ip = itemProxy(indexAt(e->pos()));
	if(ip) {
		PsiContact *pc = dynamic_cast<PsiContact*>(ip->item());
		if(pc) {
			foreach(const QUrl& url, e->mimeData()->urls()) {
				const QFileInfo fi(url.toLocalFile());
				if (!fi.isDir() && fi.exists()) {
					return true;
				}
			}
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
	ContactListItemProxy* ip = itemProxy(indexAt(e->pos()));
	if(ip) {
		PsiContact *pc = dynamic_cast<PsiContact*>(ip->item());
		if(pc) {
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
				pc->account()->sendFiles(pc->jid(), files, true);
				return;
			}
		}
	}

	ContactListDragView::dropEvent(e);
}

void PsiContactListView::setModel(QAbstractItemModel* model)
{
	ContactListDragView::setModel(model);
	QAbstractItemModel* connectToModel = realModel();
	if (dynamic_cast<ContactListModel*>(connectToModel)) {
		connect(connectToModel, SIGNAL(contactAlert(const QModelIndex&)), SLOT(contactAlert(const QModelIndex&)));
	}
	if (dynamic_cast<ContactListProxyModel*>(model)) {
		connect(model, SIGNAL(recalculateSize()), d, SLOT(recalculateSize()));
	}
}

void PsiContactListView::contactAlert(const QModelIndex& realIndex)
{
	QModelIndex index = proxyIndex(realIndex);
	itemDelegate()->contactAlert(index);

	bool alerting = index.data(ContactListModel::IsAlertingRole).toBool();
	if (alerting && PsiOptions::instance()->getOption("options.ui.contactlist.ensure-contact-visible-on-event").toBool()) {
		ensureVisible(index);
	}
}

void PsiContactListView::doItemsLayoutStart()
{
	ContactListDragView::doItemsLayoutStart();
	itemDelegate()->clearAlerts();
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
