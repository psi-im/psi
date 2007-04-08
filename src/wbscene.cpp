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

WbScene::WbScene(const QString &session, const QString &ownJid, QObject * parent) : QGraphicsScene(parent) {
	importing = false;
	highestIndex_ = 0;
	highestId_ = 0;
	session_ = session;
	ownJid_ = ownJid;

	WbRoot* w = new WbRoot(this);
	QDomElement svg = w->svg();
	// Default values
	svg.setAttribute("viewBox", "0 0 600 600");
	svg.setAttribute("xmlns", "http://www.w3.org/2000/svg");
	svg.setAttribute("version", "1.1");
	svg.setAttribute("baseProfile", "tiny");
	w->parseSvg(svg, false);
	elements_.insert("root", w);
};

QString WbScene::session() {
	return session_;
}

const QString & WbScene::ownJid() const {
	return ownJid_;
}

bool WbScene::processWb(const QDomElement &wb) {
	QDomNodeList children = wb.childNodes();
	for(uint i=0; i < children.length(); i++) {
		if(children.item(i).nodeName() == "new")	
			processNew(children.item(i).toElement());
		else if(children.item(i).nodeName() == "configure")
			processConfigure(children.item(i).toElement());
		else if(children.item(i).nodeName() == "move")
			processMove(children.item(i).toElement());
		else if(children.item(i).nodeName() == "remove")
			processRemove(children.item(i).toElement());
	}
	return true;
}

WbItem* WbScene::findWbItem(const QString &id) const {
	if(elements_.contains(id))
		return elements_.value(id);
	else
		return 0;
}

WbItem* WbScene::findWbItem(QGraphicsItem* item) const {
	QHashIterator<QString, WbItem*> i(elements_);
	while (i.hasNext()) {
		i.next();
		if(i.value()->graphicsItem() == item)
			return i.value();
	}
	return 0;
}

void WbScene::addWbItem(WbItem *item) {
	connect(item, SIGNAL(attributeChanged(QString, QString, QString, QString)), SLOT(queueAttributeEdit(QString, QString, QString, QString)));
	connect(item, SIGNAL(parentChanged(QString, QString, QString)), SLOT(queueParentEdit(QString, QString, QString)));
	connect(item, SIGNAL(contentChanged(QString, QDomNodeList, QDomNodeList)), SLOT(queueContentEdit(QString, QDomNodeList, QDomNodeList)));
	connect(item, SIGNAL(indexChanged(QString, qreal)), SLOT(queueMove(QString, qreal)));

	if(item->graphicsItem() && item->graphicsItem()->scene() != this)
		addItem(item->graphicsItem());

	elements_.insert(item->id(), item);
}

bool WbScene::removeElement(const QString &id, bool emitChanges)
{
	WbItem* wbitem = findWbItem(id);
	if(wbitem) {
		// Reparent children to root
		if(wbitem->graphicsItem()) {
			QGraphicsLineItem lineitem(0, 0);
			foreach(QGraphicsItem* c, wbitem->graphicsItem()->children()) {
				// FIXME: should just be c->setParentItem(0);, this is a workaround for a qt bug
				c->setParentItem(&lineitem);
				c->setParentItem(0);
			}
			if(wbitem->graphicsItem()->scene())
				wbitem->graphicsItem()->scene()->removeItem(wbitem->graphicsItem());
		}
		elements_.remove(wbitem->id());
		if(emitChanges)
			queueRemove(wbitem->id());
		wbitem->deleteLater();
		return true;
	} else
		return false;
}

void WbScene::queueTransformationChange(WbItem* item) {
	if(item && !pendingTranformations_.contains(item))
		pendingTranformations_.append(QPointer<WbItem>(item));
}

void WbScene::regenerateTransformations() {
	foreach(QPointer<WbItem> item, pendingTranformations_) {
		if(item)
			item->regenerateTransform();
	}
	pendingTranformations_.clear();
}

QDomElement WbScene::element(const QString &id) const {
	WbItem* i = findWbItem(id);
	if(i)
		return i->svg();
	else
		return QDomElement();
}

