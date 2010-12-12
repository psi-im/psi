/*
 * wbwidget.cpp - a widget for processing and showing whiteboard
 *				messages.
 * Copyright (C) 2006  Joonas Govenius
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

#include "wbwidget.h"
#include "wbnewpath.h"
#include "wbnewimage.h"

#include <QMouseEvent>
#include <QApplication>

WbWidget::WbWidget(SxeSession* session, QWidget *parent) : QGraphicsView(parent) {
	newWbItem_ = 0;
	adding_ = 0;
	addVertex_ = false;
	strokeColor_ = Qt::black;
	fillColor_ = Qt::transparent;
	strokeWidth_ = 1;
	session_ = session;

//	setCacheMode(CacheBackground);
	setRenderHint(QPainter::Antialiasing);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	// Make the scroll bars always stay on because otherwise the resize event can cause
	// an infinite loop as the effective size of the widget changes when scroll bars are
	// added/removed
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	// create the scene
	scene_ = new WbScene(session_, this);
	scene_->setItemIndexMethod(QGraphicsScene::NoIndex);
	setRenderHint(QPainter::Antialiasing);
	setTransformationAnchor(AnchorUnderMouse);
	setResizeAnchor(AnchorViewCenter);
	setScene(scene_);

	// render the initial document
	rerender();
	// rerender on update
	connect(session_, SIGNAL(documentUpdated(bool)), SLOT(handleDocumentUpdated(bool)));

	// add the initial items
	const QDomNodeList children = session_->document().documentElement().childNodes();
	for(uint i = 0; i < children.length(); i++) {
		const QDomNode node = children.at(i);
		if(node.isElement()) {
			queueNodeInspection(node.toElement());
		}
	}
	inspectNodes();

	// add new items as nodes are added
	// remove/add items if corresponding nodes are moved
	connect(session_, SIGNAL(nodeAdded(QDomNode, bool)), SLOT(queueNodeInspection(QDomNode)));
	connect(session_, SIGNAL(nodeMoved(QDomNode, bool)), SLOT(queueNodeInspection(QDomNode)));
	// remove items if corresponding nodes are deleted
	connect(session_, SIGNAL(nodeToBeRemoved(QDomNode, bool)), SLOT(removeWbItem(QDomNode)));
	connect(session_, SIGNAL(nodeToBeRemoved(QDomNode, bool)), SLOT(checkForRemovalOfId(QDomNode)));
	// adjust the viewBox as necessary
	connect(session_, SIGNAL(nodeAdded(QDomNode, bool)), SLOT(checkForViewBoxChange(QDomNode)));
	connect(session_, SIGNAL(nodeMoved(QDomNode, bool)), SLOT(checkForViewBoxChange(QDomNode)));
	connect(session_, SIGNAL(chdataChanged(QDomNode, bool)), SLOT(checkForViewBoxChange(QDomNode)));

	// set the default mode to select
	setMode(Select);

	// set the initial size
	if(session_->document().documentElement().hasAttribute("viewBox"))
		checkForViewBoxChange(session_->document().documentElement().attributeNode("viewBox"));
	else {
		QSize size;
		QRectF rect = scene_->sceneRect();
		size.setWidth(rect.x() + rect.width());
		size.setHeight(rect.y() + rect.height());
		if(size.width() > 0 && size.height() > 0)
			setSize(size);
		else
			setSize(QSize(400, 600));
	}
}

SxeSession* WbWidget::session() {
	return session_;
}

WbWidget::Mode WbWidget::mode() {
	return mode_;
}

void WbWidget::setMode(Mode mode) {
	 mode_ = mode;

	 if(mode_ < DrawPath) {
			 if(newWbItem_) {
					 delete newWbItem_;
					 newWbItem_ = 0;
			 }
	 }

	 if(mode_ >= Erase) {
			 setDragMode(QGraphicsView::NoDrag);
			 setInteractive(false);
			 setCursor(Qt::CrossCursor);
	 } else {
			 setInteractive(true);
	 }

	 if(mode_ == Select) {
			 setDragMode(QGraphicsView::RubberBandDrag);
			 setCursor(Qt::ArrowCursor);
	 } else if(mode_ == Translate) {
			 setDragMode(QGraphicsView::RubberBandDrag);
			 setCursor(Qt::SizeAllCursor);
	 } else if(mode_ == Rotate) {
			setDragMode(QGraphicsView::RubberBandDrag);
			 unsetCursor();
			 // TODO: load cursor from image
	 } else if(mode_ == Scale) {
			setDragMode(QGraphicsView::RubberBandDrag);
			 setCursor(Qt::SizeBDiagCursor);
	 } else if(mode_ == Scroll) {
			 setDragMode(QGraphicsView::ScrollHandDrag);
	 }
}

void WbWidget::setSize(const QSize &size) {
	session_->setAttribute(session_->document().documentElement(), "viewBox", QString("0 0 %1 %2").arg(size.width()).arg(size.height()));
	session_->flush();
}

/*! \brief Generates a QRectF based on \a string provided in the SVG viewBox format. */
static QRectF parseSvgViewBox(QString string) {
	QString strings[4];
	qreal numbers[4];
	for(int i = 0, j = 0; i < 4; i++) {
		// skip spaces before number
		while(string[j].isSpace() && j < string.length())
			j++;

		while(!string[j].isSpace() && j < string.length()) {
			if(string[j].isNumber())
				strings[i] += string[j];
			j++;
		}

		numbers[i] = strings[i].toDouble();
	}

	// qDebug() << QString("QRectF(%1 %2 %3 %4)").arg(numbers[0]).arg(numbers[1]).arg(numbers[2]).arg(numbers[3]).toAscii();
	return QRect(numbers[0], numbers[1], numbers[2], numbers[3]);
}

