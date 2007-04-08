/*
 * wbwidget.cpp - a widget for processing and showing whiteboard
 *                messages.
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
#include <QMouseEvent>
#include <QApplication>

/*
 *	WbWidget
 */

WbWidget::WbWidget(const QString &session, const QString &ownJid, const QSize &s, QWidget *parent) : QGraphicsView(parent) {
	newWbItem_ = 0;
	adding_ = 0;
	addVertex_ = false;
	controlPoint_ = 0;
	strokeColor_ = Qt::black;
	fillColor_ = Qt::transparent;
	strokeWidth_ = 1;

//	setCacheMode(CacheBackground);
	setRenderHint(QPainter::Antialiasing);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	scene = new WbScene(session, ownJid, this);
	scene->setItemIndexMethod(QGraphicsScene::NoIndex);
	scene->setSceneRect(0, 0, s.width(), s.height());
	setScene(scene);
	connect(scene, SIGNAL(newWb(const QDomElement&)), SIGNAL(newWb(const QDomElement&)));

	setMode(Select);
}

bool WbWidget::processWb(const QDomElement &wb) {
	return scene->processWb(wb);
}

QString WbWidget::session() {
	return scene->session();
}

const QString & WbWidget::ownJid() const {
	return scene->ownJid();
}

WbWidget::Mode WbWidget::mode() {
	return mode_;
}