QList<WbItem*> WbScene::elements() const {
	QList<QString> elementKeys = elements_.keys();
	QList<QString> returnKeys;
	// Append nodes so that parents are listed before children
	int elementsAdded = 1;
	while(elementsAdded != 0 && !elementKeys.isEmpty()) {
		elementsAdded = 0;
		foreach(QString key, elementKeys) {
			QString parent = elements_[key]->parentWbItem();
			if(parent == "root" || returnKeys.contains(parent) || !elements_.contains(parent)) {
				returnKeys.append(key);
				elementsAdded++;
			}
			elementKeys.removeAll(key);
		}
	}

	QList<WbItem*> returnedElements;
	foreach(QString key, returnKeys) {
		WbItem* element = elements_[key]->clone();
		returnedElements.append(element);
	}
	return returnedElements;
}

void WbScene::setStrokeColor() {
	QColor newColor = QColorDialog::getColor();
	if(newColor.isValid()) {
		foreach(QGraphicsItem* selecteditem, selectedItems()) {
			WbItem* selectedwbitem = findWbItem(selecteditem);
			if(selectedwbitem)
				selectedwbitem->setStrokeColor(newColor);
		}
	}
}

void WbScene::setFillColor() {
	QColor newColor = QColorDialog::getColor();
	if(newColor.isValid()) {
		foreach(QGraphicsItem* selecteditem, selectedItems()) {
			WbItem* selectedwbitem = findWbItem(selecteditem);
			if(selectedwbitem)
				selectedwbitem->setFillColor(newColor);
		}
	}
}

void WbScene::setStrokeWidth(QAction* a) {
	foreach(QGraphicsItem* selecteditem, selectedItems()) {
		WbItem* selectedwbitem = findWbItem(selecteditem);
		if(selectedwbitem)
			selectedwbitem->setStrokeWidth(a->data().toInt());
	}
}

void WbScene::bringForward(int n) {
	if(n == 0)
		return;
	// bring each selected item
	foreach(QGraphicsItem* selecteditem, selectedItems()) {
		if (!selecteditem->parentItem() || !selecteditem->parentItem()->isSelected()) {
			WbItem* selectedwbitem = findWbItem(selecteditem);
			if(selectedwbitem) {
				QList<qreal> collidingindeces;
				qreal selecteditemindex = selectedwbitem->index();
				foreach(QGraphicsItem* collidingitem, selecteditem->collidingItems()) {
					if(!selectedItems().contains(collidingitem)) {
						WbItem* collidingwbitem =findWbItem(collidingitem);
						if(collidingwbitem) {
							qreal collidingitemindex = collidingwbitem->index();
							if(collidingitemindex > selecteditemindex) {
								int i = 0;
								while(i < collidingindeces.size() && collidingindeces.at(i) < collidingitemindex)
									i++;
								collidingindeces.insert(i, collidingitemindex);
							}
						}
					}
				}
				if(!collidingindeces.isEmpty()) {
					if(n < 0 || n >= collidingindeces.size())
						selectedwbitem->setIndex(collidingindeces.last() + 1.1, true);
					else
						selectedwbitem->setIndex(collidingindeces.at(n-1) + ((collidingindeces.at(n) - collidingindeces.at(n-1)) / 2), true);
				}
			}
		}
	}
}

void WbScene::bringToFront() {
	bringForward(-1);
}

void WbScene::sendBackwards(int n) {
	if(n == 0)
		return;
	// bring each selected item
	foreach(QGraphicsItem* selecteditem, selectedItems()) {
		if (!selecteditem->parentItem() || !selecteditem->parentItem()->isSelected()) {
			WbItem* selectedwbitem = findWbItem(selecteditem);
			if(selectedwbitem) {
				QList<qreal> collidingindeces;
				qreal selecteditemindex = selectedwbitem->index();
				foreach(QGraphicsItem* collidingitem, selecteditem->collidingItems()) {
					if(!selectedItems().contains(collidingitem)) {
						WbItem* collidingwbitem = findWbItem(collidingitem);
						if(collidingwbitem) {
							qreal collidingitemindex = collidingwbitem->index();
							if(collidingitemindex < selecteditemindex) {
								int i = 0;
								while(i < collidingindeces.size() && collidingindeces.at(i) < collidingitemindex)
									i++;
								collidingindeces.insert(i, collidingitemindex);
							}
						}
					}
				}
				if(!collidingindeces.isEmpty()) {
					if(n < 0 || n >= collidingindeces.size())
						selectedwbitem->setIndex(collidingindeces.first() - 1.1, true);
					else
						selectedwbitem->setIndex(collidingindeces.at(collidingindeces.size() - n) - ((collidingindeces.at(collidingindeces.size() - n) - collidingindeces.at(collidingindeces.size() - n - 1)) / 2), true);
				}
			}
		}
	}
}

