/*
 * wbscene.h - an SVG whiteboard scene class
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
#ifndef WBSCENE_H
#define WBSCENE_H
#include <QPointer>
#include "wbitems.h"

/*! \brief The scene class for whiteboard items.
 *  Inherits QGraphicsScene.
 *
 *  Processes remote whiteboard edits.
 *
 *  This class also provides methods for queueing new edits and
 *  emits a signal with the corresponding whiteboard element.
 *
 *  \sa WbWidget
 *  \sa WbItem
 */
class WbScene : public QGraphicsScene
{
	Q_OBJECT

public:
	/*! \brief Constructor
	 *  Constructs a new scene with parent \a parent.
	 */
	WbScene(const QString &session, const QString &ownJid, QObject * parent = 0);
	/*! \brief Returns the session this widget is visualizing.*/
	QString session();
	/*! \brief Returns the JID used by the user in the session.*/
	const QString & ownJid() const;
	/*! \brief Processes an incoming whiteboard element.*/
	bool processWb(const QDomElement &wb);
	/*! \brief Returns a pointer to the item with ID \a id*/
	WbItem* findWbItem(const QString &id) const;
	/*! \brief Returns a pointer to the item with graphicsitem() \a item*/
	WbItem* findWbItem(QGraphicsItem* item) const;
	/*! \brief Returns the SVG element representing the item with ID \a id*/
	QDomElement element(const QString &id) const;
	/*! \brief Returns the a list of all the elements on the whiteboard*/
	QList<WbItem*> elements() const;

	/*! \brief Adds the given item to scene.
	 *  Connects the necessary signals so all items added to the board must be passed to this method.
	 */
	void addWbItem(WbItem *);
	/*! \brief Removes the element with ID \a id.*/
	bool removeElement(const QString &id, bool emitChanges = false);

	/*! \brief Return a new unique ID.*/
	QString newId();
	/*! \brief Return a new index value that will be the highest in the whiteboard.*/
	qreal newIndex();

	/*! \brief Appends the given item on the pendigTransformations_ list.*/
	void queueTransformationChange(WbItem* item);
	/*! \brief Regenerate and signal the SVG transformation matrices for items on pendigTransformations_ list.*/
	void regenerateTransformations();

	/*! \brief If set true, all configure edits are accepted regardless of version.
	 *  Default is false. Should be set true if the session state is Negotiation::DocumentBegun.
	 */
	bool importing;

signals:
	/*! \brief Emitted when a new whiteboard element is ready to be sent.*/
	void newWb(const QDomElement &wb);

public slots:
	/*! \brief Popsup a color dialog and sets color of the selected items.*/
	void setStrokeColor();
	/*! \brief Popsup a color dialog and sets fill color of the selected items.*/
	void setFillColor();
	/*! \brief Overload: Sets the width of selected items based on the QAction->data().*/
	void setStrokeWidth(QAction*);
	/*! \brief Brings the selected items \a n levels forward.
	 *  If n < 0, the items are brought to the front.
	 */
	void bringForward(int n = 1);
	/*! \brief Same as bringForward(-1).*/
	void bringToFront();
	/*! \brief Sends the selected items \a n levels backwards.
	 *  If n < 0, the items are sent to the back.
	 */
	void sendBackwards(int n = 1);
	/*! \brief Same as bringsendBackwardsForward(-1).*/
	void sendToBack();
	/*! \brief Groups the selected items.*/
	void group();
	/*! \brief Ungroups the selected item groups.*/
	void ungroup();

	/*! \brief Composes queued edits and emits a new whiteboard element.*/
	void sendWb();
	/*! \brief Queue a new element edit.*/
	void queueNew(const QString &id, const qreal &index, const QDomElement &svg);
	/*! \brief Queue an attribute edit.*/
	void queueAttributeEdit(const QString &target, const QString &attribute, const QString &value, QString oldValue, int from = -1, int to = -1);
	/*! \brief Queue a parent edit.*/
	void queueParentEdit(const QString &target, const QString &value, QString oldValue);
	/*! \brief Queue a content edit.*/
	void queueContentEdit(const QString &target, const QDomNodeList &value, QDomNodeList oldValue);
	/*! \brief Queue a move edit.*/
	void queueMove(const QString &target, qreal di);
	/*! \brief Queue a remove edit.*/
	void queueRemove(const QString &target);

private:
	/*! \brief Adds or replaces specified element with the given SVG element.*/
	bool setElement(QDomElement &element, const QString &parent, const QString &id, const qreal &index);

	/*! \brief Processes an incoming new element edit.*/
	bool processNew(const QDomElement &New);
	/*! \brief Processes an incoming configure edit.*/
	bool processConfigure(const QDomElement &configure);
	/*! \brief Processes an incoming move edit.*/
	bool processMove(const QDomElement &move);
	/*! \brief Processes an incoming remove edit.*/
	bool processRemove(const QDomElement &remove);

	/*! \brief Removes specified edits from the queue.
	 *  Removes queued edits that have the specified \a target and \a attribute.
	 *  Sets the \a oldValue to the value at the time of last sent or received edit.
	 */
	void removeFromQueue(const QString &target, const QString &attribute, QString &oldValue);
	/*! \brief Removes specified edits from the queue.
	 *  Removes queued edits that have the specified \a target and set a new parent.
	 *  Sets the \a oldParent to the value at the time of last sent or received edit.
	 */
	void removeFromQueue(const QString &target, QString &oldParent);
	/*! \brief Removes specified edits from the queue.
	 *  Removes queued edits that have the specified \a target and set content.
	 *  Sets the \a oldContent to the value at the time of last sent or received edit.
	 */
	void removeFromQueue(const QString &target, QDomNodeList &oldContent);

	/*! \brief The whiteboard session being visualized.*/
	QString session_;
	/*! \brief The JID used in creation of IDs.*/
	QString ownJid_;
	/*! \brief The highest index found in the whiteboard (with decimals cropped).*/
	int highestIndex_;
	/*! \brief The highest primary part of and ID found in the whiteboard.*/
	int highestId_;

	/*! \brief The list containing the queued edits.*/
	QList<Edit> queue_;

	/*! \brief A hash of element IDs and pointers to them.*/
	QHash<QString, WbItem*> elements_;

	/*! \brief A list of items whose transformation matrix has been changed and are waiting for the corresponding edit to be sent.*/
	QList< QPointer<WbItem> > pendingTranformations_;
};

#endif