void WbWidget::checkForViewBoxChange(const QDomNode &node) {
	if(node.isAttr() && node.nodeName().toLower() == "viewbox" && node.parentNode() == session_->document().documentElement()) {
		QRectF box = parseSvgViewBox(node.nodeValue());
		if(box.width() > 0 && box.height() > 0) {
			scene_->setSceneRect(box);
		}
	}
}

void WbWidget::setStrokeColor(const QColor &color) {
	strokeColor_ = color;
}

void WbWidget::setFillColor(const QColor &color) {
	fillColor_ = color;
}

void WbWidget::setStrokeWidth(int width) {
	strokeWidth_ = width;
}

void WbWidget::clear() {
	foreach(QGraphicsItem* graphicsitem, scene_->items()) {
		WbItem* wbitem = dynamic_cast<WbItem*>(graphicsitem);
		if(wbitem)
			session_->removeNode(wbitem->node());
	}
	session_->flush();
}

QSize WbWidget::sizeHint() const {
	if(scene_)
		return scene_->sceneRect().size().toSize();
	else
		return QSize();
}

void WbWidget::resizeEvent(QResizeEvent * event) {
	// Never show areas outside the sceneRect
	// Doesn't consider rotated views.
	QMatrix t;
	qreal sx = event->size().width() / scene_->sceneRect().width();
	qreal sy = event->size().height() / scene_->sceneRect().height();

	// Never shrink the view. Only enlarge if necessary.
	if(sx > 1 || sy > 1) {
		if(sx > sy)
			t.scale(sx, sx);
		else
			t.scale(sy, sy);
	}

	setMatrix(t);
	QGraphicsView::resizeEvent(event);
}

void WbWidget::mousePressEvent(QMouseEvent * event) {
	// ignore non-leftclicks when not in Select mode
	if(event->button() != Qt::LeftButton && mode_ != Select)
		return;

	// delete any temporary item being drawn
	if(newWbItem_) {
		delete newWbItem_;
		newWbItem_ = 0;
	}

	QPointF startPoint = mapToScene(mapFromGlobal(event->globalPos()));
	if(mode_ == DrawPath) {
		// // Create the element with starting position
		// QPointF sp = mapToScene(mapFromGlobal(event->globalPos()));
		// qDebug() << QString("1: (%1, %2)").arg(sp.x()).arg(sp.y());
		newWbItem_ = new WbNewPath(scene_, startPoint, strokeWidth_, strokeColor_, fillColor_);
		return;
	} else if(mode_ == DrawText) {

	} else if(mode_ == DrawRectangle) {

	} else if(mode_ == DrawEllipse) {

	} else if(mode_ == DrawCircle) {

	} else if(mode_ == DrawLine) {

	} else if(mode_ == DrawImage) {
		QString filename = QFileDialog::getOpenFileName(this, "Choose an image", QString(), "Images (*.png *.jpg)");
		if(!filename.isEmpty()) {
			newWbItem_ = new WbNewImage(scene_, startPoint, filename);
			session_->insertNodeAfter(newWbItem_->serializeToSvg(), session_->document().documentElement());
			session_->flush();
			delete newWbItem_;
			newWbItem_ = 0;
		}
	}

	QGraphicsView::mousePressEvent(event);
}

