/*
 * wbitem.cpp - the item classes for the SVG WB
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

#include "wbitem.h"
#include "wbscene.h"
#include "wbwidget.h"
#include <QRegExp>
#include <QDebug>
#include <QSvgRenderer>

#include <math.h>
static QMatrix parseTransformationMatrix(const QString &value);

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
 *   WbItem
 */

WbItem::WbItem(SxeSession* session, QSvgRenderer* renderer, QDomElement node, WbScene* scene, WbWidget* widget) : QGraphicsSvgItem() {
	// Store a pointer to the underlying session, scene and node
	session_ = session;
	scene_ = scene;
	widget_ = widget;
	node_ = node;

	// qDebug() << QString("constructing %1.").arg(id()).toAscii();

	if(node.isNull()) {
		qDebug("Trying to create a WbItem from a null QDomNode.");
		return;
	}


	// Make the item selectable and movable
	setFlag(QGraphicsItem::ItemIsSelectable);
	setFlag(QGraphicsItem::ItemIsMovable);

	// Enable drag-n-drop
	setAcceptDrops(true);

	// Don't cache the SVG
	setCachingEnabled(false);

	// Set the renderer for the item
	setSharedRenderer(renderer);

	// add the new item to the scene
	addToScene();
}

WbItem::~WbItem() {
	// qDebug() << QString("destructing %1.").arg(id()).toAscii();
}

QString WbItem::id() {
	return node_.toElement().attribute("id");
}

QDomNode WbItem::node() {
	return node_;
}

void WbItem::contextMenuEvent (QGraphicsSceneContextMenuEvent * event) {
	 constructContextMenu()->exec(event->screenPos());
}

void WbItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
	event->accept();

	if(isSelected()) {
		if(widget_->mode() == WbWidget::Translate) {

			// Translate each selected item
			foreach(QGraphicsItem* graphicsitem, scene()->selectedItems()) {
				if (!graphicsitem->parentItem() || !graphicsitem->parentItem()->isSelected()) {
					QPointF d = graphicsitem->mapFromScene(event->scenePos()) - graphicsitem->mapFromScene(event->lastScenePos());
					graphicsitem->translate(d.x(), d.y());
					// qDebug() QString("Translated %1 by %2 %3").arg((unsigned int) graphicsitem).arg(d.x()).arg(d.y()).toAscii();

					// Regenerate the SVG transformation matrix later
					WbItem* wbitem = dynamic_cast<WbItem*>(graphicsitem);
					if(wbitem)
						scene_->queueTransformationRegeneration(wbitem);
				}
			}

		} else if(widget_->mode() == WbWidget::Rotate) {

			// Rotate each selected item
			// get center coordinates of selected items in scene coordinates
			QPointF scenePivot = scene_->selectionCenter();
			// Determine the direction relative to the center
			// Divide the sum by two to reduce the "speed" of rotation
			QPointF difference = event->scenePos() - event->lastScenePos();
//		  qDebug() << QString("d: %1 %2 = %3 %4 + %5 %6").arg(difference.x()).arg(difference.y()).arg(event->scenePos().x()).arg(event->scenePos().y()).arg(event->lastScenePos().x()).arg(event->lastScenePos().y());
			QPointF p = event->scenePos();
			QMatrix delta;
			if(p.x() >= scenePivot.x() && p.y() >= scenePivot.y()) {
				 delta.rotate((-difference.x() + difference.y()) / 2);
			} else if(p.x() < scenePivot.x() && p.y() >= scenePivot.y()) {
				 delta.rotate((-difference.x() - difference.y()) / 2);
			} else if(p.x() < scenePivot.x() && p.y() < scenePivot.y()) {
				 delta.rotate((difference.x() - difference.y()) / 2);
			} else if(p.x() >= scenePivot.x() && p.y() < scenePivot.y()) {
				 delta.rotate((difference.x() + difference.y()) / 2);
			}

			foreach(QGraphicsItem* graphicsitem, scene()->selectedItems()) {
				if (!graphicsitem->parentItem() || !graphicsitem->parentItem()->isSelected()) {
					QMatrix translation;
					// get center coordinates of selected items in item coordinates
					QPointF itemPivot = graphicsitem->mapFromScene(scenePivot);
					// translates the the item's center to the origin of the item coordinates
					translation.translate(-itemPivot.x(), -itemPivot.y());
					// set the matrix
					graphicsitem->setTransform(QTransform(translation * delta * translation.inverted()), true);

					// Regenerate the SVG transformation matrix later
					WbItem* wbitem = dynamic_cast<WbItem*>(graphicsitem);
					if(wbitem)
						scene_->queueTransformationRegeneration(wbitem);
				}
			}

		} else if(widget_->mode() == WbWidget::Scale) {

			// Scale each selected item
			foreach(QGraphicsItem* graphicsitem, scene()->selectedItems()) {
				if (!graphicsitem->parentItem() || !graphicsitem->parentItem()->isSelected()) {

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
					setTransform(QTransform(translation * delta * translation.inverted()), true);
 
					// Regenerate the SVG transformation matrix later
					scene_->queueTransformationRegeneration(dynamic_cast<WbItem*>(graphicsitem));

				}
			}

		}
	}
}

void WbItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
	QGraphicsSvgItem::mouseReleaseEvent(event);

	scene_->regenerateTransformations();
}

QMatrix WbItem::parseSvgTransform(QString string) {
	string = string.trimmed();
	if(string.isEmpty())
		return QMatrix();

	return parseTransformationMatrix(string);
}

QString WbItem::toSvgTransform(const QMatrix &m) {
	return QString("matrix(%1 %2 %3 %4 %5 %6)").arg(m.m11()).arg(m.m12()).arg(m.m21()).arg(m.m22()).arg(m.dx()).arg(m.dy());
}

void WbItem::regenerateTransform() {
	// Replace the QGraphicsItem transformation with an SVG transformation.
	// Possible as long as no perspective transformations have been applied.

	// delta is the new transformation to be applied
	QMatrix delta = transform().toAffine();
	if(delta.isIdentity())
		return;

	// get the existing SVG transformation
	QString oldValue = node_.attribute("transform");
	QMatrix oldTransform = parseSvgTransform(oldValue);

	// construct a translation that translates the item to (0,0) in scene coordinates
	QMatrix translation;
	// translates the the item's center to the origin of the item coordinates
	translation.translate(-pos().x(), -pos().y());

	// generate the string representation
	QString newValue = toSvgTransform(oldTransform * translation * delta * translation.inverted());

	if(newValue != oldValue) {
		resetTransform();
		session_->setAttribute(node_, "transform", newValue);
	}
}

void WbItem::addToScene() {
	// qDebug() << QString("adding %1 to scene.").arg(id()).toAscii();

	// Do nothing if already added and id matches
	if(scene_ == scene()
		&& node_.hasAttribute("id")
		&& node_.attribute("id") == elementId())
	{
		return;
	}

	// Read or generate an xml:id for the node
	QString id;
	if(node_.hasAttribute("id")) {
		id = node_.attribute("id");
	}
	else {
		id = "e" + SxeSession::generateUUID();
		// qDebug() << QString("Setting new id to %1").arg(id).toAscii();
		session_->setAttribute(node_, "id", id);
		session_->flush();
	}

	// Only render the indicated item
	setElementId(id);

	// Set the position
	resetPos();

	scene_->addItem(this);
}

void WbItem::removeFromScene() {
	// qDebug() << QString("removing %1 from scene.").arg(id()).toAscii();

	scene_->removeItem(this);

	setElementId(QString());
}

void WbItem::resetPos() {
	// set the x & y approriately;
	setPos(renderer()->boundsOnElement(id()).topLeft());

	// set the drawing order
	// TODO: optimize
	int i = 0;
	QDomNodeList children = node_.parentNode().childNodes();
	while(children.at(i) != node_) {
		i++;
	}

	setZValue(i);
}