void WbScene::sendToBack() {
	sendBackwards(-1);
}

void WbScene::group() {
	if(selectedItems().size() > 1) {
		// Create the group
		QDomElement _svg = QDomDocument().createElement("g");
		WbGroup* group = new WbGroup(_svg, newId(), newIndex(), "root", this);
		addWbItem(group);
		// Send out the group item
		queueNew(group->id(), group->index(), group->svg());
		// Reparent each selected item
		foreach(QGraphicsItem* selecteditem, selectedItems()) {
			WbItem* selectedwbitem = findWbItem(selecteditem);
			if(selectedwbitem)
				selectedwbitem->setParentWbItem(group->id(), true);
		}
		clearSelection();
		group->setSelected(true);
	}
}

void WbScene::ungroup() {
	foreach(QGraphicsItem* selecteditem, selectedItems()) {
		// 87654201 == WbGroup::type()
		if(selecteditem->type() == 87654201) {
			WbItem* selectedwbitem = findWbItem(selecteditem);
			if(selectedwbitem) {
				// If the group has some transformation, the children should be accordingly transformed as the group is removed
				if(!selecteditem->sceneMatrix().isIdentity() || selecteditem->pos() != QPointF()) {
					foreach(QGraphicsItem* child, selecteditem->children()) {
						child->setMatrix(child->sceneMatrix());
						WbItem* childwbitem = findWbItem(child);
						if(childwbitem)
							childwbitem->regenerateTransform();
					}
				}
				queueRemove(selectedwbitem->id());
				removeElement(selectedwbitem->id());
			}
		}
		clearSelection();
	}
}

void WbScene::sendWb() {
	if(queue_.size() < 1)
		return;
	QDomDocument d;
	QDomElement wb = d.createElementNS("http://jabber.org/protocol/svgwb", "wb");
	wb.setAttribute("session", session_);
	WbItem* i;
	while(queue_.size() > 0) {
		// only append the version just before sending the element
		// and not when being queued to avoid unnecessary rejections
		if(queue_.first().type == Edit::AttributeEdit || queue_.first().type == Edit::ParentEdit || queue_.first().type == Edit::ContentEdit) {
			i = findWbItem(queue_.first().target);
			QDomElement configure = d.createElement("configure");
			configure.setAttribute("target", queue_.first().target);
			configure.setAttribute("version", ++i->version);
			// Lump all consequtive configure edits witht the same target into one <configure/>.
			while(queue_.first().type == Edit::AttributeEdit || queue_.first().type == Edit::ParentEdit || queue_.first().type == Edit::ContentEdit && queue_.first().target == configure.attribute("target")) {
				// Store the EditUndo's for each edit
				Edit edit = queue_.takeFirst();
				i->undos.append(EditUndo(i->version, edit));
				configure.appendChild(edit.xml);
				if(queue_.isEmpty())
					break;
			}
			wb.appendChild(configure);
		} else
			wb.appendChild(queue_.takeFirst().xml);
	}
	emit newWb(wb);
}

void WbScene::queueNew(const QString &id, const qreal &index, const QDomElement &svg) {
	QDomDocument d;
	QDomElement n = d.createElement("new");
	n.setAttribute("id", id);
	n.setAttribute("index", index);
	n.appendChild(svg.cloneNode());
	queue_.append(Edit(Edit::NewEdit, n));

	if(index > highestIndex_)
		highestIndex_ = static_cast<int>(index);

	//TODO: detect if more edits coming
	sendWb();
}

void WbScene::queueAttributeEdit(const QString &target, const QString &attribute, const QString &value, QString oldValue, int from, int to) {
	// Remove queued edits that have the same target and attribute if not a partial string replace
	if(from < 0 || to < from)
		removeFromQueue(target, attribute, oldValue);
	QDomDocument d;
	Edit e = Edit(Edit::AttributeEdit, target, d.createElement("attribute"), oldValue);
	e.xml.setAttribute("name", attribute);
	if(from > -1 && to >= from) {
		e.xml.setAttribute("from", from);
		e.xml.setAttribute("to", to);
	}
	e.xml.appendChild(d.createTextNode(value));
	queue_.append(e);

	//TODO: detect if more edits coming
	sendWb();
}

