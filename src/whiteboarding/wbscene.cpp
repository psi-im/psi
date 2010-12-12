/*
 * wbscene.cpp - an SVG whiteboard scene class
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

#include "wbscene.h"

WbScene::WbScene(SxeSession* session, QObject * parent) : QGraphicsScene(parent) {
	session_ = session;
};

void WbScene::queueTransformationRegeneration(WbItem* item) {
	if(item && !pendingTranformations_.contains(item))
		pendingTranformations_.append(QPointer<WbItem>(item));
}

void WbScene::regenerateTransformations() {
	foreach(QPointer<WbItem> item, pendingTranformations_) {
		if(item) {
			// qDebug() << QString("Regenerating %1 transform.").arg((unsigned int) &(*item)).toAscii();
			item->regenerateTransform();
		}
	}
	pendingTranformations_.clear();
	session_->flush();
}

QPointF WbScene::selectionCenter() const {
	QList<QGraphicsItem*> items = selectedItems();

	if(items.size() < 1) {
		return QPointF();
	}

	QRectF box = items.at(0)->sceneBoundingRect();
	foreach(QGraphicsItem* item, items) {
		box = box.united(item->sceneBoundingRect());
	}

	// qDebug() << QString("Selection center: %1 %2").arg(box.center().x()).arg(box.center().y()).toAscii();

	return box.center();
}

void WbScene::bringForward(int n) {
	bring(n, false);
}

void WbScene::bringToFront() {
	bring(1, true);
}

void WbScene::sendBackwards(int n) {
	bring(-n, false);
}

void WbScene::sendToBack() {
	bring(-1, true);
}

static bool zValueLessThan(QGraphicsItem* item1, QGraphicsItem* item2) {
	return (item1->zValue() < item2->zValue());
}

void WbScene::group() {
	if(selectedItems().size() > 1) {
		// Create the group
		QDomElement temp = QDomDocument().createElement("g");
		temp.setAttribute("id", "e" + SxeSession::generateUUID());
		const QDomNode group = session_->insertNodeAfter(temp, session_->document().documentElement());

		// Arrange the items by Z coordinate
		QList<QGraphicsItem*> selected = selectedItems();
		qSort(selected.begin(), selected.end(), zValueLessThan);

		// Reparent each selected item
		foreach(QGraphicsItem* item, selected) {
			WbItem* wbitem = dynamic_cast<WbItem*>(item);
			if(wbitem)
				session_->insertNodeAfter(wbitem->node(), group);
		}
		clearSelection();

		session_->flush();
	}
}

void WbScene::ungroup() {
	foreach(QGraphicsItem* item, selectedItems()) {
		// find the QDomElement matching the selected item
		WbItem* wbitem = dynamic_cast<WbItem*>(item);
		if(wbitem) {
			QDomElement group = wbitem->node().toElement();

			if(group.nodeName() == "g") {
				QDomNodeList children = group.childNodes();

				QMatrix groupTransform = WbItem::parseSvgTransform(group.attribute("transform"));

				for(int i = children.size() - 1; i >= 0; i--) {
					QDomElement child = children.at(i).toElement();
					if(!child.isNull()) {

						if(!groupTransform.isIdentity()) {
							// combine the transformations of the group and the child
							QMatrix childTransform = WbItem::parseSvgTransform(child.attribute("transform"));
							session_->setAttribute(child, "transform", WbItem::toSvgTransform(childTransform * groupTransform));
						}

						// move the child from the group to the root <svg/>
						session_->insertNodeAfter(children.at(i), session_->document().documentElement(), group);

					}
				}

				// delete the group itself
				session_->removeNode(group);
			}

		}
	}
	clearSelection();

	session_->flush();
}

void WbScene::bring(int n, bool toExtremum) {
	if(n == 0)
		 return;

	// bring each selected item
	foreach(QGraphicsItem* selecteditem, selectedItems()) {

		if (!(selecteditem->parentItem() && selecteditem->parentItem()->isSelected())) {

			WbItem* selectedwbitem = dynamic_cast<WbItem*>(selecteditem);

			if(selectedwbitem) {

				QList<QGraphicsItem*> colliding = selecteditem->collidingItems();

				// find the relative position of selecteditem itself and
				// remove other selected items from the list colliding
				int i = 0;
				while(i < colliding.size()) {
					if(selectedItems().contains(colliding[i])) {
						colliding.removeAt(i);
					} else if(colliding[i]->zValue() < selecteditem->zValue()) {
						break;
					} else
						i++;
				}
				if(n < 0)
					i--;

				// bring the selected node n levels above/below the item itself
				if(colliding.size() > 0) {
					WbItem* referencewbitem;
					if(n > 0) {
						if(i - n > 0 && !toExtremum)
							referencewbitem = dynamic_cast<WbItem*>(colliding[i - n]);
						else
							referencewbitem = dynamic_cast<WbItem*>(colliding[0]);
					} else {
						if(i - n < colliding.size() && !toExtremum)
							referencewbitem = dynamic_cast<WbItem*>(colliding[i - n]);
						else
							referencewbitem = dynamic_cast<WbItem*>(colliding[colliding.size() - 1]);
					}

					if(referencewbitem && !referencewbitem->node().isNull()) {
						if(n > 0)
							session_->insertNodeAfter(selectedwbitem->node(), selectedwbitem->node().parentNode(), referencewbitem->node());
						else
							session_->insertNodeBefore(selectedwbitem->node(), selectedwbitem->node().parentNode(), referencewbitem->node());
					}
				}
			}
		}
	}

	session_->flush();
}
