/*
 * wbitems.cpp - the item classes for the SVG WB
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

#include "wbitems.h"
#include "wbscene.h"
#include <QDebug>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 *	Edit
 */

Edit::Edit(Type _type, const QDomElement &_xml) {
	type = _type;
	xml = _xml.cloneNode().toElement();
}

Edit::Edit(Type _type, const QString &_target, const QDomElement &edit, const QString &_oldValue) {
	type = _type;
	target = _target;
	xml = edit.cloneNode().toElement();
	if(_type == AttributeEdit)
		oldValue = _oldValue;
	else if(_type == AttributeEdit)
		oldParent = _oldValue;
}

Edit::Edit(const QString &_target, const QDomElement &edit, QDomNodeList _oldContent) {
	type = Edit::ContentEdit;
	target = _target;
	xml = edit.cloneNode().toElement();
	oldContent = _oldContent;
}

/*
 *	EditUndo
 */

EditUndo::EditUndo(const int &_version, const Edit &edit) {
	type = edit.type;
	version = _version;
	if(type == Edit::AttributeEdit) {
		attribute = edit.xml.attribute("name");
		oldValue = edit.oldValue;
	} else if(type == Edit::ParentEdit) {
		oldParent = edit.oldParent;
	} else if(type == Edit::ContentEdit) {
		oldContent = edit.oldContent;
	}
}

EditUndo::EditUndo(const int &_version, const QString &_attribute, const QString &_oldValue) {
	type = Edit::AttributeEdit;
	version = _version;
	attribute = _attribute;
	oldValue = _oldValue;
}

EditUndo::EditUndo(const int &_version, const QString &_oldParent) {
	type = Edit::ParentEdit;
	version = _version;
	oldParent = _oldParent;
}

EditUndo::EditUndo(const int &_version, QDomNodeList _oldContent) {
	type = Edit::ContentEdit;
	version = _version;
	oldContent = _oldContent;
}

/*
 *	WbItemMenu
 */

WbItemMenu::WbItemMenu(QWidget* parent) : QMenu(parent) {
	connect(this, SIGNAL(aboutToHide()), SLOT(destructSelf()));
}

WbItemMenu::~WbItemMenu() {
	foreach(QActionGroup* g, groups_) {
		foreach(QAction* a, g->actions()) {
			a->deleteLater();
		}
		g->deleteLater();
	}
}

void WbItemMenu::addActionGroup(QActionGroup* g) {
	groups_.append(g);
	addActions(g->actions());
}

void WbItemMenu::destructSelf() {
	deleteLater();
}


/*
 *	WbItem
 */

WbItem::WbItem(const QString &id) {
	id_ = id;
	parent_ = "root";
	version = 0;
}

QString WbItem::parentWbItem() {
		return parent_;
}

void WbItem::setParentWbItem(const QString &newParent, bool emitChanges) {
	if(newParent != parent_) {
		if(newParent.isEmpty()) {
			if(parent_ != "root") {
				graphicsItem()->setParentItem(0);
				if(emitChanges)
					emit parentChanged(id(), "root", parent_);
				parent_ = "root";
			}
		} else {
			if(graphicsItem()) {
				WbScene* wbscene = qobject_cast<WbScene*>(graphicsItem()->scene());
				if(wbscene) {
					WbItem* p = wbscene->findWbItem(newParent);
					if(p && p->graphicsItem())
						graphicsItem()->setParentItem(p->graphicsItem());
				}
			}
			if(emitChanges)
				emit parentChanged(id(), newParent, parent_);
			parent_ = newParent;
		}
	}
}

QString WbItem::id() {
	return id_;
}

qreal WbItem::index() {
	return index_;
}

void WbItem::setIndex(qreal index, bool emitChanges) {
	if(index_ != index) {
		if(emitChanges)
			emit indexChanged(id(), index - index_);
		index_ = index;
		if(graphicsItem()) {
			QString z;
			if(index_ - static_cast<int>(index_))
				z = QString("%1").arg(index_);
			else
				z = QString("%1.").arg(index_);
			int c;
			for(int i=0; i < id_.length(); i++) {
				c = id_.at(i).unicode();
				//TODO: what should be the minumum width? at least 3 but can a jid contain characters that have a value beyond 999
				z.append(QString("%1").arg(QString("%1").arg(c), 4, QLatin1Char('0')));
			}
	// 		qDebug() << "z: " << z;
			graphicsItem()->setZValue(z.toDouble());
		}
	}
}

WbItemMenu* WbItem::constructContextMenu() {
	WbItemMenu* menu = new WbItemMenu(0);
	WbScene* wbscene = qobject_cast<WbScene*>(graphicsItem()->scene());
	if(wbscene) {
		// Add the default actions
		QActionGroup* group = new QActionGroup(this);
		QPixmap pixmap(2, 2);
		pixmap.fill(QColor(Qt::black));
		QAction* qaction = new QAction(QIcon(pixmap), tr("Thin stroke"), group);
		qaction->setData(QVariant(1));
		pixmap = QPixmap(6, 6);
		pixmap.fill(QColor(Qt::black));
		qaction = new QAction(QIcon(pixmap), tr("Medium stroke"), group);
		qaction->setData(QVariant(3));
		pixmap = QPixmap(12, 12);
		pixmap.fill(QColor(Qt::black));
		qaction = new QAction(QIcon(pixmap), tr("Thick stroke"), group);
		qaction->setData(QVariant(6));
		connect(group, SIGNAL(triggered(QAction*)), wbscene, SLOT(setStrokeWidth(QAction*)));
		menu->addActionGroup(group);

		menu->addSeparator();
		group = new QActionGroup(this);
		pixmap = QPixmap(16, 16);
		pixmap.fill(QColor(Qt::darkCyan));
		qaction = new QAction(QIcon(pixmap), tr("Change stroke color"), group);
		connect(qaction, SIGNAL(triggered()), wbscene, SLOT(setStrokeColor()));
		pixmap.fill(QColor(Qt::darkYellow));
		qaction = new QAction(QIcon(pixmap), tr("Change fill color"), group);
		connect(qaction, SIGNAL(triggered()), wbscene, SLOT(setFillColor()));
		menu->addActionGroup(group);

		menu->addSeparator();
		group = new QActionGroup(this);
		IconAction* action = new IconAction(tr("Bring forward"), "psi/bringForwards", tr("Bring forward"), 0, group);
		connect(action, SIGNAL(triggered()), wbscene, SLOT(bringForward()));
		action = new IconAction(tr("Bring to front"), "psi/bringToFront", tr("Bring to front"), 0, group);
		connect(action, SIGNAL(triggered()), wbscene, SLOT(bringToFront()));
		action = new IconAction(tr("Send backwards"), "psi/sendBackwards", tr("Send backwards"), 0, group);
		connect(action, SIGNAL(triggered()), wbscene, SLOT(sendBackwards()));
		action = new IconAction(tr("Send to back"), "psi/sendToBack", tr("Send to back"), 0, group);
		connect(action, SIGNAL(triggered()), wbscene, SLOT(sendToBack()));
		menu->addActionGroup(group);

		menu->addSeparator();
		group = new QActionGroup(this);
		action = new IconAction(tr("Group"), "psi/group", tr("Group"), 0, group);
		connect(action, SIGNAL(triggered()), wbscene, SLOT(group()));
		action = new IconAction(tr("Ungroup"), "psi/ungroup", tr("Ungroup"), 0, group);
		connect(action, SIGNAL(triggered()), wbscene, SLOT(ungroup()));
		menu->addActionGroup(group);
	}
	return menu;
}

void WbItem::setStrokeColor(QColor color) {
	if(graphicsItem()) {
		QDomElement _svg = svg();
		_svg.setAttribute("stroke", QString("rgb(%1,%2,%3)").arg(color.red()).arg(color.green()).arg(color.blue()));
		if(color.alphaF() != 1 || _svg.hasAttribute("stroke-opacity"))
			_svg.setAttribute("stroke-opacity", QString("%1").arg(color.alphaF()));
		parseSvg(_svg, true);
	}
}

void WbItem::setFillColor(QColor color) {
	if(graphicsItem()) {
		QDomElement _svg = svg();
		if(color == Qt::transparent) {
			_svg.removeAttribute("fill");
			_svg.removeAttribute("fill-opacity");
		} else {
			_svg.setAttribute("fill", QString("rgb(%1,%2,%3)").arg(color.red()).arg(color.green()).arg(color.blue()));
			if(color.alphaF() != 1 || _svg.hasAttribute("fill-opacity"))
				_svg.setAttribute("fill-opacity", QString("%1").arg(color.alphaF()));
		}
		parseSvg(_svg, true);
	}
}