void WbWidget::setMode(Mode mode) {
	mode_ = mode;
	if(mode_ == DrawImage) {
		QString filename = QFileDialog::getOpenFileName(this, "Choose an image", QString(), "Images (*.png *.jpg)");
		QFile file(filename);
		if (file.open(QIODevice::ReadOnly)) {
			// TODO: Should we perhaps scale large images?
			QByteArray data = file.readAll();
			QImage image(data);
			if(!image.isNull()) {
				// Create the element
// 				QDomDocument d = QDomDocument();
				QDomElement _svg = QDomDocument().createElement("image");
				_svg.setAttribute("x", sceneRect().x());
				_svg.setAttribute("y", sceneRect().y());
				_svg.setAttribute("xmlns:xlink", "http://www.w3.org/1999/xlink");
				_svg.setAttribute("xlink:href", QString("data:image/%1;base64,%2").arg(filename.mid(filename.lastIndexOf(".") + 1).toLower()).arg(QCA::Base64().arrayToString(data)));
				// Create the object
				newWbItem_ = new WbImage(_svg, scene->newId(), scene->newIndex(), "root", scene);
				scene->addWbItem(newWbItem_);
				newWbItem_->graphicsItem()->ensureVisible(QRectF(), 0, 0);
				// Send out the new item in pieces if large
				_svg = newWbItem_->svg();
				int chunksize = 51200;
				if(_svg.attribute("xlink:href").size() <= chunksize)
					scene->queueNew(newWbItem_->id(), newWbItem_->index(), _svg);
				else {
					QString datastring = _svg.attribute("xlink:href");
					// Send the first chunk with the new element
					_svg.setAttribute("xlink:href", datastring.left(chunksize));
					scene->queueNew(newWbItem_->id(), newWbItem_->index(), _svg);
					// Send the the rest of the data chunks in attribute edits
					int processed = chunksize;
					while(processed < datastring.size()) {
						scene->queueAttributeEdit(newWbItem_->id(), "xlink:href", datastring.mid(processed, chunksize), _svg.attribute("xlink:href"), processed, processed);
						_svg.setAttribute("xlink:href", datastring.left(processed));
						processed += chunksize;
					}
				}
				newWbItem_ = 0;
				return;
			}
		}
		mode_ = Select;
	}

	if(mode_ < DrawPath) {
		if(newWbItem_) {
			if(newWbItem_->graphicsItem() && newWbItem_->graphicsItem()->scene())
				newWbItem_->graphicsItem()->scene()->removeItem(newWbItem_->graphicsItem());
			newWbItem_->deleteLater();
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
		setDragMode(QGraphicsView::NoDrag);
		unsetCursor();
		// TODO: load cursor from image
	} else if(mode_ == Scale) {
		setDragMode(QGraphicsView::NoDrag);
		setCursor(Qt::SizeBDiagCursor);
	} else if(mode_ == Scroll) {
		setDragMode(QGraphicsView::ScrollHandDrag);
	}
}

void WbWidget::setSize(const QSize &s) {
	scene->setSceneRect(0, 0, s.width(), s.height());
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

void WbWidget::clear(const bool &sendEdits) {
	// Remove all elements but root
	foreach(QGraphicsItem* graphicsitem, scene->items()) {
		WbItem* wbitem = scene->findWbItem(graphicsitem);
		if(wbitem) {
			if(wbitem->id() != "root")
				scene->removeElement(wbitem->id(), sendEdits);
		} else
			delete graphicsitem;
	}
}

void WbWidget::setImporting(bool i) {
	scene->importing = i;
}

QSize WbWidget::sizeHint() const {
	if(scene)
		return scene->sceneRect().size().toSize();
	else
		return QSize();
}

void WbWidget::resizeEvent(QResizeEvent * event) {
	// Never show areas outside the sceneRect
	// Doesn't consider rotated views.
	QMatrix t;
	qreal sx = event->size().width() / scene->sceneRect().width();
	qreal sy = event->size().height() / scene->sceneRect().height();
	// Skip through the boundary region where the scroll bars change to avoid a nasty loop
	if((sx-sy)/sx < 0.03 && (sy-sx)/sx < 0.03)
		sx *= 1.05;
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
	if(event->button() != Qt::LeftButton && mode_ != Select)
		return;

	if(newWbItem_) {
		if(newWbItem_->graphicsItem() && newWbItem_->graphicsItem()->scene())
			newWbItem_->graphicsItem()->scene()->removeItem(newWbItem_->graphicsItem());
		newWbItem_->deleteLater();
		newWbItem_ = 0;
	}

	QMouseEvent passEvent(QEvent::MouseButtonDblClick, QPoint(), Qt::NoButton, Qt::MouseButtons(Qt::NoButton), Qt::KeyboardModifiers(Qt::NoModifier));
	if(mode_ == Select) {
		QGraphicsView::mousePressEvent(event);
		return;
	} else if(mode_ == Translate)
		//Ctrl: translate
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier));
	else if(mode_ == Rotate)
		//Ctrl + Alt: rotate
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier + Qt::AltModifier));
	else if(mode_ == Scale)
		//Ctrl + Shift: scale
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier + Qt::ShiftModifier));
	else if(mode_ == Scroll)
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::NoModifier));
	else if(mode_ == Erase) {
		return;
	} else if(mode_ == DrawPath) {
		// Create the element with starting position
		QPointF startPoint = mapToScene(mapFromGlobal(event->globalPos()));
		QDomDocument d;
		QDomElement _svg = d.createElement("path");
		_svg.setAttribute("d", QString("M %1 %2").arg(startPoint.x()).arg(startPoint.y()));
		newWbItem_ = new WbPath(_svg, scene->newId(), scene->newIndex(), "root", scene);
		newWbItem_->setStrokeColor(strokeColor_);
		newWbItem_->setFillColor(fillColor_);
		newWbItem_->setStrokeWidth(strokeWidth_);
		if(controlPoint_) {
			delete controlPoint_;
			controlPoint_ = 0;
		}
		// Force adding a vertex/control point every 30ms even for small changes.
		addVertex_ = false;
		adding_ = new QTimer(this);
		connect(adding_, SIGNAL(timeout()), SLOT(addVertex()));
		adding_->start(30);
		return;
	} else if(mode_ == DrawText) {
		QPointF startPoint = mapToScene(mapFromGlobal(event->globalPos()));
		QDomDocument d;
		QDomElement _svg = d.createElement("text");
		_svg.appendChild(d.createTextNode("text"));
		_svg.setAttribute("x", startPoint.x());
		_svg.setAttribute("y", startPoint.y());
		// Create the element
		WbText* newText = new WbText(_svg, scene->newId(), scene->newIndex(), "root", scene);
		newText->setStrokeColor(strokeColor_);
		newWbItem_->setFillColor(fillColor_);
		newText->setStrokeWidth(strokeWidth_);
		// Send out the new item
		scene->queueNew(newText->id(), newText->index(), newText->svg());
		// Make it a "normal" item by adding it to the scene properly and by removing the pointer to it
		scene->addWbItem(newText);
		return;
	} else if(mode_ == DrawRectangle) {
		QPointF startPoint = mapToScene(mapFromGlobal(event->globalPos()));
		QDomDocument d;
		QDomElement _svg = d.createElement("rect");
		_svg.setAttribute("x", startPoint.x());
		_svg.setAttribute("y", startPoint.y());
		_svg.setAttribute("height", 2);
		_svg.setAttribute("width", 2);
		// Create the element
		newWbItem_ = new WbRectangle(_svg, scene->newId(), scene->newIndex(), "root", scene);
		newWbItem_->setStrokeColor(strokeColor_);
		newWbItem_->setFillColor(fillColor_);
		newWbItem_->setStrokeWidth(strokeWidth_);
		return;
	} else if(mode_ == DrawEllipse) {
		QPointF startPoint = mapToScene(mapFromGlobal(event->globalPos()));
		QDomDocument d;
		QDomElement _svg = d.createElement("ellipse");
		_svg.setAttribute("cx", startPoint.x());
		_svg.setAttribute("cy", startPoint.y());
		_svg.setAttribute("rx", 2);
		_svg.setAttribute("ry", 2);
		// Create the empty text element
		newWbItem_ = new WbEllipse(_svg, scene->newId(), scene->newIndex(), "root", scene);
		newWbItem_->setStrokeColor(strokeColor_);
		newWbItem_->setFillColor(fillColor_);
		newWbItem_->setStrokeWidth(strokeWidth_);
		return;
	} else if(mode_ == DrawCircle) {
		QPointF startPoint = mapToScene(mapFromGlobal(event->globalPos()));
		QDomDocument d;
		QDomElement _svg = d.createElement("circle");
		_svg.setAttribute("cx", startPoint.x());
		_svg.setAttribute("cy", startPoint.y());
		_svg.setAttribute("r", 2);
		// Create the empty text element
		newWbItem_ = new WbCircle(_svg, scene->newId(), scene->newIndex(), "root", scene);
		newWbItem_->setStrokeColor(strokeColor_);
		newWbItem_->setFillColor(fillColor_);
		newWbItem_->setStrokeWidth(strokeWidth_);
		return;
	} else if(mode_ == DrawLine) {
		QPointF startPoint = mapToScene(mapFromGlobal(event->globalPos()));
		QDomDocument d;
		QDomElement _svg = d.createElement("line");
		_svg.setAttribute("x1", startPoint.x());
		_svg.setAttribute("y1", startPoint.y());
		_svg.setAttribute("x2", startPoint.x() + 2);
		_svg.setAttribute("y2", startPoint.y() + 2);
		// Create the empty text element
		newWbItem_ = new WbLine(_svg, scene->newId(), scene->newIndex(), "root", scene);
		newWbItem_->setStrokeColor(strokeColor_);
		newWbItem_->setFillColor(fillColor_);
		newWbItem_->setStrokeWidth(strokeWidth_);
		return;
	}

	if(passEvent.type() == event->type()) {
		QGraphicsView::mousePressEvent(&passEvent);
	}
}

