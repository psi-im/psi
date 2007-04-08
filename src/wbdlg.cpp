/*
 * wbdlg.cpp - dialog for whiteboarding
 * Copyright (C) 2006  Joonas Govenius
 *
 * Originally developed from:
 * chatdlg.cpp - dialog for handling chats
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include "wbdlg.h"

#include <QMessageBox>

#include "accountlabel.h"
#include "stretchwidget.h"
#include "iconset.h"

//----------------------------------------------------------------------------
// WbDlg
//----------------------------------------------------------------------------

WbDlg::WbDlg(const Jid &target, const QString &session, const Jid &ownJid, bool groupChat, PsiAccount *pa) {
	if ( option.brushedMetal )
		setAttribute(Qt::WA_MacMetalStyle);

	groupChat_ = groupChat;
	pending_ = 0;
	keepOpen_ = false;
	allowEdits_ = true;
	queueing_ = false;
	selfDestruct_ = 0;

	QVBoxLayout *vb1 = new QVBoxLayout(this);

	// first row
	le_jid_ = new QLineEdit(this);
	le_jid_->setReadOnly(true);
	le_jid_->setFocusPolicy(Qt::NoFocus);
	le_jid_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
	lb_ident_ = new AccountLabel(this);
	lb_ident_->setAccount(pa);
	lb_ident_->setShowJid(false);
	lb_ident_->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
	QHBoxLayout *hb1 = new QHBoxLayout();
	hb1->addWidget(le_jid_);
	hb1->addWidget(lb_ident_);
	vb1->addLayout(hb1);

	// mid area
	wbWidget_ = new WbWidget(session, ownJid.full(), QSize(600, 600), this);
	wbWidget_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	vb1->addWidget(wbWidget_);
	connect(wbWidget_, SIGNAL(newWb(QDomElement)), SLOT(doSend(QDomElement)));

	// Bottom (tool) area
	act_end_ = new IconAction(tr("End session"), "psi/closetab", tr("End session"), 0, this );
	act_clear_ = new IconAction(tr("End session"), "psi/clearChat", tr("Clear the whiteboard"), 0, this );
	act_geometry_ = new IconAction(tr("Change the geometry"), "psi/whiteboard", tr("Change the geometry"), 0, this );
	// Black is the default color
	QPixmap pixmap(16, 16);
	pixmap.fill(QColor(Qt::black));
	act_color_ = new QAction(QIcon(pixmap), tr("Stroke color"), this);
	pixmap.fill(QColor(Qt::transparent));
	act_fill_ = new QAction(QIcon(pixmap), tr("Fill color"), this);
	act_widths_ = new IconAction(tr("Stroke width" ), "psi/drawPaths", tr("Stroke width"), 0, this );
	act_modes_ = new IconAction(tr("Edit mode" ), "psi/select", tr("Edit mode"), 0, this );
	group_widths_ = new QActionGroup(this);
	group_modes_ = new QActionGroup(this);

	connect(act_color_, SIGNAL(triggered()), SLOT(setStrokeColor()));
	connect(act_fill_, SIGNAL(triggered()), SLOT(setFillColor()));
	connect(group_widths_, SIGNAL(triggered(QAction *)), SLOT(setStrokeWidth(QAction *)));
	connect(group_modes_, SIGNAL(triggered(QAction *)), SLOT(setMode(QAction *)));
	connect(act_end_, SIGNAL(activated()), SLOT(endSession()));
	connect(act_clear_, SIGNAL(activated()), wbWidget_, SLOT(clear()));
	connect(act_geometry_, SIGNAL(activated()), SLOT(setGeometry()));

	pixmap = QPixmap(2, 2);
	pixmap.fill(QColor(Qt::black));
	QAction* widthaction = new QAction(QIcon(pixmap), tr("Thin stroke"), group_widths_);
	widthaction->setData(QVariant(1));
	widthaction->setCheckable(true);
	widthaction->trigger();
	pixmap = QPixmap(6, 6);
	pixmap.fill(QColor(Qt::black));
	widthaction = new QAction(QIcon(pixmap), tr("Medium stroke"), group_widths_);
	widthaction->setData(QVariant(3));
	widthaction->setCheckable(true);
	pixmap = QPixmap(12, 12);
	pixmap.fill(QColor(Qt::black));
	widthaction = new QAction(QIcon(pixmap), tr("Thick stroke"), group_widths_);
	widthaction->setData(QVariant(6));
	widthaction->setCheckable(true);

	IconAction* action = new IconAction(tr("Select"), "psi/select", tr("Select"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::Select));
	action->setCheckable(true);
	action = new IconAction(tr( "Translate"), "psi/translate", tr("Translate"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::Translate));
	action->setCheckable(true);
	action = new IconAction(tr( "Rotate"), "psi/rotate", tr("Rotate"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::Rotate));
	action->setCheckable(true);
	action = new IconAction(tr( "Scale"), "psi/scale", tr("Scale"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::Scale));
	action->setCheckable(true);
	action = new IconAction(tr( "Erase"), "psi/erase", tr("Erase"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::Erase));
	action->setCheckable(true);
	QAction *separator = new QAction(group_modes_);
	separator->setSeparator(true);
	action = new IconAction(tr( "Scroll view"), "psi/scroll", tr("Scroll"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::Scroll));
	action->setCheckable(true);
	separator = new QAction(group_modes_);
	separator->setSeparator(true);
	action = new IconAction(tr( "Draw paths"), "psi/drawPaths", tr("Draw paths"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::DrawPath));
	action->setCheckable(true);
	action->trigger();
	action = new IconAction(tr( "Draw lines"), "psi/drawLines", tr("Draw lines"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::DrawLine));
	action->setCheckable(true);
	action = new IconAction(tr( "Draw ellipses"), "psi/drawEllipses", tr("Draw ellipses"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::DrawEllipse));
	action->setCheckable(true);
	action = new IconAction(tr( "Draw circles"), "psi/drawCircles", tr("Draw circles"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::DrawCircle));
	action->setCheckable(true);
	action = new IconAction(tr( "Draw rectangles"), "psi/drawRectangles", tr("Draw rectangles"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::DrawRectangle));
	action->setCheckable(true);
// 	action = new IconAction(tr( "Add text"), "psi/addText", tr("Add text"), 0, group_modes_ );
// 	action->setData(QVariant(WbWidget::DrawText));
// 	action->setCheckable(true);
	action = new IconAction(tr( "Add an image"), "psi/addImage", tr("Add an image"), 0, group_modes_ );
	action->setData(QVariant(WbWidget::DrawImage));
	action->setCheckable(true);

	menu_widths_ = new QMenu(this);
	menu_widths_->addActions(group_widths_->actions());
	act_widths_->setMenu(menu_widths_);

	menu_modes_ = new QMenu(this);
	menu_modes_->addActions(group_modes_->actions());
	act_modes_->setMenu(menu_modes_);

	toolbar_ = new QToolBar(tr("Whiteboard toolbar"), this);
	toolbar_->setIconSize(QSize(16,16));
	toolbar_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
	toolbar_->addAction(act_end_);
	toolbar_->addAction(act_clear_);
	toolbar_->addAction(act_geometry_);
	toolbar_->addWidget(new StretchWidget(this));
	toolbar_->addAction(act_fill_);
	toolbar_->addAction(act_color_);
	toolbar_->addAction(act_widths_);
	toolbar_->addAction(act_modes_);
	vb1->addWidget(toolbar_);

	// Context menu
	pm_settings_ = new QMenu(this);
	connect(pm_settings_, SIGNAL(aboutToShow()), SLOT(buildMenu()));

	X11WM_CLASS("whiteboard");

	// set the Jid -> le_jid.
	target_ = target;
	le_jid_->setText(QString("%1: %2").arg(session).arg(target_.full()));
	le_jid_->setCursorPosition(0);
	le_jid_->setToolTip(target_.full());

	// update the widget icon
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/whiteboard").icon());
#endif
	
	setWindowOpacity(double(qMax(MINIMUM_OPACITY, PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt())) / 100);

	resize(option.sizeChatDlg);
}

WbDlg::~WbDlg() {
// 	emit sessionEnded(session());
}

void WbDlg::incomingWbElement(const QDomElement &wb, const Jid &sender)
{
	if(queueing_) {
		incomingElement incoming = incomingElement();
		incoming.wb = wb;
		incoming.sender = sender;
		queuedIncomingElements_.append(incoming);
	} else {
		// Save the information of last processed edit
		lastWb_["sender"] = sender.full();
		lastWb_["hash"] = wb.attribute("hash");
		// Process the wb element
		wbWidget_->processWb(wb);
		if(!isActiveWindow()) {
			++pending_;
			updateCaption();
			doFlash(true);
			if(option.raiseChatWindow)
				bringToFront(this, false);
		}
		keepOpen_ = true;
		QTimer::singleShot(1000, this, SLOT(setKeepOpenFalse()));
	}
}

const bool WbDlg::groupChat() const {
	return groupChat_;
}

const Jid WbDlg::target() const {
	return target_;
}

const QString WbDlg::session() const {
	return wbWidget_->session();
}

const Jid WbDlg::ownJid() const {
	return Jid(wbWidget_->ownJid());
}

bool WbDlg::allowEdits() const {
	return allowEdits_;
}

void WbDlg::setAllowEdits(bool a) {
	allowEdits_ = a;
	if(!allowEdits_)
		wbWidget_->setMode(WbWidget::Scroll);
}

void WbDlg::peerLeftSession() {
	setQueueing(true);
	le_jid_->setText(tr("%1: %2 left the session.").arg(session()).arg(target_.full()));
}

void WbDlg::setQueueing(bool q) {
	if(queueing_ && !q) {
		queueing_ = false;
		// Process queued elements
		while(!queuedOutgoingElements_.isEmpty())
			emit newWbElement(queuedOutgoingElements_.takeFirst(), target(), groupChat());
		while(!queuedIncomingElements_.isEmpty()) {
			incomingElement incoming = queuedIncomingElements_.takeFirst();
			incomingWbElement(incoming.wb, incoming.sender);
		}
		// Delete snapshot
		foreach(WbItem* item, snapshot_)
			item->deleteLater();
		snapshot_.clear();
		le_jid_->setText(tr("%1: %2").arg(session()).arg(target_.full()));
	} else if(!queueing_ && q) {
		queueing_ = true;
		snapshot_ = wbWidget_->scene->elements();
	}
}

void WbDlg::eraseQueueUntil(QString sender, QString hash) {
	for(int i = 0; i < queuedIncomingElements_.size(); i++) {
		// Check each queued element if the sender matches
		if(queuedIncomingElements_.at(i).sender.full() == sender && queuedIncomingElements_.at(i).wb.attribute("hash") == hash) {
			// Erase the queue up to and including the matching edit
			while(i >= 0) {
				queuedIncomingElements_.removeFirst();
				i--;
			}
			// Set the information of last processed edit
			lastWb_["sender"] = sender;
			lastWb_["hash"] = hash;
			return;
		}
	}
}

QList<WbItem*> WbDlg::snapshot() const {
	return snapshot_;
}

void WbDlg::setImporting(bool i) {
	wbWidget_->setImporting(i);
	if(i) {
		wbWidget_->clear(false);
		queuedIncomingElements_.clear();
		queuedOutgoingElements_.clear();
	}
}

QHash<QString, QString> WbDlg::lastWb() const {
	return lastWb_;
}

void WbDlg::setLastWb(const QString &sender, const QString &hash) {
	lastWb_["sender"] = sender;
	lastWb_["hash"] = hash;
}

void WbDlg::endSession() {
	if(sender() == act_end_) {
		int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to end the session?\nThe contents of the whiteboard will be lost."), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;
	}
	setAttribute(Qt::WA_DeleteOnClose);
	emit sessionEnded(session());
	close();
}

void WbDlg::activated() {
	if(pending_ > 0) {
		pending_ = 0;
		updateCaption();
	}
	doFlash(false);
}

void WbDlg::keyPressEvent(QKeyEvent *e) {
	if(e->key() == Qt::Key_Escape && !option.useTabs)
		close();
	else if(e->key() == Qt::Key_W && e->state() & Qt::ControlButton && !option.useTabs)
		close();
	else
		e->ignore();
}

void WbDlg::closeEvent(QCloseEvent *e) {
	e->accept();
	if(testAttribute(Qt::WA_DeleteOnClose))
		return;
	if(keepOpen_) {
		int n = QMessageBox::information(this, tr("Warning"), tr("A new whiteboard message was just received.\nDo you still want to close the window?"), tr("&Yes"), tr("&No"));
		if(n != 0) {
			e->ignore();
			return;
		}
	}

	// destroy the dialog if delChats is dcClose
	if(option.delChats == dcClose)
		endSession();
	else {
		if(option.delChats == dcHour)
			setSelfDestruct(60);
		else if(option.delChats == dcDay)
			setSelfDestruct(60 * 24);
	}
}

void WbDlg::resizeEvent(QResizeEvent *e) {
	if(option.keepSizes)
		option.sizeChatDlg = e->size();
}

void WbDlg::showEvent(QShowEvent *) {
	setSelfDestruct(0);
}

void WbDlg::changeEvent(QEvent *e) {
	if (e->type() == QEvent::ActivationChange && isActiveWindow()) {
		activated();
	}
	e->ignore();
}

void WbDlg::setStrokeColor() {
	QColor newColor = QColorDialog::getColor();
	if(newColor.isValid()) {
		QPixmap pixmap(16, 16);
		pixmap.fill(newColor);
		act_color_->setIcon(QIcon(pixmap));
		wbWidget_->setStrokeColor(newColor);
	}
}

void WbDlg::setFillColor() {
	QColor newColor = QColorDialog::getColor();
	if(newColor.isValid()) {
		QPixmap pixmap(16, 16);
		pixmap.fill(newColor);
		act_fill_->setIcon(QIcon(pixmap));
		wbWidget_->setFillColor(newColor);
	}
}

void WbDlg::setStrokeWidth(QAction *a) {
	wbWidget_->setStrokeWidth(a->data().toInt());
}

void WbDlg::setMode(QAction *a) {
	if(allowEdits_)
		wbWidget_->setMode(WbWidget::Mode(a->data().toInt()));
	else
		wbWidget_->setMode(WbWidget::Scroll);
}

void WbDlg::doSend(const QDomElement &wb) {
	if(queueing_)
		queuedOutgoingElements_.append(wb);
	else
		emit newWbElement(wb, target(), groupChat());
}

void WbDlg::setKeepOpenFalse() {
	keepOpen_ = false;
}

void WbDlg::buildMenu()
{
	pm_settings_->clear();
	pm_settings_->addAction(act_modes_);
	pm_settings_->addAction(act_widths_);
	pm_settings_->addAction(act_color_);
	pm_settings_->insertSeparator();
	pm_settings_->addAction(act_end_);
	pm_settings_->addAction(act_clear_);
}

void WbDlg::setGeometry() {
	// TODO: do a proper dialog
	bool ok;
        int width = QInputDialog::getInteger(this, tr("Set new width:"), tr("Width:"), static_cast<int>(wbWidget_->scene->sceneRect().width()), 10, 100000, 10, &ok);
	if(!ok)
		return;
        int height = QInputDialog::getInteger(this, tr("Set new height:"), tr("Height:"), static_cast<int>(wbWidget_->scene->sceneRect().height()), 10, 100000, 10, &ok);
	if(!ok)
		return;

	WbItem* root = wbWidget_->scene->findWbItem("root");
	if(root) {
		QDomElement _svg = root->svg();
		wbWidget_->scene->queueAttributeEdit("root", "viewBox", QString("%1 %2 %3 %4").arg(0).arg(0).arg(width).arg(height), _svg.attribute("viewBox"));
		_svg.setAttribute("viewBox", QString("%1 %2 %3 %4").arg(0).arg(0).arg(width).arg(height));
		root->parseSvg(_svg);
	}
}

void WbDlg::contextMenuEvent(QContextMenuEvent * e) {
	pm_settings_->exec(QCursor::pos());
	e->accept();
}

void WbDlg::setSelfDestruct(int minutes) {
	if(minutes <= 0) {
		if(selfDestruct_) {
			delete selfDestruct_;
			selfDestruct_ = 0;
		}
		return;
	}

	if(!selfDestruct_) {
		selfDestruct_ = new QTimer(this);
		connect(selfDestruct_, SIGNAL(timeout()), SLOT(endSession()));
	}

	selfDestruct_->start(minutes * 60000);
}

void WbDlg::updateCaption() {
	QString cap = "";

	if(pending_ > 0) {
		cap += "* ";
		if(pending_ > 1)
			cap += QString("[%1] ").arg(pending_);
	}
	cap += target_.full();

	setWindowTitle(cap);
}