void WbItem::setStrokeWidth(int width) {
	if(graphicsItem()) {
		QDomElement _svg = svg();
		_svg.setAttribute("stroke-width", QString("%1").arg(width));
		parseSvg(_svg, true);
	}
}

QDomElement WbItem::svg() {
	QDomDocument doc;
	QDomElement _svg = doc.createElement("item");
	foreach(QString a, attributes.keys()) {
		if(!attributes[a].isNull())
			_svg.setAttribute(a, attributes[a]);
		else
			attributes.remove(a);
	}
	switch(type()) {
		case 87654000:
			_svg.setTagName("root");
			break;
		case 87654001:
			_svg.setTagName("path");
			break;
		case 87654002:
			_svg.setTagName("ellipse");
			break;
		case 87654003:
			_svg.setTagName("circle");
			break;
		case 87654004:
			_svg.setTagName("rect");
			break;
		case 87654005:
			_svg.setTagName("line");
			break;
		case 87654006:
			_svg.setTagName("polyline");
			break;
		case 87654007:
			_svg.setTagName("polygon");
			break;
		case 87654101:
			_svg.setTagName("text");
			break;
		case 87654102:
			_svg.setTagName("image");
			break;
		case 87654201:
			_svg.setTagName("g");
			break;
	}
	return _svg;
};

QList<QString> WbItem::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed;
	// First process changes to existing elements
	foreach(QString a, attributes.keys()) {
		if(attributes[a] != _svg.attribute(a)) {
			if(a.startsWith("stroke") || a == "opacity")
				changed.append("pen");
			else
				changed.append(a);
		}
		if(_svg.hasAttribute(a)) {
			if(attributes[a] != _svg.attribute(a) && emitChanges)
				emit attributeChanged(id(), a, _svg.attribute(a), attributes[a]);
			attributes[a] = _svg.attribute(a);
			_svg.removeAttribute(a);
		} else {
			if(emitChanges)
				emit attributeChanged(id(), a, QString(), attributes[a]);
			attributes.remove(a);
		}
	}
	QDomNode a;
	for(uint i = 0; i < _svg.attributes().length(); i++) {
		a = _svg.attributes().item(i).cloneNode();
		if(emitChanges)
			emit attributeChanged(id(), a.nodeName(), a.nodeValue(), QString());
		attributes[a.nodeName()] = a.nodeValue();
		if(a.nodeName().startsWith("stroke") || a.nodeName() == "opacity")
			changed.append("pen");
		else
			changed.append(a.nodeName());
	}

	if(graphicsItem()) {
		// 'x' & 'y' & 'cx' & 'cy'
		if(changed.contains("x") || changed.contains("y") || changed.contains("cx") || changed.contains("cy")) {
			bool xOk, yOk;
			qreal xPos, yPos;
			if(changed.contains("x") || changed.contains("y")) {
				xPos = attributes["x"].toDouble(&xOk);
				yPos = attributes["y"].toDouble(&yOk);
			} else {
				xPos = attributes["cx"].toDouble(&xOk);
				yPos = attributes["cy"].toDouble(&yOk);
			}
			if(xOk && yOk)
				graphicsItem()->setPos(QPointF(xPos, yPos));
// 				graphicsItem()->setPos(graphicsItem()->mapToParent(graphicsItem()->mapFromScene(QPointF(xPos, yPos))));
		}

		// 'fill'
		if(changed.contains("fill")) {
			QBrush brush;
			if(!attributes["fill"].isEmpty())
				brush = QBrush(constructColor(attributes["fill"], attributes["fill-opacity"]));
			QAbstractGraphicsShapeItem* s = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(graphicsItem());
			if(s) {
				// Only set pens for items that implement it
				s->setBrush(brush);
			}
		}

		// 'stroke' & stroke-dasharray & stroke-dashoffset & stroke-dashoffset & stroke-linecap & stroke-linejoin & stroke-miterlimit & stroke-opacity &
		if(changed.contains("pen")) {
			QPen pen;
			pen = parseQPen(attributes["stroke"],
					attributes["stroke-dasharray"],
					attributes["stroke-dashoffset"],
					attributes["stroke-linecap"],
					attributes["stroke-linejoin"],
					attributes["stroke-miterlimit"],
					attributes["stroke-opacity"],
					attributes["opacity"],
					attributes["stroke-width"]);
			QAbstractGraphicsShapeItem* s = qgraphicsitem_cast<QAbstractGraphicsShapeItem*>(graphicsItem());
			if(s) {
				// Only set pens for items that implement it
				s->setPen(pen);
			}/* else if(typerange == 101) {
				QGraphicsTextItem* s = dynamic_cast<QGraphicsTextItem*>(graphicsItem());
				s->setPen(pen);
			}*/
		}

		if(changed.contains("transform"))
			graphicsItem()->setMatrix(parseTransformationMatrix(attributes["transform"]));

		graphicsItem()->update();
	}
	return changed;
}

void WbItem::regenerateTransform() {
	if(!graphicsItem())
		return;
	QMatrix m = graphicsItem()->matrix();
	QString old = attributes["transform"];
	if(m.isIdentity())
		attributes["transform"] = QString();
	else
		attributes["transform"] = QString("matrix(%1 %2 %3 %4 %5 %6)").arg(m.m11()).arg(m.m12()).arg(m.m21()).arg(m.m22()).arg(m.dx()).arg(m.dy());
	if(attributes["transform"] != old)
		emit attributeChanged(id(), "transform", attributes["transform"], old);
}

QPointF WbItem::center() {
	if(graphicsItem()) {
		// Determine the center of the item in item coordinates before transformation
		QRectF r = graphicsItem()->boundingRect();
		QPointF c(r.x() + r.width()/2, r.y() + r.height()/2);
// 		qDebug() << QString("center: %1 + %2    %3 + %4").arg(r.x()).arg(r.width()/2).arg(r.y()).arg(r.height()/2);
		// return the center with transformation applied
		return graphicsItem()->matrix().map(c);
	}
	return QPointF();
}

void WbItem::handleMouseMoveEvent(QGraphicsSceneMouseEvent * event) {
	event->accept();
	if(graphicsItem() && graphicsItem()->isSelected()) {
		if(event->modifiers() == Qt::KeyboardModifiers(Qt::ControlModifier)) {
			// Ctrl: Translate
			// Translate each selected item
			WbScene* wbscene = qobject_cast<WbScene*>(graphicsItem()->scene());
			if(wbscene) {
				foreach(QGraphicsItem* graphicsitem, graphicsItem()->scene()->selectedItems()) {
					if (!graphicsitem->parentItem() || !graphicsitem->parentItem()->isSelected()) {
						QPointF d = graphicsitem->mapFromScene(event->scenePos()) - graphicsitem->mapFromScene(event->lastScenePos());
						graphicsitem->translate(d.x(), d.y());
						// Regenerate the SVG transformation matrix later
						wbscene->queueTransformationChange(wbscene->findWbItem(graphicsitem));
					}
				}
			}
		} else if(event->modifiers() == Qt::KeyboardModifiers(Qt::ControlModifier + Qt::AltModifier)) {
			// Ctrl + Alt: Rotate
			// get center coordinates in item coordinates
			QPointF c = center();
			QMatrix translation, delta;
			// translates the the item's center to the origin of the item coordinates
			translation.translate(-c.x(), -c.y());
			// Rotate. Determine the direction relative to the center
			// Divide the sum by two to reduce the "speed" of rotation
			QPointF difference = event->scenePos() - event->lastScenePos();
	// 		qDebug() << QString("d: %1 %2 = %3 %4 + %5 %6").arg(difference.x()).arg(difference.y()).arg(event->scenePos().x()).arg(event->scenePos().y()).arg(event->lastScenePos().x()).arg(event->lastScenePos().y());
			QPointF p = event->scenePos();
			QPointF sceneCenter = graphicsItem()->mapToScene(c);
			if(p.x() >= sceneCenter.x() && p.y() >= sceneCenter.y()) {
				delta.rotate((-difference.x() + difference.y()) / 2);
			} else if(p.x() < sceneCenter.x() && p.y() >= sceneCenter.y()) {
				delta.rotate((-difference.x() - difference.y()) / 2);
			} else if(p.x() < sceneCenter.x() && p.y() < sceneCenter.y()) {
				delta.rotate((difference.x() - difference.y()) / 2);
			} else if(p.x() >= sceneCenter.x() && p.y() < sceneCenter.y()) {
				delta.rotate((difference.x() + difference.y()) / 2);
			}
			// set the matrix
			graphicsItem()->setMatrix(graphicsItem()->matrix() * translation * delta * translation.inverted());
			// Regenerate the SVG transformation matrix later
			WbScene* wbscene = qobject_cast<WbScene*>(graphicsItem()->scene());
			if(wbscene)
				wbscene->queueTransformationChange(this);
		} else if(event->modifiers() == Qt::KeyboardModifiers(Qt::ControlModifier + Qt::ShiftModifier)) {
			// Ctrl + Shift: Scale
			// get center coordinates in item coordinates
			QPointF c = center();
			QMatrix translation, delta;
			// translates the the item's center to the origin of the item coordinates
			translation.translate(-c.x(), -c.y());
			// Scale.
			// Moving mouse up enlarges, down shrinks, y axis.
			// Moving mouse right enlarges, left shrinks, x axis.
			QPointF difference = event->scenePos() - event->lastScenePos();
			// Note: the y axis points downwards in scene coordinates
			delta.scale(1 + difference.x()/50, 1 - difference.y()/50);
			// set the matrix
			graphicsItem()->setMatrix(graphicsItem()->matrix() * translation * delta * translation.inverted());
			// Regenerate the SVG transformation matrix later
			WbScene* wbscene = qobject_cast<WbScene*>(graphicsItem()->scene());
			if(wbscene)
				wbscene->queueTransformationChange(this);
		}
	}
}