WbItemMenu* WbItem::constructContextMenu() {
	 WbItemMenu* menu = new WbItemMenu(0);

	 if(scene_) {
		 // Add the default actions
		 QActionGroup* group;
		 // QAction* qaction;
		 QPixmap pixmap;

		 // group = new QActionGroup(0);
		 // pixmap(2, 2);
		 // pixmap.fill(QColor(Qt::black));
		 // qaction = new QAction(QIcon(pixmap), tr("Thin stroke"), group);
		 // qaction->setData(QVariant(1));
		 // pixmap = QPixmap(6, 6);
		 // pixmap.fill(QColor(Qt::black));
		 // qaction = new QAction(QIcon(pixmap), tr("Medium stroke"), group);
		 // qaction->setData(QVariant(3));
		 // pixmap = QPixmap(12, 12);
		 // pixmap.fill(QColor(Qt::black));
		 // qaction = new QAction(QIcon(pixmap), tr("Thick stroke"), group);
		 // qaction->setData(QVariant(6));
		 // connect(group, SIGNAL(triggered(QAction*)), wbscene, SLOT(setStrokeWidth(QAction*)));
		 // menu->addActionGroup(group);

		 // menu->addSeparator();
		 // group = new QActionGroup(0);
		 // pixmap = QPixmap(16, 16);
		 // pixmap.fill(QColor(Qt::darkCyan));
		 // qaction = new QAction(QIcon(pixmap), tr("Change stroke color"), group);
		 // connect(qaction, SIGNAL(triggered()), wbscene, SLOT(setStrokeColor()));
		 // pixmap.fill(QColor(Qt::darkYellow));
		 // qaction = new QAction(QIcon(pixmap), tr("Change fill color"), group);
		 // connect(qaction, SIGNAL(triggered()), wbscene, SLOT(setFillColor()));
		 // menu->addActionGroup(group);

		 // menu->addSeparator();
		 group = new QActionGroup(0);
		 IconAction* action = new IconAction(tr("Bring forward"), "psi/bringForwards", tr("Bring forward"), 0, group);
		 connect(action, SIGNAL(triggered()), scene_, SLOT(bringForward()));
		 action = new IconAction(tr("Bring to front"), "psi/bringToFront", tr("Bring to front"), 0, group);
		 connect(action, SIGNAL(triggered()), scene_, SLOT(bringToFront()));
		 action = new IconAction(tr("Send backwards"), "psi/sendBackwards", tr("Send backwards"), 0, group);
		 connect(action, SIGNAL(triggered()), scene_, SLOT(sendBackwards()));
		 action = new IconAction(tr("Send to back"), "psi/sendToBack", tr("Send to back"), 0, group);
		 connect(action, SIGNAL(triggered()), scene_, SLOT(sendToBack()));
		 menu->addActionGroup(group);

		 menu->addSeparator();
		 group = new QActionGroup(0);
		 action = new IconAction(tr("Group"), "psi/group", tr("Group"), 0, group);
		 connect(action, SIGNAL(triggered()), scene_, SLOT(group()));
		 action = new IconAction(tr("Ungroup"), "psi/ungroup", tr("Ungroup"), 0, group);
		 connect(action, SIGNAL(triggered()), scene_, SLOT(ungroup()));
		 menu->addActionGroup(group);
	 }

	 return menu;
}

QPointF WbItem::center() {
	// Determine the center of the item in item coordinates before transformation
	QRectF r = boundingRect();
	QPointF c(r.x() + r.width()/2, r.y() + r.height()/2);
//  qDebug() << QString("center: %1 + %2	%3 + %4").arg(r.x()).arg(r.width()/2).arg(r.y()).arg(r.height()/2);
	// return the center with transformation applied
	return transform().map(c);
}