void WbScene::queueParentEdit(const QString &target, const QString &value, QString oldValue) {
	// Remove queued edits that have the same target and attribute
	removeFromQueue(target, oldValue);
	QDomDocument d;
	Edit e = Edit(Edit::ParentEdit, target, d.createElement("parent"), oldValue);
	e.xml.appendChild(d.createTextNode(value));
	queue_.append(e);

	//TODO: detect if more edits coming
	sendWb();
}

void WbScene::queueContentEdit(const QString &target, const QDomNodeList &value, QDomNodeList oldValue) {
	// Remove queued edits that have the same target and attribute
	removeFromQueue(target, oldValue);
	QDomDocument d;
	Edit e = Edit(target, d.createElement("content"), oldValue);
	for(uint j=0; j < value.length(); j++)
		e.xml.appendChild(value.at(j));
	queue_.append(e);

	//TODO: detect if more edits coming
	sendWb();
}

void WbScene::queueMove(const QString &target, qreal di) {
	QDomDocument d;
	QDomElement n = d.createElement("move");
	n.setAttribute("target", target);
	n.setAttribute("di", di);
	queue_.append(Edit(Edit::MoveEdit, n));

	//TODO: detect if more edits coming
	sendWb();
}

void WbScene::queueRemove(const QString &target) {
	QDomDocument d;
	QDomElement n = d.createElement("remove");
	n.setAttribute("target", target);
	queue_.append(Edit(Edit::RemoveEdit, n));

	//TODO: detect if more edits coming
	sendWb();	
}

bool WbScene::setElement(QDomElement &element, const QString &parent, const QString &id, const qreal &index)
{
	if(id.length() < 1)
		return false;
	WbItem* target = findWbItem(id);

	// Get the QGraphicsItem* type of pointer to the parent WbItem
	if(target) {
		target->parseSvg(element, false);
		if(parent != target->parentWbItem())
			target->setParentWbItem(parent);
		if(index != target->index())
			target->setIndex(index);
		return true;
	} else {
		if(element.tagName() == "path")
			target = new WbPath(element, id, index, parent, this);
		else if(element.tagName() == "ellipse")
			target = new WbEllipse(element, id, index, parent, this);
		else if(element.tagName() == "circle")
			target = new WbCircle(element, id, index, parent, this);
		else if(element.tagName() == "rect")
			target = new WbRectangle(element, id, index, parent, this);
		else if(element.tagName() == "line")
			target = new WbLine(element, id, index, parent, this);
		else if(element.tagName() == "polyline")
			target = new WbPolyline(element, id, index, parent, this);
		else if(element.tagName() == "polygon")
			target = new WbPolygon(element, id, index, parent, this);
		else if(element.tagName() == "text")
			target = new WbText(element, id, index, parent, this);
		else if(element.tagName() == "image")
			target = new WbImage(element, id, index, parent, this);
		else if(element.tagName() == "g")
			target = new WbGroup(element, id, index, parent, this);
		else
			target = new WbUnknown(element, id, index, parent, this);
		// Add the element pointer to the hash of elements_
		if(target) {
			addWbItem(target);
			return true;
		}
	}
	return false;
}

bool WbScene::processNew(const QDomElement &New) {
	QDomElement element = New.firstChildElement();
	if(New.hasAttribute("id") && !element.isNull()) {
		QString id = New.attribute("id");
		QString parent = New.attribute("parent");
		if(parent.isEmpty())
			parent = "root";
		qreal index;
		// FIXME: Actually, 'index' is REQUIRED in the JEP, leaving this as a fallback for now
		if(New.hasAttribute("index"))
			index = New.attribute("index").toDouble();
		else
			index = id.left(id.indexOf("/")).toDouble();
		// Check that element with 'id' doesn't exist yet.
		if(!findWbItem(id)) {
// 			qDebug() << QString("Adding - id: %1, index: %2").arg(id).arg(index).toAscii();
			if(setElement(element, parent, id, index)) {
				// save the highest index and id if appropriate
				if(index > highestIndex_)
					highestIndex_ = static_cast<int>(index);
				if(id.left(id.indexOf("/")).toInt() > highestId_)
					highestId_ = id.left(id.indexOf("/")).toInt();
				return true;
			}
		}
	}
	return false;
}

