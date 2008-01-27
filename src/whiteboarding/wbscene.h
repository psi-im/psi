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
#include "wbitem.h"

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
	WbScene(SxeSession* session, QObject * parent = 0);

	/*! \brief Appends the item to a list of items whose "transform" attribute is to be regenerated.*/
	void queueTransformationRegeneration(WbItem* item);
	/*! \brief Regenerate the SVG transformation matrices for items queued by queueTransformationRegeneration(WbItem* item) since last regeneration.*/
	void regenerateTransformations();
	/*! \brief Returns the coordinates of the center of all selected items. */
    QPointF selectionCenter() const;

public slots:
    /*! \brief Brings the selected items \a n levels forward. */
    void bringForward(int n = 1);
    /*! \brief Brings the item to the top. */
    void bringToFront();
    /*! \brief Sends the selected items \a n levels backwards. */
    void sendBackwards(int n = 1);
    /*! \brief Brings the item to the back. */
    void sendToBack();
    /*! \brief Groups the selected items.*/
    void group();
    /*! \brief Ungroups the selected item groups.*/
    void ungroup();

private:
    /*! \brief Brings the selected items \a n levels forward.
    *  If \a n < 0, the items are send \a n levels baskwards.
    *  If \a toExtremum is true, the item is sent to the front/back.
    */
    void bring(int n, bool toExtremum);

    SxeSession* session_;
    QList< QPointer<WbItem> > pendingTranformations_;
    
    QPointF selectionCenter_;
};

#endif

