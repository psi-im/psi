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
#include "wbscene.h"
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
	 *  Constructs a new widget with \a session and \a size with parent \a parent.
	 *  \a ownJid is the Jid that should be used in item IDs created by this widget.
	 */
	WbWidget(const QString &session, const QString &ownJid, const QSize &size, QWidget *parent=0);
	/*! \brief Processes an incoming whiteboard element.*/
	bool processWb(const QDomElement &wb);
	/*! \brief Returns the session this widget is visualizing.*/
	QString session();
	/*! \brief Returns the JID used by the user in the session.*/
	const QString & ownJid() const;
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
	/*! \brief Sets whether configure edits are accepted regardless of version.
	 *  Default is false. Should be set true if the session state is Negotiation::DocumentBegun.
	 */
	void setImporting(bool);

	/*! \brief Returns the size set by setSize().*/
	virtual QSize sizeHint() const;

	/*! \brief A pointer to the WbScene.*/
	WbScene* scene;

signals:
	/*! \brief Emitted when a new whiteboard element is ready to be sent.*/
	void newWb(const QDomElement &wb);

public slots:
	/*! \brief Clears the whiteboard.
	 *  If \a sendEdits is true, the remove edits are sent.
	 */
	void clear(const bool &sendEdits = true);

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
	/*! \brief The user interaction mode the widget is in.*/
	Mode mode_;
	/*! \brief The stroke color used for new items.*/
	QColor strokeColor_;
	/*! \brief The fill color used for new items.*/
	QColor fillColor_;
	/*! \brief The stroke width used for new items.*/
	int strokeWidth_;

	/*! \brief Pointer to a new item that is being drawn.*/
	WbItem* newWbItem_;
	/*! \brief Boolean used to force adding a vertex to a path being drawn.*/
	bool addVertex_;
	/*! \brief Pointer to a control point to be used for the next vertex.*/
	QPointF* controlPoint_;
	/*! \brief Timer used for forcing the addition of a new vertex.*/
	QTimer* adding_;

private slots:
	/*! \brief Slot used for forcing the addition of a new vertex.*/
	void addVertex();
};

#endif
