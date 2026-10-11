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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "wbscene.h"

WbScene::WbScene(XMPP::Jingle::SXE::Document *document, QObject *parent) : QGraphicsScene(parent)
{
    document_ = document;
};

void WbScene::queueTransformationRegeneration(WbItem *item)
{
    if (item && !pendingTranformations_.contains(item))
        pendingTranformations_.append(QPointer<WbItem>(item));
}

void WbScene::regenerateTransformations()
{
    for (QPointer<WbItem> item : std::as_const(pendingTranformations_)) {
        if (item) {
            // qDebug() << QString("Regenerating %1 transform.").arg((unsigned int) &(*item)).toLatin1();
            item->regenerateTransform();
        }
    }
    pendingTranformations_.clear();
    document_->flush();
}

QPointF WbScene::selectionCenter() const
{
    QList<QGraphicsItem *> items = selectedItems();

    if (items.size() < 1) {
        return QPointF();
    }

    QRectF box = items.at(0)->sceneBoundingRect();
    for (QGraphicsItem *item : std::as_const(items)) {
        box = box.united(item->sceneBoundingRect());
    }

    // qDebug() << QString("Selection center: %1 %2").arg(box.center().x()).arg(box.center().y()).toLatin1();

    return box.center();
}

void WbScene::bringForward(int n) { bring(n, false); }

void WbScene::bringToFront() { bring(1, true); }

void WbScene::sendBackwards(int n) { bring(-n, false); }

void WbScene::sendToBack() { bring(-1, true); }

static bool zValueLessThan(QGraphicsItem *item1, QGraphicsItem *item2) { return (item1->zValue() < item2->zValue()); }

void WbScene::group()
{
    if (selectedItems().size() > 1) {
        // Create the group
        QDomElement temp = QDomDocument().createElement("g");
        temp.setAttribute("id", "e" + XMPP::Jingle::SXE::Document::generateUUID());
        const QDomNode group = document_->insertNodeAfter(temp, document_->document().documentElement());

        // Arrange the items by Z coordinate
        QList<QGraphicsItem *> selected = selectedItems();
        std::sort(selected.begin(), selected.end(), zValueLessThan);

        // Reparent each selected item
        for (QGraphicsItem *item : std::as_const(selected)) {
            WbItem *wbitem = dynamic_cast<WbItem *>(item);
            if (wbitem)
                document_->insertNodeAfter(wbitem->node(), group);
        }
        clearSelection();

        document_->flush();
    }
}

void WbScene::ungroup()
{
    const auto &gItems = selectedItems();
    for (QGraphicsItem *item : gItems) {
        // find the QDomElement matching the selected item
        WbItem *wbitem = dynamic_cast<WbItem *>(item);
        if (wbitem) {
            QDomElement group = wbitem->node().toElement();

            if (group.nodeName() == "g") {
                QDomNodeList children = group.childNodes();

                QTransform groupTransform = WbItem::parseSvgTransform(group.attribute("transform"));

                for (int i = children.size() - 1; i >= 0; i--) {
                    QDomElement child = children.at(i).toElement();
                    if (!child.isNull()) {

                        if (!groupTransform.isIdentity()) {
                            // combine the transformations of the group and the child
                            QTransform childTransform = WbItem::parseSvgTransform(child.attribute("transform"));
                            document_->setAttribute(child, "transform",
                                                    WbItem::toSvgTransform(childTransform * groupTransform));
                        }

                        // move the child from the group to the root <svg/>
                        document_->insertNodeAfter(children.at(i), document_->document().documentElement(), group);
                    }
                }

                // delete the group itself
                document_->removeNode(group);
            }
        }
    }
    clearSelection();

    document_->flush();
}

void WbScene::bring(int n, bool toExtremum)
{
    if (n == 0)
        return;

    // bring each selected item
    const auto &gItems = selectedItems();
    for (QGraphicsItem *selecteditem : gItems) {

        if (!(selecteditem->parentItem() && selecteditem->parentItem()->isSelected())) {

            WbItem *selectedwbitem = dynamic_cast<WbItem *>(selecteditem);

            if (selectedwbitem) {

                QList<QGraphicsItem *> colliding = selecteditem->collidingItems();

                // find the relative position of selecteditem itself and
                // remove other selected items from the list colliding
                int i = 0;
                while (i < colliding.size()) {
                    if (selectedItems().contains(colliding[i])) {
                        colliding.removeAt(i);
                    } else if (colliding[i]->zValue() < selecteditem->zValue()) {
                        break;
                    } else
                        i++;
                }
                if (n < 0)
                    i--;

                // bring the selected node n levels above/below the item itself
                if (colliding.size() > 0) {
                    WbItem *referencewbitem;
                    if (n > 0) {
                        if (i - n > 0 && !toExtremum)
                            referencewbitem = dynamic_cast<WbItem *>(colliding[i - n]);
                        else
                            referencewbitem = dynamic_cast<WbItem *>(colliding[0]);
                    } else {
                        if (i - n < colliding.size() && !toExtremum)
                            referencewbitem = dynamic_cast<WbItem *>(colliding[i - n]);
                        else
                            referencewbitem = dynamic_cast<WbItem *>(colliding[colliding.size() - 1]);
                    }

                    if (referencewbitem && !referencewbitem->node().isNull()) {
                        if (n > 0)
                            document_->insertNodeAfter(selectedwbitem->node(), selectedwbitem->node().parentNode(),
                                                       referencewbitem->node());
                        else
                            document_->insertNodeBefore(selectedwbitem->node(), selectedwbitem->node().parentNode(),
                                                        referencewbitem->node());
                    }
                }
            }
        }
    }

    document_->flush();
}
