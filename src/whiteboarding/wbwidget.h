/*
 * wbwidget.h - a widget for processing and showing whiteboard
 *              messages.
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

#ifndef WBWIDGET_H
#define WBWIDGET_H

#include "../sxe/sxesession.h"
#include "wbscene.h"
#include "wbitem.h"
#include "wbnewitem.h"

#include <QSvgRenderer>
#include <QWidget>
#include <QGraphicsView>
#include <QTimer>
#include <QTime>
#include <QFileDialog>


/*! \brief The whiteboard widget.
 *  Visualizes the whiteboard scene and provides different modes for editing and adding new elements.
 *  Local edits to existing elements are handled by the elements themselves
 *  as mouse events are passed on to them but new items are temporarily
 *  added to the scene while being drawn and only when finished, added to
 *  the WbScene.
 *
 *  \sa WbScene
 *  \sa WbDlg
 */
class WbWidget : public QGraphicsView
{
	Q_OBJECT
public:
	/*! \brief Mode
	 *  Indicates the mode the widget is in.
	 */
	enum Mode { Select, Translate, Rotate, Scale, Scroll, Erase, DrawPath, DrawLine, DrawRectangle, DrawEllipse, DrawCircle, DrawPolyline, DrawText, DrawImage };

	/*! \brief Constructor
	 *  Constructs a new widget with \a session and parent \a parent.
	 */
	WbWidget(SxeSession* session, QWidget* parent = 0);
	/*! \brief Returns the session this widget is visualizing.*/
	SxeSession* session();
	/*! \brief Returns the mode this widget is in.*/
	Mode mode();
	/*! \brief Sets the mode which determines how to react to user interaction.*/
	void setMode(Mode mode);
	/*! \brief Sets the size of the whiteboard.*/
	void setSize(const QSize &s);
	/*! \brief Sets the stroke color of new items.*/
	void setStrokeColor(const QColor &color);
	/*! \brief Sets the stroke color of new items.*/
	void setFillColor(const QColor &color);
	/*! \brief Sets the stroke width of new items.*/
	void setStrokeWidth(int width);

	/*! \brief Returns the size set by setSize().*/
	virtual QSize sizeHint() const;

public slots:
	/*! \brief Clears the whiteboard. */
	void clear();

protected:
	/*! \brief Makes sure that area outside the whiteboard is not shown by zooming if necessary.*/
	virtual void resizeEvent(QResizeEvent * event);
	/*! \brief Passes events to items as specified by the mode.*/
	virtual void mousePressEvent(QMouseEvent * event);
	/*! \brief Passes events to items as specified by the mode.*/
	virtual void mouseMoveEvent(QMouseEvent * event);
	/*! \brief Passes events to items as specified by the mode.*/
	virtual void mouseReleaseEvent(QMouseEvent * event);

private:
	/*! \brief Returns the item representing the node (if any).*/
	WbItem* wbItem(const QDomNode &node);

	/*! \brief The SxeSession synchronizing the document.*/
	SxeSession* session_;
	/*! \brief The WbScene used for visualizing the document.*/
	WbScene* scene_;
	/*! \brief The user interaction mode the widget is in.*/
	Mode mode_;
	/*! \brief The stroke color used for new items.*/
	QColor strokeColor_;
	/*! \brief The fill color used for new items.*/
	QColor fillColor_;
	/*! \brief The stroke width used for new items.*/
	int strokeWidth_;

	/*! \brief A list of existing WbItems */
    QList<WbItem*> items_;
    // /*! \brief A list of WbItems to be deleted. */
    //     QList<WbItem*> deletionQueue_;
	/*! \brief A list of QDomNode's that were added since last documentUpdated() signal received. */
    QList<QDomNode> recentlyRelocatedNodes_;
	/*! \brief A list of WbItem's whose nodes don't have 'id' attributes. */
    QList<WbItem*> idlessItems_;
	/*! \brief Pointer to a new item that is being drawn.*/
    WbNewItem* newWbItem_;
	/*! \brief Boolean used to force adding a vertex to a path being drawn.*/
	bool addVertex_;
	/*! \brief Timer used for forcing the addition of a new vertex.*/
	QTimer* adding_;
	/*! \brief The primary renderer used for rendering the document.*/
    QSvgRenderer renderer_;

private slots:
	/*! \brief Tries to add 'id' attributes to nodes in deletionQueue_ if they still don't have them.*/
    void handleDocumentUpdated(bool remote);
	/*! \brief Ensures that an item for the nodes in the inspection queue exist
	 *      iff they're children of the root <svg/>.*/
	void inspectNodes();
	/*! \brief Adds a node to the list of nodes that will be processed by inspectNodes() at next documentUpdated().*/
	void queueNodeInspection(const QDomNode &node);
	/*! \brief Removes the item representing the node (if any).
	 *  Doesn't affect \a node.
	 */
	void removeWbItem(const QDomNode &node);
	/*! \brief Removes the item from the scene.
     *  Doesn't affect the underlying node.
     */
	void removeWbItem(WbItem *wbitem);
	/*! \brief Tries to add 'id' attributes to nodes in idlessItems_ if they still don't have them.*/
	void addIds();
	/*! \brief Adds the item associated with node to idlessItems_. */
	void addToIdLess(const QDomElement &element);
	/*! \brief If node is an 'id' attribute node, adds the ownerElement to idlessItems_. */
    void checkForRemovalOfId(QDomNode node);
    /*! \brief Checks if \a node is the 'viewBox' attribute of the <svg/> element.
     *  If so, the scene size is adjusted accordingly.
     */
    void checkForViewBoxChange(const QDomNode &node);
    // /*! \brief Deletes the WbItem's in the deletion queue. */
    // void flushDeletionQueue();


	/*! \brief Rerenders the contents of the document.*/
	void rerender();
};

#endif