bool WbScene::processConfigure(const QDomElement &configure) {
	WbItem* wbitem = findWbItem(configure.attribute("target"));
	if(wbitem) {
		// Note that _svg->svg() returns a deep copy
		QDomElement _svg = wbitem->svg();
		if(importing)
			wbitem->version = configure.attribute("version").toInt();
		else
 			wbitem->version++;
		if(configure.attribute("version").toInt() == wbitem->version) {
// 			qDebug) << (QString("Applying <configure/> - configure version: %1, current version: %2 target: %3 attribute: %4 value: %5").arg(configure.attribute("version")).arg(wbitem->version-1).arg(configure.attribute("target")).arg(configure.attribute("attribute")).arg(configure.attribute("value")).toAscii());
			QDomNodeList configureChildren = configure.childNodes();
			QString newParent = wbitem->parentWbItem();
			for(uint i = 0; i < configureChildren.length(); i++) {
				if(configureChildren.at(i).isElement()) {
					QDomElement edit = configureChildren.at(i).toElement();
					if(edit.nodeName() == "attribute" && !edit.attribute("name").isEmpty()) {
						QString attributeName = edit.attribute("name");
						QString oldValue = _svg.attribute(edit.attribute("name"));
						// Remove queued <configure>s that had the same target and attribute and retrieve the correct oldvalue
						removeFromQueue(configure.attribute("target"), attributeName, oldValue);
						wbitem->undos.append(EditUndo(configure.attribute("version").toInt(), attributeName, oldValue));
						QString newValue;
						if(!edit.attribute("from").isEmpty() && !edit.attribute("to").isEmpty()) {
							bool okF, okT;
							int from = edit.attribute("from").toInt(&okF);
							int to = edit.attribute("to").toInt(&okT);
							if(okF && okT && from <= to) {
								newValue = oldValue;
								newValue.replace(from, to - from, edit.text());
							} else
								newValue = edit.text();
						} else
							newValue = edit.text();
						_svg.setAttribute(attributeName, newValue);
					} else if(edit.nodeName() == "content") {
						QDomNodeList oldContent = _svg.cloneNode().childNodes();
						// Remove queued <configure>s that had the same target and attribute and retrieve the correct oldvalue
						removeFromQueue(configure.attribute("target"), oldContent);
						wbitem->undos.append(EditUndo(configure.attribute("version").toInt(), oldContent));
						while(_svg.hasChildNodes())
							_svg.removeChild(_svg.firstChild());
						for(uint j=0; j < edit.childNodes().length(); j++)
							_svg.appendChild(edit.childNodes().at(j));
					} else if(edit.nodeName() == "parent") {
						QString oldParent = newParent;
						// Remove queued <configure>s that had the same target and attribute and retrieve the correct oldvalue
						removeFromQueue(configure.attribute("target"), oldParent);
						wbitem->undos.append(EditUndo(configure.attribute("version").toInt(), oldParent));
						newParent = edit.text();
					}
				}
			}
			if(setElement(_svg, newParent, wbitem->id(), wbitem->index())) {
				return true;
			} else {
				wbitem->undos.removeLast();
// 				wbitem->version--;
				return false;
			}
		} else if (configure.attribute("version").toInt() < wbitem->version) {
// 			qDebug() << (QString("Reverting <configure/>'s - configure version: %1, current version: %2 target: %3 attribute: %4 value: %5").arg(configure.attribute("version")).arg(wbitem->version-1).arg(configure.attribute("target")).arg(configure.attribute("attribute")).arg(configure.attribute("value")).toAscii());
			// Revert the changes down to version of the configure - 1.
			if(!wbitem->undos.isEmpty()) {
				QString oldParent = wbitem->parentWbItem();
				while(configure.attribute("version").toInt() <= wbitem->undos.last().version) {
					EditUndo u = wbitem->undos.takeLast();
// 					qDebug() << (QString("setting %1 to %2").arg(u.attribute).arg(u.oldValue).toAscii());
					if(u.type == Edit::AttributeEdit) {
						if(u.oldValue.isNull())
							_svg.removeAttribute(u.attribute);
						else
							_svg.setAttribute(u.attribute, u.oldValue);
					} else if(u.type == Edit::ContentEdit) {
						while(_svg.hasChildNodes())
							_svg.removeChild(_svg.firstChild());
						for(uint j=0; j < u.oldContent.length(); j++)
							_svg.appendChild(u.oldContent.at(j));
					} else if(u.type == Edit::ParentEdit) {
						oldParent = u.oldParent;
					}
					if(wbitem->undos.isEmpty())
						break;
				}
				if(setElement(_svg, oldParent, wbitem->id(), wbitem->index()))
					return true;
				else {
					// Freak out! Should never happen.
// 					wbitem->version--;
					qCritical(QString("Couldn't revert wbitem with id '%1' back to version '%2'").arg(configure.attribute("target")).arg(configure.attribute("version")).toAscii());
					return false;
				}
			}
			// The configure was processed succesfully even though no action was taken.
			// This may happen if the wbitem was already reverted by another configure.
			return true;
		} else if (configure.attribute("version").toInt() > wbitem->version) {
			// This should never happen given the seriality condition.
			// Reason to worry about misfunction of infrastructure but not much to do from here.
			qWarning(QString("Configure to wbitem '%1' version '%1' arrived when the wbitem had version '%3'.").arg(configure.attribute("target")).arg(configure.attribute("version")).arg(wbitem->version-1).toAscii());
			wbitem->version--;
			return false;
		}
	}
	return false;
}