void WbItem::handleMouseReleaseEvent(QGraphicsSceneMouseEvent * event) {	
	WbScene * wbscene = qobject_cast<WbScene*>(graphicsItem()->scene());
	wbscene->regenerateTransformations();
	event->ignore();
}

/*
 *	WbUnknown
 */

WbUnknown::WbUnknown(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene *) : WbItem(id) {
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
}

QDomElement WbUnknown::svg() {
	return svgElement_.cloneNode().toElement();
}

QList<QString> WbUnknown::parseSvg(QDomElement &_svg, bool) {
	svgElement_ = _svg.cloneNode().toElement();
	return QList<QString>();
}

WbItem* WbUnknown::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbUnknown(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

/*
 *	WbRoot
 */

WbRoot::WbRoot(QGraphicsScene* scene) : WbItem("root") {
	scene_ = scene;
}

QList<QString> WbRoot::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed = WbItem::parseSvg(_svg, emitChanges);
	if(changed.contains("viewBox")) {
		QString::const_iterator itr = attributes["viewBox"].constBegin();
		QList<qreal> points = WbItem::parseNumbersList(itr);
		if(points.size() == 4)
			scene_->setSceneRect(points.at(0), points.at(1), points.at(2), points.at(3));
	}
	return changed;
}

WbItem* WbRoot::clone() {
	WbItem* cloned = new WbRoot(scene_);
	QDomElement _svg = svg();
	cloned->parseSvg(_svg, false);
	cloned->undos = undos;
	return cloned;
}

/*
 *	WbPath
 */

WbPath::WbPath(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) : QGraphicsPathItem(0, scene), WbItem(id) {
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);
	setAcceptDrops(true);
}

QPainterPath WbPath::shape() const {
	QPainterPath qpath = path();
	qpath.lineTo(qpath.currentPosition() + QPointF(2, 0));
	qpath.lineTo(qpath.currentPosition() + QPointF(-2, 2));
	qpath.closeSubpath();
	qpath.lineTo(qpath.currentPosition() + QPointF(2, 0));
	qpath.lineTo(qpath.currentPosition() + QPointF(-2, 2));
	return qpath;
}

QList<QString> WbPath::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed = WbItem::parseSvg(_svg, emitChanges);
	QPainterPath qpath;
	// 'd' && 'fill-rule'
	if(changed.contains("d")) {
		parsePathDataFast(attributes["d"], qpath);
	} else
		qpath = this->path();
	if(changed.contains("fill-rule")) {
		if (attributes["fill-rule"] == QLatin1String("nonzero"))
			qpath.setFillRule(Qt::WindingFill);
		else
			qpath.setFillRule(Qt::OddEvenFill);
	} else
		qpath.setFillRule(this->path().fillRule());
	setPath(qpath);
	return changed;
}

WbItem* WbPath::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbPath(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

void WbPath::lineTo(const QPointF &newPoint) {
	QPainterPath qpath = path();
	qpath.lineTo(newPoint);
	setPath(qpath);
	QString old = attributes["d"];
	attributes["d"] += QString("L%1,%2").arg(newPoint.x()).arg(newPoint.y());
	emit attributeChanged(id(), "d", attributes["d"], old);
}

void WbPath::quadTo(const QPointF &controlPoint, const QPointF &newPoint) {
	QPainterPath qpath = path();
	qpath.quadTo(controlPoint, newPoint);
	setPath(qpath);
	QString old = attributes["d"];
	attributes["d"] += QString("Q%1,%2 %3,%4").arg(controlPoint.x()).arg(controlPoint.y()).arg(newPoint.x()).arg(newPoint.y());
	emit attributeChanged(id(), "d", attributes["d"], old);
}

void WbPath::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	constructContextMenu()->exec(event->screenPos());
	regenerateTransform();
}

void WbPath::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseMoveEvent(event);
	if(!event->isAccepted())
		QGraphicsPathItem::mouseMoveEvent(event);
}

void WbPath::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseReleaseEvent(event);
	if(!event->isAccepted())
		QGraphicsPathItem::mouseReleaseEvent(event);
}

/*
 *	WbEllipse
 */

WbEllipse::WbEllipse(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) :  QGraphicsEllipseItem(0, scene), WbItem(id) {
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);
	setAcceptDrops(true);
}

QList<QString> WbEllipse::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed = WbItem::parseSvg(_svg, emitChanges);
	// 'rx' & 'ry'
	if(changed.contains("rx") || changed.contains("ry")) {
		bool okX, okY;
		qreal rx = attributes["rx"].toDouble(&okX);
		qreal ry = attributes["ry"].toDouble(&okY);
		// FIXME: I think it's an issue with Qt, but with very small value I sometime get when setRect()
		// QPainterPath::lineTo: Adding point where x or y is NaN, results are undefined
		// QPainterPath::cubicTo: Adding point where x or y is NaN, results are undefined
		// That's why i'm putting those minumum values in
		if(okX && okY) {
			if(rx <= 1)
				rx = 1;
			if(ry <= 1)
				ry = 1;
			setRect(-rx, -ry, 2 * rx, 2 * ry);
		} else
			setRect(0, 0, 2, 2);
	}
	return changed;
}

WbItem* WbEllipse::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbEllipse(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

void WbEllipse::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	WbItemMenu* menu = constructContextMenu();
	menu->exec(event->screenPos());
	event->accept();
}

void WbEllipse::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	handleMouseMoveEvent(event);
	if(!event->isAccepted())
		QGraphicsEllipseItem::mouseMoveEvent(event);
}

void WbEllipse::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
	handleMouseReleaseEvent(event);
	if(!event->isAccepted())
		QGraphicsEllipseItem::mouseReleaseEvent(event);
}

/*
 *	WbCircle
 */

WbCircle::WbCircle(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) :  WbEllipse(_svg, id, index, parent, scene) {
	QDomElement _svg_ = svg();
	attributes.remove("r");
	parseSvg(_svg_);
}

QList<QString> WbCircle::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed = WbItem::parseSvg(_svg, emitChanges);
	// 'r'
	if(changed.contains("r")) {
		bool ok;
		qreal r = attributes["r"].toDouble(&ok);
		if(ok && r > 0)
			setRect(-r, -r, 2 * r, 2 * r);
		else
			setRect(0, 0, 0, 0);
	}
	return changed;
}

WbItem* WbCircle::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbCircle(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

/*
 *	WbRectangle
 */

WbRectangle::WbRectangle(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) :  QGraphicsRectItem(0, scene), WbItem(id) {
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);
	setAcceptDrops(true);
}

QList<QString> WbRectangle::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed = WbItem::parseSvg(_svg, emitChanges);
	// TODO: support rx and ry
	// 'width' & 'height'
	if(changed.contains("width") || changed.contains("height")) {
		bool okW, okH;
		qreal width = attributes["width"].toDouble(&okW);
		qreal height = attributes["height"].toDouble(&okH);
		if(okW && okH && width > 0 && height > 0)
			setRect(-width/2, -height/2, width, height);
		else
			setRect(0, 0, 0, 0);
	}
	return changed;
}

WbItem* WbRectangle::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbRectangle(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

void WbRectangle::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	WbItemMenu* menu = constructContextMenu();
	menu->exec(event->screenPos());
	event->accept();
}

void WbRectangle::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	handleMouseMoveEvent(event);
	if(!event->isAccepted())
		QGraphicsRectItem::mouseMoveEvent(event);
}

void WbRectangle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
	handleMouseReleaseEvent(event);
	if(!event->isAccepted())
		QGraphicsRectItem::mouseReleaseEvent(event);
}

