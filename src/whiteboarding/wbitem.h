/*
 * wbitem.h - the item classes for the SVG WB
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

#ifndef WBITEM_H
#define WBITEM_H

#include <QGraphicsSvgItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include "iconaction.h"
#include "../sxe/sxesession.h"

class WbScene;
class WbWidget;

/*! \brief The context menu for WbItems
 *  This menu pops up when as the context menu for WbItems and displays
 *  \sa WbItem
 */
class WbItemMenu : public QMenu {
	Q_OBJECT
public:
	/*! \brief Constructor
	 *  Constructs a new empty menu.
	 */
	WbItemMenu(QWidget* parent);
	/*! \brief Destructor
	 *  Deletes all the actiongroups and their actions.
	 */
	~WbItemMenu();
	/*! \brief Add actiongroup to the menu.*/
	void addActionGroup(QActionGroup*);

private slots:
	/*! \brief Destruct self.*/
	void destructSelf();

private:
	/*! \brief The actiongroups that the menu shows.*/
	QList<QActionGroup*> groups_;
};

class WbItem : public QGraphicsSvgItem {
	Q_OBJECT
public:
	/*! \brief Constructor
	 *  Constructs a new whiteboard item that visualized \a node.
	 */
	WbItem(SxeSession* session, QSvgRenderer* renderer, QDomElement node, WbScene* scene, WbWidget* widget);
	/*! \brief Destructor
	 *  Makes sure that the item gets deleted from the underlying <svg/> document
	 */
	~WbItem();

	/*! \brief Returns the xml:id of the node.*/
	QString id();
	/*! \brief Returns the underlying node.*/
	QDomNode node();

	/*! \brief Regenerates the SVG transformation matrix.*/
	void regenerateTransform();

	/*! \brief Adds the item to the scene.
		Generates an 'id' attribute for the node if none exists. */
	void addToScene();
	/*! \brief Removes the item from the scene. */
	void removeFromScene();

	/*! \brief Resets the position of the item according to the SVG and clears any QGraphicsItem transformations.*/
	void resetPos();

	/*! \brief Returns a QTransform based on \a string provided in the SVG 'transform' attribute format.*/
	static QMatrix parseSvgTransform(QString string);
	/*! \brief Returns a QString in the SVG 'transform' attribute format based on \a matrix.*/
	static QString toSvgTransform(const QMatrix &matrix);

protected:
	 /*! \brief Constructs and popsup the default context menu.*/
	 virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	 /*! \brief Implements the default item interaction behaviour.
	 *  The action depends on the mode of widget_;
	  */
	 virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	 /*! \brief Implements the default item interaction behaviour.
	  *  The action depends on the mode of widget_;
	  */
	 virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);

private:
	 /*! \brief Construct a context menu with the default items.*/
	 WbItemMenu* constructContextMenu();
	 /*! \brief Return the center of the item in item coordinates.*/
	 QPointF center();

	// The session that the item belongs to
	SxeSession* session_;
	// The WbScene that the item belongs to
	WbScene* scene_;
	// The WbWidget that shows the item
	WbWidget* widget_;
	// The node SVG node that's being visualized
	QDomElement node_;

};

#endif