bool WbScene::processMove(const QDomElement &move) {
	WbItem* wbitem = findWbItem(move.attribute("target"));
	if(wbitem) {
// 		qDebug() << (QString("Moving - target: %1 index: %2").arg(move.attribute("target")).arg(move.attribute("di")).toAscii());
		wbitem->setIndex(wbitem->index() + move.attribute("di").toDouble());
		if(wbitem->index() > highestIndex_)
			highestIndex_ = static_cast<int>(wbitem->index());
		return true;
	}
	return false;
}

bool WbScene::processRemove(const QDomElement &remove) {
	if(findWbItem(remove.attribute("target"))) {
// 		qDebug() << (QString("Removing - target: %1").arg(remove.attribute("target")).toAscii());
		if(removeElement(remove.attribute("target"))) {
			return true;
		}
	}
	return false;
}

void WbScene::removeFromQueue(const QString &target, const QString &attribute, QString &oldValue) {
	// remove queued edits that have the same target and attribute
	for(int i = queue_.size() - 1; i >= 0; i--) {
		if(queue_.at(i).type == Edit::AttributeEdit && queue_.at(i).target == target && queue_.at(i).xml.attribute("name") == attribute) {
			// Set the "older" old value since that's the correct one to undo if necessary;
			oldValue = queue_.at(i).oldValue;
			queue_.removeAt(i);
			// We can return because there can never be two that match
			return;
		}
	}
}

void WbScene::removeFromQueue(const QString &target, QString &oldValue) {
	// remove queued edits that have the same target and set the parent
	for(int i = queue_.size() - 1; i >= 0; i--) {
		if(queue_.at(i).type == Edit::ParentEdit && queue_.at(i).target == target) {
			// Set the "older" old parent since that's the correct one to undo if necessary;
			oldValue = queue_.at(i).oldParent;
			queue_.removeAt(i);
			// We can return because there can never be two that match
			return;
		}
	}
}

void WbScene::removeFromQueue(const QString &target, QDomNodeList &oldValue) {
	// remove queued edits that have the same target and set the content
	for(int i = queue_.size() - 1; i >= 0; i--) {
		if(queue_.at(i).type == Edit::ContentEdit && queue_.at(i).target == target) {
			// Set the "older" old content since that's the correct one to undo if necessary;
			oldValue = queue_.at(i).oldContent;
			queue_.removeAt(i);
			// We can return because there can never be two that match
			return;
		}
	}
}

QString WbScene::newId() {
	return QString("%1/%2").arg(++highestId_).arg(ownJid_);
}

qreal WbScene::newIndex() {
	return ++highestIndex_;
}