/*
 *	WbLine
 */

WbLine::WbLine(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) : QGraphicsPathItem(0, scene), WbItem(id) {
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);
	setAcceptDrops(true);
}

QPainterPath WbLine::shape() const {
	QPainterPath qpath = path();
	qpath.lineTo(qpath.currentPosition() + QPointF(2, 0));
	qpath.lineTo(qpath.currentPosition() + QPointF(-2, 2));
	qpath.closeSubpath();
	qpath.lineTo(qpath.currentPosition() + QPointF(2, 0));
	qpath.lineTo(qpath.currentPosition() + QPointF(-2, 2));
	return qpath;
}

QList<QString> WbLine::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed= WbItem::parseSvg(_svg, emitChanges);
	// 'x1' & 'y1' & 'x2' & 'y2'
	if(changed.contains("x1") || changed.contains("y1") || changed.contains("x2") || changed.contains("y2")) {
		bool okX1, okY1, okX2, okY2;
		qreal x1 = attributes["x1"].toDouble(&okX1);
		qreal y1 = attributes["y1"].toDouble(&okY1);
		qreal x2 = attributes["x2"].toDouble(&okX2);
		qreal y2 = attributes["y2"].toDouble(&okY2);
		if(okX1 && okY1 && okX2 && okY2) {
			QPainterPath qpath(QPointF(x1, y1));
			qpath.lineTo(QPointF(x2, y2));
			setPath(qpath);
		} else
			setPath(QPainterPath());
	}
	return changed;
}

WbItem* WbLine::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbLine(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

void WbLine::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	constructContextMenu()->exec(event->screenPos());
	regenerateTransform();
}

void WbLine::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseMoveEvent(event);
	if(!event->isAccepted())
		QGraphicsPathItem::mouseMoveEvent(event);
}

void WbLine::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseReleaseEvent(event);
	if(!event->isAccepted())
		QGraphicsPathItem::mouseReleaseEvent(event);
}

/*
 *	WbPolyline
 */

WbPolyline::WbPolyline(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) : QGraphicsPathItem(0, scene), WbItem(id) {
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);
	setAcceptDrops(true);
}

QPainterPath WbPolyline::shape() const {
	QPainterPath qpath = path();
	qpath.lineTo(qpath.currentPosition() + QPointF(2, 0));
	qpath.lineTo(qpath.currentPosition() + QPointF(-2, 2));
	qpath.closeSubpath();
	qpath.lineTo(qpath.currentPosition() + QPointF(2, 0));
	qpath.lineTo(qpath.currentPosition() + QPointF(-2, 2));
	return qpath;
}

QList<QString> WbPolyline::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed= WbItem::parseSvg(_svg, emitChanges);
	// 'points'
	if(changed.contains("points")) {
		QString::const_iterator itr = attributes["points"].constBegin();
		QList<qreal> points = WbItem::parseNumbersList(itr);
		if(points.size() > 1) {
			QPainterPath qpath(QPointF(points.takeFirst(), points.takeFirst()));
			while(points.size() > 1) {
				qpath.lineTo(QPointF(points.takeFirst(), points.takeFirst()));
			}
			setPath(qpath);
		} else
			setPath(QPainterPath());
	}
	return changed;
}

WbItem* WbPolyline::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbPolyline(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

void WbPolyline::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	constructContextMenu()->exec(event->screenPos());
	regenerateTransform();
}

void WbPolyline::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseMoveEvent(event);
	if(!event->isAccepted())
		QGraphicsPathItem::mouseMoveEvent(event);
}

void WbPolyline::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseReleaseEvent(event);
	if(!event->isAccepted())
		QGraphicsPathItem::mouseReleaseEvent(event);
}

/*
 *	WbPolygon
 */

WbPolygon::WbPolygon(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) :  WbPolyline(_svg, id, index, parent, scene) {
	QDomElement _svg_ = svg();
	attributes.remove("points");
	parseSvg(_svg_);
}

QList<QString> WbPolygon::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed = WbPolyline::parseSvg(_svg, emitChanges);
	// 'points'
	if(changed.contains("points")) {
		QPainterPath qpath = path();
		qpath.closeSubpath();
		setPath(qpath);
	}
	return changed;
}

WbItem* WbPolygon::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbCircle(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

/*
 *	WbGraphicsTextItem
 */

WbGraphicsTextItem::WbGraphicsTextItem(WbText* wbtext, QGraphicsScene * scene) : QGraphicsTextItem(0, scene) {
	wbText_ = wbtext;
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);
	setTextInteractionFlags(Qt::TextInteractionFlags(Qt::NoTextInteraction));
	setAcceptDrops(true);
}

void WbGraphicsTextItem::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	if(hasFocus())
		QGraphicsTextItem::contextMenuEvent(event);
	else {
		WbItemMenu* menu = wbText_->constructContextMenu();
		// Add the default actions
		menu->addSeparator();
		QActionGroup* group = new QActionGroup(menu);
		IconAction* action = new IconAction(QObject::tr("Font..."), "psi/compact", QObject::tr("Font..."), 0, group);
		QObject::connect(action, SIGNAL(triggered()), wbText_, SLOT(setFont()));
		menu->addActionGroup(group);
	
		menu->exec(event->screenPos());
		event->accept();
	}
}

void WbGraphicsTextItem::focusOutEvent (QFocusEvent *event) {
	QGraphicsTextItem::focusOutEvent(event);
	wbText_->checkTextChanges();
	setTextInteractionFlags(Qt::TextInteractionFlags(Qt::NoTextInteraction));
// 	setFlag(QGraphicsItem::ItemIsFocusable, false);
}

void WbGraphicsTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
	if(event->modifiers() == Qt::KeyboardModifiers(Qt::NoModifier)) {
		setTextInteractionFlags(Qt::TextInteractionFlags(Qt::TextEditorInteraction));
		setFlag(QGraphicsItem::ItemIsFocusable);
	}
	QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void WbGraphicsTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	wbText_->handleMouseMoveEvent(event);
	if(!event->isAccepted())
		QGraphicsTextItem::mouseMoveEvent(event);
}

void WbGraphicsTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
	wbText_->handleMouseReleaseEvent(event);
	if(!event->isAccepted())
		QGraphicsTextItem::mouseReleaseEvent(event);
}

/*
 *	WbText
 */

WbText::WbText(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) : WbItem(id) {
	graphicsitem_ = new WbGraphicsTextItem(this, scene);
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
}

WbText::~WbText() {
	graphicsitem_->deleteLater();
}

QDomElement WbText::svg() {
	QDomElement _svg = WbItem::svg();
	QDomDocument d;
	_svg.appendChild(d.createTextNode(text_));
	return _svg;
};

QList<QString> WbText::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed = WbItem::parseSvg(_svg, emitChanges);
	QFont _font = graphicsitem_->font();
	// 'font-family'
	if(changed.contains("font-family"))
		_font.setFamily(attributes["font-family"]);
	// 'font-size'
	if(changed.contains("font-size"))
		_font.setPointSize(attributes["font-size"].toInt());
	// 'font-style'
	if(changed.contains("font-style")) {
		if(attributes["font-style"] == "normal")
			_font.setStyle(QFont::StyleNormal);
		else if(attributes["font-style"] == "italic")
			_font.setStyle(QFont::StyleItalic);
		else if(attributes["font-style"] == "oblique")
			_font.setStyle(QFont::StyleOblique);
	}
	// 'font-weight'
	if(changed.contains("font-weight"))
		_font.setWeight(((attributes["font-size"].toInt() - 100) / 800) * 99);
	if(graphicsitem_->font() != _font)
		graphicsitem_->setFont(_font);
	// text content
	QString t = _svg.text();
	if(text_ != t) {
		if(emitChanges) {
			QDomDocument d;
			QDomElement oldSvg = d.createElement("e");
			oldSvg.appendChild(d.createTextNode(text_));
			emit contentChanged(id(), _svg.cloneNode().childNodes(), oldSvg.childNodes());
			graphicsitem_->adjustSize();
		}
		text_ = t;
		graphicsitem_->setPlainText(t);
	}

	return changed;
}