void WbWidget::mouseMoveEvent(QMouseEvent * event) {
	if(QApplication::mouseButtons() != Qt::MouseButtons(Qt::LeftButton)) {
		if(newWbItem_) {
			if(newWbItem_->graphicsItem() && newWbItem_->graphicsItem()->scene())
				newWbItem_->graphicsItem()->scene()->removeItem(newWbItem_->graphicsItem());
			newWbItem_->deleteLater();
			newWbItem_ = 0;
		}
		return;
	}

	QMouseEvent passEvent(QEvent::MouseButtonDblClick, QPoint(), Qt::NoButton, Qt::MouseButtons(Qt::NoButton), Qt::KeyboardModifiers(Qt::NoModifier));
	if(mode_ == Select) {
		QGraphicsView::mouseMoveEvent(event);
		return;
	} else if(mode_ == Translate)
		//Ctrl: translate
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier));
	else if(mode_ == Rotate)
		//Ctrl + Alt: rotate
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier + Qt::AltModifier));
	else if(mode_ == Scale)
		//Ctrl + Shift: scale
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier + Qt::ShiftModifier));
	else if(mode_ == Scroll)
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::NoModifier));
	else if(mode_ == Erase) {
		if(event->buttons() != Qt::MouseButtons(Qt::LeftButton))
			return;
		// Erase all items that appear in a 2*strokeWidth_ square with center at the event position
		QPointF p = mapToScene(mapFromGlobal(event->globalPos()));
		QGraphicsRectItem* eraseRect = scene->addRect(QRectF(p.x() - strokeWidth_, p.y() - strokeWidth_, 2 * strokeWidth_, 2 * strokeWidth_));
		foreach(QGraphicsItem * item, eraseRect->collidingItems()) {
			// Make sure that the colliding item is WbItem, if for some reason not, delete anyway
			WbItem* wbitem = scene->findWbItem(item);
			if(wbitem)
				scene->removeElement(wbitem->id(), true);
			else
				delete item;
		}
		delete eraseRect;
		eraseRect = 0;
		return;
	} else if(mode_ == DrawPath && newWbItem_) {
		WbPath* newWbPath = qobject_cast<WbPath*>(newWbItem_);
		if(!newWbPath)
			return;
		QPointF newPoint = QPointF(mapToScene(mapFromGlobal(event->globalPos())));
		QPointF difference;
		if(controlPoint_)
			difference = newPoint - *controlPoint_;
		else
			difference = newPoint - newWbPath->path().currentPosition();
		// Try to reduce the amount of points
		if(difference.isNull()) {
			if(addVertex_ && controlPoint_) {
				newWbPath->lineTo(*controlPoint_);
				adding_->start();
				addVertex_ = false;
				delete controlPoint_;
				controlPoint_ = 0;
			}
			return;
		} else if(!addVertex_ && -50 < difference.x() + difference.y() && difference.x() + difference.y() < 50) {
// 			qDebug() << QString("difference x + y: %1").arg(difference.x() + difference.y());
			return;
		}
		if(controlPoint_) {
			newWbPath->quadTo(*controlPoint_, newPoint);
			delete controlPoint_;
			controlPoint_ = 0;
		} else {
			// TODO: Recalculate a better position for the controlPoint_
			controlPoint_ = new QPointF(newPoint);
		}
		adding_->start();
		addVertex_ = false;
		// I think Qt should take care of this. However, at least in TP1 this is necessary for "small changes"
		scene->update(newWbPath->boundingRect());
		return;
	} else if (mode_ == DrawRectangle && qobject_cast<WbRectangle*>(newWbItem_)) {
		QPointF newPoint = QPointF(mapToScene(mapFromGlobal(event->globalPos())));
		QDomElement _svg = newWbItem_->svg();
		QPointF difference = QPointF(newPoint.x() - _svg.attribute("x").toDouble(), newPoint.y() - _svg.attribute("y").toDouble());
		if(difference.x() < 0)
			difference.setX(-difference.x());
		if(difference.y() < 0)
			difference.setY(-difference.y());
		_svg.setAttribute("width", difference.x());
		_svg.setAttribute("height", difference.y());
		newWbItem_->parseSvg(_svg);
		return;
	} else if (mode_ == DrawEllipse && qobject_cast<WbEllipse*>(newWbItem_)) {
		QPointF newPoint = QPointF(mapToScene(mapFromGlobal(event->globalPos())));
		QDomElement _svg = newWbItem_->svg();
		QPointF difference = QPointF(newPoint.x() - _svg.attribute("cx").toDouble(), newPoint.y() - _svg.attribute("cy").toDouble());
		if(difference.x() < 0)
			difference.setX(-difference.x());
		if(difference.y() < 0)
			difference.setY(-difference.y());
		_svg.setAttribute("rx", difference.x());
		_svg.setAttribute("ry", difference.y());
		newWbItem_->parseSvg(_svg);
		return;
	} else if (mode_ == DrawCircle && qobject_cast<WbCircle*>(newWbItem_)) {
		QPointF newPoint = QPointF(mapToScene(mapFromGlobal(event->globalPos())));
		QDomElement _svg = newWbItem_->svg();
		QPointF difference = QPointF(newPoint.x() - _svg.attribute("cx").toDouble(), newPoint.y() - _svg.attribute("cy").toDouble());
		if(difference.x() < 0)
			difference.setX(-difference.x());
		if(difference.y() < 0)
			difference.setY(-difference.y());
		_svg.setAttribute("r", std::pow(difference.x()*difference.x() + difference.y()*difference.y(), 0.5));
		newWbItem_->parseSvg(_svg);
		return;
	} else if (mode_ == DrawLine && qobject_cast<WbLine*>(newWbItem_)) {
		QPointF newPoint = QPointF(mapToScene(mapFromGlobal(event->globalPos())));
		QDomElement _svg = newWbItem_->svg();
		_svg.setAttribute("x2", newPoint.x());
		_svg.setAttribute("y2", newPoint.y());
		newWbItem_->parseSvg(_svg);
		return;
	}

	if(passEvent.type() == event->type()) {
		QGraphicsView::mouseMoveEvent(&passEvent);
	}
}