// The following functions are from qt-mac-opensource-src-4.3.1/src/svg/qsvghandler.cpp
/****************************************************************************
**
** Copyright (C) 1992-2007 Trolltech ASA. All rights reserved.
**
** This file is part of the QtSVG module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://trolltech.com/products/qt/licenses/licensing/opensource/
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://trolltech.com/products/qt/licenses/licensing/licensingoverview
** or contact the sales department at sales@trolltech.com.
**
** In addition, as a special exception, Trolltech gives you certain
** additional rights. These rights are described in the Trolltech GPL
** Exception version 1.0, which can be found at
** http://www.trolltech.com/products/qt/gplexception/ and in the file
** GPL_EXCEPTION.txt in this package.
**
** In addition, as a special exception, Trolltech, as the sole copyright
** holder for Qt Designer, grants users of the Qt/Eclipse Integration
** plug-in the right for the Qt/Eclipse Integration to link to
** functionality provided by Qt Designer and its related libraries.
**
** Trolltech reserves all rights not expressly granted herein.
** 
** Trolltech ASA (c) 2007
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

double qstrtod(const char *s00, char const **se, bool *ok);

static qreal toDouble(const QChar *&str)
{
	const int maxLen = 255;//technically doubles can go til 308+ but whatever
	char temp[maxLen+1];
	int pos = 0;

	if (*str == QLatin1Char('-')) {
		temp[pos++] = '-';
		++str;
	}
	else if (*str == QLatin1Char('+')) {
		++str;
	}
	while (*str >= QLatin1Char('0') && *str <= QLatin1Char('9') && pos < maxLen) {
		temp[pos++] = str->toLatin1();
		++str;
	}
	if (*str == QLatin1Char('.') && pos < maxLen) {
		temp[pos++] = '.';
		++str;
	}
	while (*str >= QLatin1Char('0') && *str <= QLatin1Char('9') && pos < maxLen) {
		temp[pos++] = str->toLatin1();
		++str;
	}
	bool exponent = false;
	if (*str == QLatin1Char('e') && pos < maxLen) {
		exponent = true;
		temp[pos++] = 'e';
		++str;
		if ((*str == QLatin1Char('-') || *str == QLatin1Char('+')) && pos < maxLen) {
			temp[pos++] = str->toLatin1();
			++str;
		}
		while (*str >= QLatin1Char('0') && *str <= QLatin1Char('9') && pos < maxLen) {
			temp[pos++] = str->toLatin1();
			++str;
		}
	}
	temp[pos] = '\0';

	qreal val;
	if (!exponent && pos < 10) {
		int ival = 0;
		const char *t = temp;
		bool neg = false;
		if(*t == '-') {
			neg = true;
			++t;
		}
		while(*t && *t != '.') {
			ival *= 10;
			ival += (*t) - '0';
			++t;
		}
		if(*t == '.') {
			++t;
			int div = 1;
			while(*t) {
				ival *= 10;
				ival += (*t) - '0';
				div *= 10;
				++t;
			}
			val = ((qreal)ival)/((qreal)div);
		}
		else {
			val = ival;
		}
		if (neg)
			val = -val;
	} else {
#ifdef Q_WS_QWS
		if(sizeof(qreal) == sizeof(float))
			val = strtof(temp, 0);
		else
#endif
		{
			bool ok = false;
			val = qstrtod(temp, 0, &ok);
		}
	}
	return val;

}

static QVector<qreal> parseNumbersList(const QChar *&str)
{
	QVector<qreal> points;
	if (!str)
		return points;
	points.reserve(32);

	while (*str == QLatin1Char(' ')) {
		++str;
	}
	while ((*str >= QLatin1Char('0') && *str <= QLatin1Char('9')) ||
		   *str == QLatin1Char('-') || *str == QLatin1Char('+') ||
		   *str == QLatin1Char('.')) {

		points.append(::toDouble(str));

		while (*str == QLatin1Char(' ')) {
			++str;
		}
		if (*str == QLatin1Char(',')) {
			++str;
		}

		//eat the rest of space
		while (*str == QLatin1Char(' ')) {
			++str;
		}
	}

	return points;
}

static QMatrix parseTransformationMatrix(const QString &value)
{
	QMatrix matrix;
	const QChar *str = value.constData();

	while (*str != QLatin1Char(0)) {
		if (str->isSpace() || *str == QLatin1Char(',')) {
			++str;
			continue;
		}
		enum State {
			Matrix,
			Translate,
			Rotate,
			Scale,
			SkewX,
			SkewY
		};
		State state = Matrix;
		if (*str == QLatin1Char('m')) {  //matrix
			const char *ident = "atrix";
			for (int i = 0; i < 5; ++i) {
				if (*(++str) != QLatin1Char(ident[i])) {
					goto error;
				}
			}
			++str;
			state = Matrix;
		}
		else if (*str == QLatin1Char('t')) { //translate
			const char *ident = "ranslate";
			for (int i = 0; i < 8; ++i)
				if (*(++str) != QLatin1Char(ident[i]))
					goto error;
			++str;
			state = Translate;
		}
		else if (*str == QLatin1Char('r')) { //rotate
			const char *ident = "otate";
			for (int i = 0; i < 5; ++i)
				if (*(++str) != QLatin1Char(ident[i]))
					goto error;
			++str;
			state = Rotate;
		}
		else if (*str == QLatin1Char('s')) { //scale, skewX, skewY
			++str;
			if (*str == QLatin1Char('c')) {
				const char *ident = "ale";
				for (int i = 0; i < 3; ++i)
					if (*(++str) != QLatin1Char(ident[i]))
						goto error;
				++str;
				state = Scale;
			} else if (*str == QLatin1Char('k')) {
				if (*(++str) != QLatin1Char('e'))
					goto error;
				if (*(++str) != QLatin1Char('w'))
					goto error;
				++str;
				if (*str == QLatin1Char('X'))
					state = SkewX;
				else if (*str == QLatin1Char('Y'))
					state = SkewY;
				else
					goto error;
				++str;
			} else {
				goto error;
			}
		}
		else {
			goto error;
		}


		while (str->isSpace()) {
			++str;
		}
		if (*str != QLatin1Char('(')) {
			goto error;
		}
		++str;
		QVector<qreal> points = parseNumbersList(str);
		if (*str != QLatin1Char(')')) {
			goto error;
		}
		++str;

		if(state == Matrix) {
			if(points.count() != 6) {
				goto error;
			}
			matrix = matrix * QMatrix(points[0], points[1],
									  points[2], points[3],
									  points[4], points[5]);
		}
		else if (state == Translate) {
			if (points.count() == 1)
				matrix.translate(points[0], 0);
			else if (points.count() == 2)
				matrix.translate(points[0], points[1]);
			else
				goto error;
		}
		else if (state == Rotate) {
			if(points.count() == 1) {
				matrix.rotate(points[0]);
			} else if (points.count() == 3) {
				matrix.translate(points[1], points[2]);
				matrix.rotate(points[0]);
				matrix.translate(-points[1], -points[2]);
			} else {
				goto error;
			}
		}
		else if (state == Scale) {
			if (points.count() < 1 || points.count() > 2)
				goto error;
			qreal sx = points[0];
			qreal sy = sx;
			if(points.count() == 2)
				sy = points[1];
			matrix.scale(sx, sy);
		}
		else if (state == SkewX) {
			if (points.count() != 1)
				goto error;
			const qreal deg2rad = qreal(0.017453292519943295769);
			matrix.shear(tan(points[0]*deg2rad), 0);
		}
		else if (state == SkewY) {
			if (points.count() != 1)
				goto error;
			const qreal deg2rad = qreal(0.017453292519943295769);
			matrix.shear(0, tan(points[0]*deg2rad));
		}
	}
  error:
	return matrix;
}