WbItem* WbText::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbText(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

void WbText::checkTextChanges() {
	if(graphicsitem_->toPlainText() != text_) {
		QDomDocument d;
		QDomElement _svg = d.createElement("e");
		_svg.appendChild(d.createTextNode(graphicsitem_->toPlainText()));
		QDomElement oldSvg = d.createElement("e");
		oldSvg.appendChild(d.createTextNode(text_));
		emit contentChanged(id(), _svg.cloneNode().childNodes(), oldSvg.childNodes());
		text_ = graphicsitem_->toPlainText();
		graphicsitem_->adjustSize();
	}
}

void WbText::setFont() {
	bool ok;
	QFont _font = QFontDialog::getFont(&ok, graphicsitem_->font());
	if(ok) {
		// Not supported features
		_font.setUnderline(false);
		_font.setOverline(false);
		_font.setStrikeOut(false);
		_font.setFixedPitch(false);
		// end: Not supported features
		if(_font != graphicsitem_->font()) {
			if(_font.family() != graphicsitem_->font().family())
				emit attributeChanged(id(), "font-family", _font.family(), graphicsitem_->font().family());
			if(_font.pointSize() != graphicsitem_->font().pointSize())
				emit attributeChanged(id(), "font-size", QString("%1").arg(_font.pointSize()), QString("%1").arg(graphicsitem_->font().pointSize()));
			if(_font.style() != graphicsitem_->font().style()) {
				QString oldStyle, newStyle;
				switch(graphicsitem_->font().style()) {
					case QFont::StyleNormal:
						oldStyle = "normal";
						break;
					case QFont::StyleItalic:
						oldStyle = "italic";
						break;
					case QFont::StyleOblique:
						oldStyle = "oblique";
						break;
				}
				switch(_font.style()) {
					case QFont::StyleNormal:
						newStyle = "normal";
						break;
					case QFont::StyleItalic:
						newStyle = "italic";
						break;
					case QFont::StyleOblique:
						newStyle = "oblique";
						break;
				}
				emit attributeChanged(id(), "font-style", newStyle, oldStyle);
			}
			if(_font.weight() != graphicsitem_->font().weight()) {
				int oldWeight = (graphicsitem_->font().weight() - (graphicsitem_->font().weight() % 10)) * 10;
				int newWeight = (_font.weight() - (_font.weight() % 10)) * 10;
				if(oldWeight < 100)
					oldWeight = 100;
				if(newWeight < 100)
					newWeight = 100;
				emit attributeChanged(id(), "font-weight", QString("%1").arg(newWeight), QString("%1").arg(oldWeight));
				_font.setWeight(((newWeight - 100) / 800) * 99);
			}
			graphicsitem_->setFont(_font);
		}
	}
}

/*
 *	WbImage
 */

// WbImage::WbImage(const QString &id, qreal index, const QPointF &startPoint, const QString &parent, QGraphicsScene * scene) : QGraphicsPathItem(0, scene), WbItem(id) {
// 	setIndex(index);
// 	attributes["d"] = QString("M %1 %2").arg(startPoint.x()).arg(startPoint.y());
// 	setPath(QPainterPath(startPoint));
// 	setFlag(QGraphicsItem::ItemIsSelectable);
// 	setFlag(QGraphicsItem::ItemIsMovable);
// 	setAcceptDrops(true);
// }

WbImage::WbImage(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) : QGraphicsPixmapItem(0, scene), WbItem(id) {
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);
	setAcceptDrops(true);
}

QList<QString> WbImage::parseSvg(QDomElement &_svg, bool emitChanges) {
	QList<QString> changed = WbItem::parseSvg(_svg, emitChanges);
	if(changed.contains("xlink:href") || changed.contains("width") || changed.contains("height")) {
		QString header = attributes["xlink:href"].left(23);
		// we're looking for something like this "data:;image/png;base64,"
		if(header.startsWith("data:")) {
			if(header.left(12).contains("image/")) {
				if(header.left(15).contains("png") || header.left(15).contains("jpg") || header.left(16).contains("jpeg") || header.left(15).contains("bmp") || header.left(15).contains("gif") || header.left(15).contains("pbm") || header.left(15).contains("pgm") || header.left(15).contains("ppm") || header.left(15).contains("xbm") || header.left(15).contains("xpm")) {
					if(header.contains("base64,")) {
// 						qDebug() << "data: " << attributes["xlink:href"].mid(header.indexOf("base64,") + 7);
						QCA::Base64 decoder(QCA::Decode);
						decoder.setLineBreaksEnabled(true);
						QImage image = QImage::fromData(decoder.stringToArray(attributes["xlink:href"].mid(header.indexOf("base64,") + 7)).toByteArray());
						bool okW, okH;
						int width = static_cast<int>(attributes["width"].toDouble(&okW));
						int height = static_cast<int>(attributes["height"].toDouble(&okH));
						// TODO: aspect ratio
						if(okW && okH)
							image = image.scaled(QSize(width, height), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
						setPixmap(QPixmap::fromImage(image));
					}
				}
			}
		}
	}
	return changed;
}

WbItem* WbImage::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbImage(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

void WbImage::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	constructContextMenu()->exec(event->screenPos());
	regenerateTransform();
}

void WbImage::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseMoveEvent(event);
	if(!event->isAccepted())
		QGraphicsPixmapItem::mouseMoveEvent(event);
}

void WbImage::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseReleaseEvent(event);
	if(!event->isAccepted())
		QGraphicsPixmapItem::mouseReleaseEvent(event);
}

/*
 *	WbGroup
 */

WbGroup::WbGroup(QDomElement &_svg, const QString &id, const qreal &index, const QString &parent, QGraphicsScene * scene) : QGraphicsItem(0, scene), WbItem(id) {
	setParentWbItem(parent);
	setIndex(index);
	parseSvg(_svg);
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);
	setAcceptDrops(true);
	setHandlesChildEvents(true);
}

WbItem* WbGroup::clone() {
	QDomElement _svg = svg();
	WbItem* cloned = new WbGroup(_svg, id(), index(), parentWbItem(), 0);
	cloned->undos = undos;
	return cloned;
}

void WbGroup::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) {
	if (option->state & QStyle::State_Selected) {
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(boundingRect_);
	}
}

QRectF WbGroup::boundingRect() const {
	return boundingRect_;
}

QVariant WbGroup::itemChange(GraphicsItemChange change, const QVariant &value) {
	if(change == QGraphicsItem::ItemChildAddedChange) {
		QGraphicsItem* child = qVariantValue<QGraphicsItem *>(value);
		if(child) {
			if(QGraphicsItem::children().size() == 1)
				boundingRect_ = mapFromItem(child, child->boundingRect()).boundingRect();
			else
				addToBoundingRect(mapFromItem(child, child->boundingRect()).boundingRect());
		}
	} else if(change == QGraphicsItem::ItemChildRemovedChange) {
		QGraphicsItem* child = qVariantValue<QGraphicsItem *>(value);
		if(child) {
			QRectF itemrect = mapFromItem(child, child->boundingRect()).boundingRect();
			if(itemrect.x() == boundingRect_.x() || itemrect.y() == boundingRect_.y() || itemrect.right() == boundingRect_.right() || itemrect.bottom() == boundingRect_.bottom()) {
				// If the item was on one of the borders of the boundingRect_ recalculate it
				if(!QGraphicsItem::children().isEmpty()) {
					boundingRect_ = QGraphicsItem::children().first()->boundingRect();
					for(int i = 1; i < QGraphicsItem::children().size(); i++)
						addToBoundingRect(QGraphicsItem::children().at(i)->boundingRect());
				} else
					boundingRect_ = QRectF();
			}
		}
	}
	return value;
}

void WbGroup::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	constructContextMenu()->exec(event->screenPos());
	regenerateTransform();
}

void WbGroup::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseMoveEvent(event);
	if(!event->isAccepted())
		QGraphicsItem::mouseMoveEvent(event);
}

void WbGroup::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
	handleMouseReleaseEvent(event);
	if(!event->isAccepted())
		QGraphicsItem::mouseReleaseEvent(event);
}

void WbGroup::addToBoundingRect(const QRectF &itemRect) {
	if(itemRect.left() < boundingRect_.left())
		boundingRect_.adjust(itemRect.left() - boundingRect_.left(), 0, 0, 0);
	if(itemRect.top() < boundingRect_.top())
		boundingRect_.adjust(0, itemRect.top() - boundingRect_.top(), 0, 0);
	if(itemRect.right() > boundingRect_.right())
		boundingRect_.adjust(0, 0, itemRect.right() - boundingRect_.right(), 0);
	if(itemRect.bottom() > boundingRect_.bottom())
		boundingRect_.adjust(0, 0, 0, itemRect.bottom() - boundingRect_.bottom());
}

/*
 * Parsing
 ******************/

/*
 * Many of he following methods are copied and/or adapted from
 * the Qt Svg module's file called qsvghandler.cpp.
 * The original header is shown below:
 */

/****************************************************************************
**
** Copyright (C) 1992-2006 Trolltech AS. All rights reserved.
**
** This file is part of the QtSVG module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

// WbItem::WbItem(const QDomElement &svg, QObject * parent, QGraphicsScene * scene) : QGraphicsItem(parent, scene) {
// 
// }

/*
 * Parsing: General
 */