void WbWidget::mouseReleaseEvent(QMouseEvent * event) {
	if(event->button() != Qt::LeftButton && mode_ != Select)
		return;

	QMouseEvent passEvent(QEvent::MouseButtonDblClick, QPoint(), Qt::NoButton, Qt::MouseButtons(Qt::NoButton), Qt::KeyboardModifiers(Qt::NoModifier));
	if(mode_ == Select || event->button() != Qt::LeftButton) {
		QGraphicsView::mouseReleaseEvent(event);
		return;
	} else if (mode_ == Translate)
		//Ctrl: translate
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier));
	else if (mode_ == Rotate)
		//Ctrl + Alt: rotate
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier + Qt::AltModifier));
	else if (mode_ == Scale)
		//Ctrl + Shift: scale
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::ControlModifier + Qt::ShiftModifier));
	else if (mode_ == Scroll)
		passEvent = QMouseEvent(event->type(), event->pos(), event->globalPos(), event->button(), event->buttons(), Qt::KeyboardModifiers(Qt::NoModifier));
	else if(mode_ == DrawPath) {
		if(adding_) {
			delete adding_;
			adding_ = 0;
		}
		if (newWbItem_) {	
			WbPath* newWbPath = qobject_cast<WbPath*>(newWbItem_);
			if(!newWbPath)
				return;
			// Finish the path
			QPointF endPoint = QPointF(mapToScene(mapFromGlobal(event->globalPos())));
			if(newWbPath->path().currentPosition() != endPoint) {
				if(controlPoint_) {
					newWbPath->quadTo(*controlPoint_, endPoint);
					delete controlPoint_;
					controlPoint_ = 0;
				} else {
					newWbPath->lineTo(endPoint);
				}
			}
			scene->update(newWbPath->boundingRect());
			// Send out the new item
			scene->queueNew(newWbItem_->id(), newWbItem_->index(), newWbItem_->svg());
			// Make it a "normal" item by adding it to the scene properly and by removing the pointer to it
			scene->addWbItem(newWbItem_);
			newWbItem_ = 0;
		}
		return;
	} else if (newWbItem_ && (mode_ == DrawRectangle || mode_ == DrawEllipse || mode_ == DrawCircle || mode_ == DrawLine)) {
		// Send out the new item
		scene->queueNew(newWbItem_->id(), newWbItem_->index(), newWbItem_->svg());
		// Make it a "normal" item by adding it to the scene properly and by removing the pointer to it
		scene->addWbItem(newWbItem_);
		newWbItem_ = 0;
		return;
	}

	if(passEvent.type() == event->type()) {
		QGraphicsView::mouseReleaseEvent(&passEvent);
	}
}

void WbWidget::addVertex() {
	// force adding a vertex
	addVertex_ = true;
	QMouseEvent* passEvent = new QMouseEvent(QEvent::MouseMove, mapFromGlobal(QCursor::pos()), Qt::NoButton, Qt::MouseButtons(Qt::LeftButton), Qt::KeyboardModifiers(Qt::NoModifier));
	mouseMoveEvent(passEvent);
	delete passEvent;
}