void WbWidget::mouseMoveEvent(QMouseEvent * event) {
	if(mode_ < Erase) {
		QGraphicsView::mouseMoveEvent(event);
		return;
	}

	if(QApplication::mouseButtons() != Qt::MouseButtons(Qt::LeftButton)) {
		if(newWbItem_) {
			 delete newWbItem_;
			 newWbItem_ = 0;
		}
		return;
	}

	if(mode_ == Erase) {
		 if(event->buttons() != Qt::MouseButtons(Qt::LeftButton))
			 return;
		 // Erase all items that appear in a 2*strokeWidth_ square with center at the event position
		 QPointF p = mapToScene(mapFromGlobal(event->globalPos()));
		 QGraphicsRectItem* eraseRect = scene_->addRect(QRectF(p.x() - strokeWidth_, p.y() - strokeWidth_, 2 * strokeWidth_, 2 * strokeWidth_));
		 foreach(QGraphicsItem * item, eraseRect->collidingItems()) {
			WbItem* wbitem = dynamic_cast<WbItem*>(item);
			if(wbitem)
				session_->removeNode(wbitem->node());
		 }
		 delete eraseRect;
		 eraseRect = 0;

		 event->ignore();
		 return;
	} else if(mode_ >= DrawPath && newWbItem_) {
		newWbItem_->parseCursorMove(mapToScene(mapFromGlobal(event->globalPos())));
	}
}

void WbWidget::mouseReleaseEvent(QMouseEvent * event) {

	if(event->button() != Qt::LeftButton && mode_ >= Erase)
		return;

	if (newWbItem_ && mode_ >= DrawPath && mode_ != DrawImage) {
		session_->insertNodeAfter(newWbItem_->serializeToSvg(), session_->document().documentElement());
		session_->flush();
		delete newWbItem_;
		newWbItem_ = 0;
		return;
	}

	QGraphicsView::mouseReleaseEvent(event);
}

WbItem* WbWidget::wbItem(const QDomNode &node) {
	foreach(WbItem* wbitem, items_) {
		if(wbitem->node() == node)
			return wbitem;
	}
	return 0;
}

void WbWidget::handleDocumentUpdated(bool remote) {
	Q_UNUSED(remote);
	inspectNodes();
	rerender();
}

void WbWidget::inspectNodes() {
	while(!recentlyRelocatedNodes_.isEmpty()) {
		QDomNode node = recentlyRelocatedNodes_.takeFirst();

		if(!node.isElement())
			continue;

		// check if we already have a WbItem for the node
		WbItem* item = wbItem(node);

		// We don't need to do anything iff node is child of <svg/> and item exists or vice versa
		if((item != NULL) == (node.parentNode() == session_->document().documentElement()))
			continue;

		// Otherwise, either item exists and needs to be removed
		//			  or it doesn't exist and needs to be added
		if(item)
			removeWbItem(item);
		else
			items_.append(new WbItem(session_, &renderer_, node.toElement(), scene_, this));
	}
}

void WbWidget::queueNodeInspection(const QDomNode &node) {
	if(node.isElement())
		recentlyRelocatedNodes_.append(node);
}

void WbWidget::removeWbItem(const QDomNode &node) {
	removeWbItem(wbItem(node));
}

void WbWidget::removeWbItem(WbItem *wbitem) {
	if(wbitem) {
		// Remove from the lookup table to avoid infinite loop of deletes
		items_.removeAll(wbitem);
		// items_.takeAt(items_.indexOf(wbitem));

		idlessItems_.removeAll(wbitem);

		delete wbitem;
	}
}

void WbWidget::addIds() {
	while(!idlessItems_.isEmpty())
		idlessItems_.takeFirst()->addToScene();

	session_->flush();
}

void WbWidget::addToIdLess(const QDomElement &element) {
	if(element.parentNode() != session_->document().documentElement())
		return;

	WbItem* item = wbItem(element);
	if(item && !idlessItems_.contains(item)) {
		// Remove from the scene until a new 'id' attribute is added
		item->removeFromScene();

		idlessItems_.append(item);

		// Try adding the 'id' attribute after a random delay of 0 to 2s
		QTimer::singleShot(2000 * qrand() / RAND_MAX, this, SLOT(addIds()));
	}
}

void WbWidget::checkForRemovalOfId(QDomNode node) {
	if(node.isAttr()) {
		if(node.nodeName() == "id") {
			while(!(node.isElement() || node.isNull()))
				node = node.parentNode();

			if(!node.isNull())
				addToIdLess(node.toElement());
		}
	}
}

void WbWidget::rerender() {
	QString xmldump;
	QTextStream stream(&xmldump);
	session_->document().save(stream, 1);

	// qDebug("Document in WbWidget:");
	// qDebug() << xmldump.toAscii();

	renderer_.load(xmldump.toAscii());

	// Update all positions if changed
	foreach(WbItem* wbitem, items_) {
		// resetting elementId is necessary for rendering some updates to the element (e.g. adding child elements to <g/>)
		wbitem->setElementId(wbitem->id());

		// qDebug() << QString("Rerendering %1").arg((unsigned int) wbitem).toAscii();
		wbitem->resetPos();
	}
}