// QString WbItem::xmlSimplify(const QString &str)
// {
//	 QString dummy = str;
//	 dummy.remove('\n');
//	 if (dummy.trimmed().isEmpty())
//		 return QString();
//	 QString temp;
//	 QString::const_iterator itr = dummy.constBegin();
//	 bool wasSpace = false;
//	 for (;itr != dummy.constEnd(); ++itr) {
//		 if ((*itr).isSpace()) {
//			 if (wasSpace || !(*itr).isPrint()) {
//				 continue;
//			 }
//			 temp += *itr;
//			 wasSpace = true;
//		 } else {
//			 temp += *itr;
//			 wasSpace = false;
//		 }
//	 }
//	 return temp;
// }

QList<qreal> WbItem::parseNumbersList(QString::const_iterator &itr)
{
	QList<qreal> points;
	QString temp;
	while ((*itr).isSpace())
		++itr;
	while ((*itr).isNumber() ||
		   (*itr) == '-' || (*itr) == '+' || (*itr) == '.') {
		temp = QString();

		if ((*itr) == '-')
			temp += *itr++;
		else if ((*itr) == '+')
			temp += *itr++;
		while ((*itr).isDigit())
			temp += *itr++;
		if ((*itr) == '.')
			temp += *itr++;
		while ((*itr).isDigit())
			temp += *itr++;
		if (( *itr) == 'e') {
			temp += *itr++;
			if ((*itr) == '-' ||
				(*itr) == '+')
				temp += *itr++;
		}
		while ((*itr).isDigit())
			temp += *itr++;
		while ((*itr).isSpace())
			++itr;
		if ((*itr) == ',')
			++itr;
		points.append(temp.toDouble());
		//eat the rest of space
		while ((*itr).isSpace())
			++itr;
	}

	return points;
}

QList<qreal> WbItem::parsePercentageList(QString::const_iterator &itr)
{
	QList<qreal> points;
	QString temp;
	while ((*itr).isSpace())
		++itr;
	while ((*itr).isNumber() ||
		   (*itr) == '-' || (*itr) == '+') {
		temp = QString();

		if ((*itr) == '-')
			temp += *itr++;
		else if ((*itr) == '+')
			temp += *itr++;
		while ((*itr).isDigit())
			temp += *itr++;
		if ((*itr) == '.')
			temp += *itr++;
		while ((*itr).isDigit())
			temp += *itr++;
		if (( *itr) == '%') {
			itr++;
		}
		while ((*itr).isSpace())
			++itr;
		if ((*itr) == ',')
			++itr;
		points.append(temp.toDouble());
		//eat the rest of space
		while ((*itr).isSpace())
			++itr;
	}

	return points;
}

//inline 
bool WbItem::isUnreserved(const QChar &c, bool first_char)
{
	return (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char(':')
			|| (!first_char && (c == QLatin1Char('.') || c == QLatin1Char('-'))));

}

QString WbItem::idFromUrl(const QString &url)
{
	QString::const_iterator itr = url.constBegin();
	while ((*itr).isSpace())
		++itr;
	if ((*itr) == '(')
		++itr;
	while ((*itr).isSpace())
		++itr;
	if ((*itr) == '#')
		++itr;
	QString id;

	if (isUnreserved(*itr, true)) {		
		while (isUnreserved(*itr)) {
			id += *itr;
			++itr;	
		}
	}

	return id;
}

QMatrix WbItem::parseTransformationMatrix(const QString &value)
{
	if(value.isEmpty())
		return QMatrix();
	QMatrix matrix;
	QString::const_iterator itr = value.constBegin();

	while (itr != value.constEnd()) {
		if ((*itr) == 'm') {  //matrix
			QString temp("m");
			int remains = 6;
			while (remains--) {
				temp += *itr++;
			}

			while ((*itr).isSpace())
				++itr;
			++itr;// '('
			QList<qreal> points = WbItem::parseNumbersList(itr);
			++itr; // ')'

			Q_ASSERT(points.count() == 6);
			matrix = matrix * QMatrix(points[0], points[1],
									  points[2], points[3],
									  points[4], points[5]);

			//qDebug()<<"matrix is "<<temp;
		} else if ((*itr) == 't') { //translate
			QString trans;
			int remains = 9;
			while (remains--) {
				trans += *itr++;
			}
			while ((*itr).isSpace())
				++itr;
			++itr;// '('
			QList<qreal> points = WbItem::parseNumbersList(itr);
			++itr; // ')'

			Q_ASSERT(points.count() == 2 ||
					 points.count() == 1);
			if (points.count() == 2)
				matrix.translate(points[0], points[1]);
			else
				matrix.translate(points[0], 0);

			//qDebug()<<"trans is "<<points;
		} else if ((*itr) == 'r') { //rotate
			QString rot;
			int remains = 6;
			while (remains--) {
				rot += *itr++;
			}
			while ((*itr).isSpace())
				++itr;

			++itr;// '('
			QList<qreal> points = WbItem::parseNumbersList(itr);
			++itr;// ')'
			Q_ASSERT(points.count() == 3 ||
					 points.count() == 1);
			if (points.count() == 3) {
				matrix.translate(points[1], points[2]);
				matrix.rotate(points[0]);
				matrix.translate(-points[1], -points[2]);
			}
			else
				matrix.rotate(points[0]);

			//qDebug()<<"rot is "<<points;
		} else if ((*itr) == 's') { //scale | skewX | skewY
			QString temp;
			int remains = 5;
			while (remains--) {
				temp += *itr++;
			}
			while ((*itr).isSpace())
				++itr;

			++itr;// '('
			QList<qreal> points = WbItem::parseNumbersList(itr);
			++itr;// ')'
			Q_ASSERT(points.count() == 2 ||
					 points.count() == 1);
			if (temp == QLatin1String("scale")) {
				if (points.count() == 2) {
					matrix.scale(points[0], points[1]);
				}
				else
					matrix.scale(points[0], points[0]);
			} else if (temp == QLatin1String("skewX")) {
				const qreal deg2rad = qreal(0.017453292519943295769);
				matrix.shear(tan(points[0]*deg2rad), 0);
			} else if (temp == QLatin1String("skewY")) {
				const qreal deg2rad = qreal(0.017453292519943295769);
				matrix.shear(0, tan(points[0]*deg2rad));
			}
		} else if ((*itr) == ' '  ||
				   (*itr) == '\t' ||
				   (*itr) == '\n') {
			++itr;
		}
		if (itr != value.constEnd())
			++itr;
	}
	return matrix;
}

QPen WbItem::parseQPen(QString stroke, const QString &dashArray, const QString &dashOffset, const QString &linecap, const QString &linejoin, const QString &miterlimit, QString strokeopacity, QString opacity, const QString &width)
{
	// TODO: do something with 'dashOffset'
	Q_UNUSED(dashOffset);

	QPen pen;

	if (!strokeopacity.isEmpty())
		opacity = strokeopacity;

	if (!stroke.isEmpty() || !width.isEmpty()) {
		if (stroke != QLatin1String("none")) {
			if (!stroke.isEmpty()) {
				if (stroke.startsWith("url")) {
					// TODO: support stroke='url...'
//					 stroke = stroke.remove(0, 3);
//					 QString id = idFromUrl(stroke);
//					 QSvgStructureNode *group = 0;
//					 QSvgNode *dummy = node;
//					 while (dummy && (dummy->type() != QSvgNode::DOC  &&
//									  dummy->type() != QSvgNode::G	&&
//									  dummy->type() != QSvgNode::DEFS &&
//									  dummy->type() != QSvgNode::SWITCH)) {
//						 dummy = dummy->parent();
//					 }
//					 if (dummy)
//						 group = static_cast<QSvgStructureNode*>(dummy);
//					 if (group) {
//						 QSvgStyleProperty *style =
//							 group->scopeStyle(id);
//						 if (style->type() == QSvgStyleProperty::GRADIENT) {
//							 QBrush b(*((QSvgGradientStyle*)style)->qgradient());
//							 pen.setBrush(b);
//						 } else if (style->type() == QSvgStyleProperty::SOLID_COLOR) {
//							 pen.setColor(
//								 ((QSvgSolidColorStyle*)style)->qcolor());
//						 }
//					 } else {
//						 qDebug()<<"QSvgHandler::parsePen no parent group?";
//					 }
				} else {
					pen.setColor(constructColor(stroke, opacity));
				}
				//since we could inherit stroke="none"
				//we need to reset the style of our stroke to something
				pen.setStyle(Qt::SolidLine);
			}
			if (!width.isEmpty()) {
// //				QSvgHandler::LengthType lt;
				qreal widthF = parseLength(width);
				// TODO: support percentage widths
				if (!widthF) {
					pen.setWidthF(1);
//					 return;
				}
				pen.setWidthF(widthF);
			}
			qreal penw = pen.widthF();

			if (!linejoin.isEmpty()) {
				if (linejoin == "miter")
					pen.setJoinStyle(Qt::MiterJoin);
				else if (linejoin == "round")
					pen.setJoinStyle(Qt::RoundJoin);
				else if (linejoin == "bevel")
					pen.setJoinStyle(Qt::BevelJoin);
			}
			if (!miterlimit.isEmpty()) {
				pen.setMiterLimit(miterlimit.toDouble()/2);
			}

			if (!linecap.isEmpty()) {
				if (linecap == "butt")
					pen.setCapStyle(Qt::FlatCap);
				else if (linecap == "round")
					pen.setCapStyle(Qt::RoundCap);
				else if (linecap == "square")
					pen.setCapStyle(Qt::SquareCap);
			}

			if (!dashArray.isEmpty()) {
				QString::const_iterator itr = dashArray.constBegin();
				QList<qreal> dashes = parseNumbersList(itr);
				QVector<qreal> vec(dashes.size());

				int i = 0;
				foreach(qreal dash, dashes) {
					vec[i++] = dash/penw;
				}
				pen.setDashPattern(vec);
			}

		} else {
			pen.setStyle(Qt::NoPen);
		}
	} else {
	// TODO: check what the actual defaults are
		pen = QPen(QColor(Qt::black), 1, Qt::NoPen, Qt::FlatCap, Qt::MiterJoin);
	}
	return pen;
}

