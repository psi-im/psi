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
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QColorDialog>

#include "accountlabel.h"
#include "stretchwidget.h"
#include "iconset.h"

//----------------------------------------------------------------------------
// WbDlg
//----------------------------------------------------------------------------

WbDlg::WbDlg(SxeSession* session, PsiAccount* pa) {
	groupChat_ = session->groupChat();
	pending_ = 0;
	keepOpen_ = false;
	allowEdits_ = true;

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
	wbWidget_ = new WbWidget(session, this);
	wbWidget_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	vb1->addWidget(wbWidget_);

	// Bottom (tool) area
	act_save_ = new IconAction(tr("Save session"), "psi/saveBoard", tr("Save the contents of the whiteboard"), 0, this );
	act_geometry_ = new IconAction(tr("Change the geometry"), "psi/whiteboard", tr("Change the geometry"), 0, this );
	act_clear_ = new IconAction(tr("End session"), "psi/clearChat", tr("Clear the whiteboard"), 0, this );
	act_end_ = new IconAction(tr("End session"), "psi/closetab", tr("End session"), 0, this );

	// Black is the default color
	QPixmap pixmap(16, 16);
	pixmap.fill(QColor(Qt::black));
	act_color_ = new QAction(QIcon(pixmap), tr("Stroke color"), this);
	pixmap.fill(QColor(Qt::lightGray));
	act_fill_ = new QAction(QIcon(pixmap), tr("Fill color"), this);

	act_widths_ = new IconAction(tr("Stroke width" ), "psi/drawPaths", tr("Stroke width"), 0, this );
	act_modes_ = new IconAction(tr("Edit mode" ), "psi/select", tr("Edit mode"), 0, this );
	group_widths_ = new QActionGroup(this);
	group_modes_ = new QActionGroup(this);

	connect(act_color_, SIGNAL(triggered()), SLOT(setStrokeColor()));
	connect(act_fill_, SIGNAL(triggered()), SLOT(setFillColor()));
	connect(group_widths_, SIGNAL(triggered(QAction *)), SLOT(setStrokeWidth(QAction *)));
	connect(group_modes_, SIGNAL(triggered(QAction *)), SLOT(setMode(QAction *)));
	connect(act_save_, SIGNAL(triggered()), SLOT(save()));
	connect(act_geometry_, SIGNAL(triggered()), SLOT(setGeometry()));
	connect(act_clear_, SIGNAL(triggered()), wbWidget_, SLOT(clear()));
	connect(act_end_, SIGNAL(triggered()), SLOT(endSession()));

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

	IconAction* action;
	action = new IconAction(tr("Select"), "psi/select", tr("Select"), 0, group_modes_ );
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
	// action = new IconAction(tr( "Draw lines"), "psi/drawLines", tr("Draw lines"), 0, group_modes_ );
	// action->setData(QVariant(WbWidget::DrawLine));
	// action->setCheckable(true);
	// action = new IconAction(tr( "Draw ellipses"), "psi/drawEllipses", tr("Draw ellipses"), 0, group_modes_ );
	// action->setData(QVariant(WbWidget::DrawEllipse));
	// action->setCheckable(true);
	// action = new IconAction(tr( "Draw circles"), "psi/drawCircles", tr("Draw circles"), 0, group_modes_ );
	// action->setData(QVariant(WbWidget::DrawCircle));
	// action->setCheckable(true);
	// action = new IconAction(tr( "Draw rectangles"), "psi/drawRectangles", tr("Draw rectangles"), 0, group_modes_ );
	// action->setData(QVariant(WbWidget::DrawRectangle));
	// action->setCheckable(true);
	// 	action = new IconAction(tr( "Add text"), "psi/addText", tr("Add text"), 0, group_modes_ );
	// 	action->setData(QVariant(WbWidget::DrawText));
	// 	action->setCheckable(true);
	action = new IconAction(tr( "Add images"), "psi/addImage", tr("Add images"), 0, group_modes_ );
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
	toolbar_->addAction(act_save_);
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
	le_jid_->setText(QString("%1 (session: %2)").arg(session->target().full()).arg(session->session()));
	le_jid_->setCursorPosition(0);
	le_jid_->setToolTip(session->target().full());

	// update the widget icon
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/whiteboard").icon());
#endif
	
	setWindowOpacity(double(qMax(MINIMUM_OPACITY, PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt())) / 100);

	resize(PsiOptions::instance()->getOption("options.ui.chat.size").toSize());
}

WbDlg::~WbDlg() {
	// terminate the underlying SXE session
	session()->endSession();

	emit sessionEnded(this);
}

SxeSession* WbDlg::session() const {
	 return wbWidget_->session();
}

bool WbDlg::allowEdits() const {
	return allowEdits_;
}

void WbDlg::setAllowEdits(bool a) {
	allowEdits_ = a;
	if(!allowEdits_)
		wbWidget_->setMode(WbWidget::Scroll);
}

void WbDlg::peerLeftSession(const Jid &peer) {
	le_jid_->setText(tr("%1 left (session: %2).").arg(peer.full()).arg(session()->session()));
}

void WbDlg::endSession() {
	if(sender() == act_end_) {
		int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to end the session?\nThe contents of the whiteboard will be lost."), tr("&Yes"), tr("&No"));
		if(n != 0)
			return;
	}
	setAttribute(Qt::WA_DeleteOnClose);
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
	if(e->key() == Qt::Key_Escape && !PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool())
		close();
	else if(e->key() == Qt::Key_W && e->state() & Qt::ControlButton && !PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool())
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
	if(PsiOptions::instance()->getOption("options.ui.chat.delete-contents-after").toString() == "instant")
		endSession();
	else {
		if(PsiOptions::instance()->getOption("options.ui.chat.delete-contents-after").toString() == "hour")
			setSelfDestruct(60);
		else if(PsiOptions::instance()->getOption("options.ui.chat.delete-contents-after").toString() == "day")
			setSelfDestruct(60 * 24);
	}
}

void WbDlg::resizeEvent(QResizeEvent *e) {
	if (PsiOptions::instance()->getOption("options.ui.remember-window-sizes").toBool()) {
		PsiOptions::instance()->setOption("options.ui.chat.size", e->size());
	}
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
	pm_settings_->addAction(act_save_);
	pm_settings_->addAction(act_clear_);
	pm_settings_->addAction(act_end_);
}

void WbDlg::setGeometry() {
	// TODO: make a proper dialog
	QSize size;

	bool ok;
	size.setWidth(QInputDialog::getInteger(this, tr("Set new width:"), tr("Width:"), static_cast<int>(wbWidget_->scene()->sceneRect().width()), 10, 100000, 10, &ok));
	if(!ok) {
		return;
	}

	size.setHeight(QInputDialog::getInteger(this, tr("Set new height:"), tr("Height:"), static_cast<int>(wbWidget_->scene()->sceneRect().height()), 10, 100000, 10, &ok));
	if(!ok)
		return;

	wbWidget_->setSize(size);
}

void WbDlg::save() {
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Whitebaord"),
								QDir::homePath(),
								tr("Scalable Vector Graphics (*.svg)"));
	fileName = fileName.trimmed();
	if(!fileName.endsWith(".svg", Qt::CaseInsensitive))
		fileName += ".svg";

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return;

	QTextStream stream(&file);
	wbWidget_->session()->document().save(stream, 2);

	file.close();
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
	cap += session()->target().full();

	setWindowTitle(cap);
}