QColor WbItem::constructColor(const QString &colorStr, const QString &opacity)
{
	QColor c = resolveColor(colorStr);
	if (!opacity.isEmpty()) {
		qreal op = opacity.toDouble();
		if (op <= 1)
			op *= 255;
		c.setAlpha(int(op));
	}
	return c;
}

QColor WbItem::resolveColor(const QString &colorStr)
{
	QColor color;
	static QHash<QString, QColor> colors;
	QString colorStrTr = colorStr.trimmed();
	if (colors.isEmpty()) {
		colors.insert("black",   QColor(  0,   0,   0));
		colors.insert("green",   QColor(  0, 128,   0));
		colors.insert("silver",  QColor(192, 192, 192));
		colors.insert("lime",	QColor(  0, 255,   0));
		colors.insert("gray",	QColor(128, 128, 128));
		colors.insert("olive",   QColor(128, 128,   0));
		colors.insert("white",   QColor(255, 255, 255));
		colors.insert("yellow",  QColor(255, 255,   0));
		colors.insert("maroon",  QColor(128,   0,   0));
		colors.insert("navy",	QColor(  0,   0, 128));
		colors.insert("red",	 QColor(255,   0,   0));
		colors.insert("blue",	QColor(  0,   0, 255));
		colors.insert("purple",  QColor(128,   0, 128));
		colors.insert("teal",	QColor(  0, 128, 128));
		colors.insert("fuchsia", QColor(255,   0, 255));
		colors.insert("aqua",	QColor(  0, 255, 255));
	}
	if (colors.contains(colorStrTr)) {
		color = colors[colorStrTr];
	} else if (colorStr.startsWith("rgb(")) {
		QString::const_iterator itr = colorStr.constBegin();
		++itr; ++itr; ++itr; ++itr;
		QString::const_iterator itr_back = itr;
		QList<qreal> compo = parseNumbersList(itr);
		//1 means that it failed after reaching non-parsable
		//character which is going to be "%"
		if (compo.size() == 1) {
			itr = itr_back;
			compo = parsePercentageList(itr);
			compo[0] *= 2.55;
			compo[1] *= 2.55;
			compo[2] *= 2.55;
		}

		color = QColor(int(compo[0]),
					   int(compo[1]),
					   int(compo[2]));
	} else if (colorStr == QLatin1String("inherited") ||
			   colorStr == QLatin1String("inherit"))  {
		// TODO: handle inherited color
		color = QColor(Qt::black);
	} else if (colorStr == QLatin1String("currentColor")) {
		//TODO: handle current Color
		color = QColor(Qt::black);
	}
	return color;
}

qreal WbItem::parseLength(const QString &str)
{
	QString numStr = str.trimmed();
	qreal len = 0;

	if (numStr.endsWith("%")) {
		numStr.chop(1);
		len = numStr.toDouble();
// 		type = QSvgHandler::PERCENT;
	} else if (numStr.endsWith("px")) {
		numStr.chop(2);
		len = numStr.toDouble();
// 		type = QSvgHandler::PX;
	} else if (numStr.endsWith("pc")) {
		numStr.chop(2);
		len = numStr.toDouble();
// 		type = QSvgHandler::PC;
	} else if (numStr.endsWith("pt")) {
		numStr.chop(2);
		len = numStr.toDouble();
// 		type = QSvgHandler::PT;
	} else if (numStr.endsWith("mm")) {
		numStr.chop(2);
		len = numStr.toDouble();
// 		type = QSvgHandler::MM;
	} else if (numStr.endsWith("cm")) {
		numStr.chop(2);
		len = numStr.toDouble();
// 		type = QSvgHandler::CM;
	} else if (numStr.endsWith("in")) {
		numStr.chop(2);
		len = numStr.toDouble();
// 		type = QSvgHandler::IN;
	} else {
		len = numStr.toDouble();
// 		type = handler->defaultCoordinateSystem();
		//type = QSvgHandler::OTHER;
	}
	//qDebug()<<"len is "<<len<<", from "<<numStr;
	return len;
}

/*!
 *	Parsing: WbPath	
 */

bool WbPath::parsePathDataFast(const QString &data, QPainterPath &path)
{
	QString::const_iterator itr = data.constBegin();
	qreal x0 = 0, y0 = 0;			  // starting point
	qreal x = 0, y = 0;				// current point
	char lastMode = 0;
	QChar pathElem;
	QPointF ctrlPt;

	while (itr != data.constEnd()) {
		while ((*itr).isSpace())
			++itr;
		pathElem = *itr;
		++itr;
		QList<qreal> arg = WbItem::parseNumbersList(itr);
		if (pathElem == 'z' || pathElem == 'Z')
			arg.append(0);//dummy
		while (!arg.isEmpty()) {
			qreal offsetX = x;		// correction offsets
			qreal offsetY = y;		// for relative commands
			switch (pathElem.toAscii()) {
			case 'm': {
				x = x0 = arg[0] + offsetX;
				y = y0 = arg[1] + offsetY;
				path.moveTo(x0, y0);
				arg.pop_front(); arg.pop_front();
			}
				break;
			case 'M': {
				x = x0 = arg[0];
				y = y0 = arg[1];
				path.moveTo(x0, y0);
				arg.pop_front(); arg.pop_front();
			}
				break;
			case 'z':
			case 'Z': {
				x = x0;
				y = y0;
				path.closeSubpath();
				arg.pop_front();//pop dummy
			}
				break;
			case 'l': {
				x = arg.front() + offsetX;
				arg.pop_front();
				y = arg.front() + offsetY;
				arg.pop_front();
				path.lineTo(x, y);

			}
				break;
			case 'L': {
				x = arg.front(); arg.pop_front();
				y = arg.front(); arg.pop_front();
				path.lineTo(x, y);
			}
				break;
			case 'h': {
				x = arg.front() + offsetX; arg.pop_front();
				path.lineTo(x, y);
			}
				break;
			case 'H': {
				x = arg[0];
				path.lineTo(x, y);
				arg.pop_front();
			}
				break;
			case 'v': {
				y = arg[0] + offsetY;
				path.lineTo(x, y);
				arg.pop_front();
			}
				break;
			case 'V': {
				y = arg[0];
				path.lineTo(x, y);
				arg.pop_front();
			}
				break;
			case 'c': {
				QPointF c1(arg[0]+offsetX, arg[1]+offsetY);
				QPointF c2(arg[2]+offsetX, arg[3]+offsetY);
				QPointF e(arg[4]+offsetX, arg[5]+offsetY);
				path.cubicTo(c1, c2, e);
				ctrlPt = c2;
				x = e.x();
				y = e.y();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				break;
			}
			case 'C': {
				QPointF c1(arg[0], arg[1]);
				QPointF c2(arg[2], arg[3]);
				QPointF e(arg[4], arg[5]);
				path.cubicTo(c1, c2, e);
				ctrlPt = c2;
				x = e.x();
				y = e.y();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				break;
			}
			case 's': {
				QPointF c1;
				if (lastMode == 'c' || lastMode == 'C' ||
					lastMode == 's' || lastMode == 'S')
					c1 = QPointF(2*x-ctrlPt.x(), 2*y-ctrlPt.y());
				else
					c1 = QPointF(x, y);
				QPointF c2(arg[0]+offsetX, arg[1]+offsetY);
				QPointF e(arg[2]+offsetX, arg[3]+offsetY);
				path.cubicTo(c1, c2, e);
				ctrlPt = c2;
				x = e.x();
				y = e.y();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				break;
			}
			case 'S': {
				QPointF c1;
				if (lastMode == 'c' || lastMode == 'C' ||
					lastMode == 's' || lastMode == 'S')
					c1 = QPointF(2*x-ctrlPt.x(), 2*y-ctrlPt.y());
				else
					c1 = QPointF(x, y);
				QPointF c2(arg[0], arg[1]);
				QPointF e(arg[2], arg[3]);
				path.cubicTo(c1, c2, e);
				ctrlPt = c2;
				x = e.x();
				y = e.y();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				break;
			}
			case 'q': {
				QPointF c(arg[0]+offsetX, arg[1]+offsetY);
				QPointF e(arg[2]+offsetX, arg[3]+offsetY);
				path.quadTo(c, e);
				ctrlPt = c;
				x = e.x();
				y = e.y();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				break;
			}
			case 'Q': {
				QPointF c(arg[0], arg[1]);
				QPointF e(arg[2], arg[3]);
				path.quadTo(c, e);
				ctrlPt = c;
				x = e.x();
				y = e.y();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				break;
			}
			case 't': {
				QPointF e(arg[0]+offsetX, arg[1]+offsetY);
				QPointF c;
				if (lastMode == 'q' || lastMode == 'Q' ||
					lastMode == 't' || lastMode == 'T')
					c = QPointF(2*x-ctrlPt.x(), 2*y-ctrlPt.y());
				else
					c = QPointF(x, y);
				path.quadTo(c, e);
				ctrlPt = c;
				x = e.x();
				y = e.y();
				arg.pop_front(); arg.pop_front();
				break;
			}
			case 'T': {
				QPointF e(arg[0], arg[1]);
				QPointF c;
				if (lastMode == 'q' || lastMode == 'Q' ||
					lastMode == 't' || lastMode == 'T')
					c = QPointF(2*x-ctrlPt.x(), 2*y-ctrlPt.y());
				else
					c = QPointF(x, y);
				path.quadTo(c, e);
				ctrlPt = c;
				x = e.x();
				y = e.y();
				arg.pop_front(); arg.pop_front();
				break;
			}
			case 'a': {
				qreal rx = arg[0];
				qreal ry = arg[1];
				qreal xAxisRotation = arg[2];
				qreal largeArcFlag  = arg[3];
				qreal sweepFlag = arg[4];
				qreal ex = arg[5] + offsetX;
				qreal ey = arg[6] + offsetY;
				qreal curx = x;
				qreal cury = y;
				pathArc(path, rx, ry, xAxisRotation, int(largeArcFlag),
						int(sweepFlag), ex, ey, curx, cury);

				x = ex;
				y = ey;

				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				arg.pop_front();
			}
				break;
			case 'A': {
				qreal rx = arg[0];
				qreal ry = arg[1];
				qreal xAxisRotation = arg[2];
				qreal largeArcFlag  = arg[3];
				qreal sweepFlag = arg[4];
				qreal ex = arg[5];
				qreal ey = arg[6];
				qreal curx = x;
				qreal cury = y;
				pathArc(path, rx, ry, xAxisRotation, int(largeArcFlag),
						int(sweepFlag), ex, ey, curx, cury);
				x = ex;
				y = ey;
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				arg.pop_front(); arg.pop_front();
				arg.pop_front();
			}
				break;
			default:
				qDebug() << QString("path data is ") << pathElem;
				Q_ASSERT(!"invalid path data");
				break;
			}
			lastMode = pathElem.toAscii();
		}
	}
	return true;
}

void WbPath::pathArcSegment(QPainterPath &path, qreal xc, qreal yc, qreal th0, qreal th1, qreal rx, qreal ry, qreal xAxisRotation)
{
	qreal sinTh, cosTh;
	qreal a00, a01, a10, a11;
	qreal x1, y1, x2, y2, x3, y3;
	qreal t;
	qreal thHalf;

	sinTh = sin(xAxisRotation * (M_PI / 180.0));
	cosTh = cos(xAxisRotation * (M_PI / 180.0));

	a00 =  cosTh * rx;
	a01 = -sinTh * ry;
	a10 =  sinTh * rx;
	a11 =  cosTh * ry;

	thHalf = 0.5 * (th1 - th0);
	t = (8.0 / 3.0) * sin(thHalf * 0.5) * sin(thHalf * 0.5) / sin(thHalf);
	x1 = xc + cos(th0) - t * sin(th0);
	y1 = yc + sin(th0) + t * cos(th0);
	x3 = xc + cos(th1);
	y3 = yc + sin(th1);
	x2 = x3 + t * sin(th1);
	y2 = y3 - t * cos(th1);

	path.cubicTo(a00 * x1 + a01 * y1, a10 * x1 + a11 * y1,
				 a00 * x2 + a01 * y2, a10 * x2 + a11 * y2,
				 a00 * x3 + a01 * y3, a10 * x3 + a11 * y3);
}

// the arc handling code underneath is from XSVG (BSD license)
/*
 * Copyright  2002 USC/Information Sciences Institute
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Information Sciences Institute not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission.  Information Sciences Institute
 * makes no representations about the suitability of this software for
 * any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * INFORMATION SCIENCES INSTITUTE DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL INFORMATION SCIENCES
 * INSTITUTE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
void WbPath::pathArc(QPainterPath &path,
					qreal	rx,
					qreal	ry,
					qreal	x_axis_rotation,
					int		large_arc_flag,
					int		sweep_flag,
					qreal	x,
					qreal	y,
					qreal curx, qreal cury)
{
	qreal sin_th, cos_th;
	qreal a00, a01, a10, a11;
	qreal x0, y0, x1, y1, xc, yc;
	qreal d, sfactor, sfactor_sq;
	qreal th0, th1, th_arc;
	int i, n_segs;
	qreal dx, dy, dx1, dy1, Pr1, Pr2, Px, Py, check;

	rx = qAbs(rx);
	ry = qAbs(ry);

	sin_th = sin(x_axis_rotation * (M_PI / 180.0));
	cos_th = cos(x_axis_rotation * (M_PI / 180.0));

	dx = (curx - x) / 2.0;
	dy = (cury - y) / 2.0;
	dx1 =  cos_th * dx + sin_th * dy;
	dy1 = -sin_th * dx + cos_th * dy;
	Pr1 = rx * rx;
	Pr2 = ry * ry;
	Px = dx1 * dx1;
	Py = dy1 * dy1;
	/* Spec : check if radii are large enough */
	check = Px / Pr1 + Py / Pr2;
	if (check > 1) {
		rx = rx * sqrt(check);
		ry = ry * sqrt(check);
	}

	a00 =  cos_th / rx;
	a01 =  sin_th / rx;
	a10 = -sin_th / ry;
	a11 =  cos_th / ry;
	x0 = a00 * curx + a01 * cury;
	y0 = a10 * curx + a11 * cury;
	x1 = a00 * x + a01 * y;
	y1 = a10 * x + a11 * y;
	/* (x0, y0) is current point in transformed coordinate space.
	   (x1, y1) is new point in transformed coordinate space.

	   The arc fits a unit-radius circle in this space.
	*/
	d = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
	sfactor_sq = 1.0 / d - 0.25;
	if (sfactor_sq < 0) sfactor_sq = 0;
	sfactor = sqrt(sfactor_sq);
	if (sweep_flag == large_arc_flag) sfactor = -sfactor;
	xc = 0.5 * (x0 + x1) - sfactor * (y1 - y0);
	yc = 0.5 * (y0 + y1) + sfactor * (x1 - x0);
	/* (xc, yc) is center of the circle. */

	th0 = atan2(y0 - yc, x0 - xc);
	th1 = atan2(y1 - yc, x1 - xc);

	th_arc = th1 - th0;
	if (th_arc < 0 && sweep_flag)
		th_arc += 2 * M_PI;
	else if (th_arc > 0 && !sweep_flag)
		th_arc -= 2 * M_PI;

	n_segs = int(ceil(qAbs(th_arc / (M_PI * 0.5 + 0.001))));

	for (i = 0; i < n_segs; i++) {
		pathArcSegment(path, xc, yc,
					   th0 + i * th_arc / n_segs,
					   th0 + (i + 1) * th_arc / n_segs,
					   rx, ry, x_axis_rotation);
	}
}
