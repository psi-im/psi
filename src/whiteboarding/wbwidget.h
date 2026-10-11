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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WBWIDGET_H
#define WBWIDGET_H

#include "wbitem.h"
#include "wbnewitem.h"
#include "wbscene.h"
#include <iris/xmpp-im/jingle-sxe-document.h>

#include <QFileDialog>
#include <QGraphicsView>
#include <QPointer>
#include <QRubberBand>
#include <QSvgRenderer>
#include <QTime>
#include <QTimer>
#include <QWidget>

/*! \brief The whiteboard widget.
 *  Visualizes the whiteboard scene. The current tool (mode) acts on the left
 *  button; selecting, moving, scaling, rotating, zooming and panning are
 *  temporary gestures with modifiers, other buttons and the keyboard, so they
 *  don't need a mode switch. New items are temporarily added to the scene
 *  while being drawn and only when finished, added to the document.
 *
 *  \sa WbScene
 *  \sa WbView
 */
class WbWidget : public QGraphicsView {
    Q_OBJECT
public:
    /*! \brief Mode
     *  Indicates the mode the widget is in.
     */
    enum class Mode : char {
        Select,
        Erase,
        DrawPath,
        DrawLine,
        DrawRectangle,
        DrawEllipse,
        DrawRhombus,
        DrawText,
        DrawImage
    };

    /*! \brief Constructor
     *  Constructs a new widget with \a session and parent \a parent.
     */
    WbWidget(XMPP::Jingle::SXE::Document *document, QWidget *parent = nullptr);
    /*! \brief Returns the shared document this widget is visualizing.*/
    XMPP::Jingle::SXE::Document *document();
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
    /*! \brief Allows or forbids changing the document. Viewing (zoom, pan) stays possible.*/
    void setEditable(bool editable);
    /*! \brief Fits the whole canvas into the view.*/
    void fitToView();

    /*! \brief Returns the size set by setSize().*/
    virtual QSize sizeHint() const;

public slots:
    /*! \brief Clears the whiteboard. */
    void clear();

signals:
    /*! \brief The tool was changed from the keyboard.*/
    void modeChanged(WbWidget::Mode mode);
    /*! \brief The stroke width was changed from the keyboard.*/
    void strokeWidthChanged(int width);
    /*! \brief A context menu was requested over an empty part of the canvas.*/
    void canvasContextMenuRequested(const QPoint &globalPos);

protected:
    /*! \brief Keeps the canvas fitted into the view unless the user zoomed in.*/
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    /*! \brief Handles touchpad pinch zoom.*/
    bool viewportEvent(QEvent *event) override;
    /*! \brief Shows the canvas as a white sheet on a gray background.*/
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    /*! \brief A temporary interaction started by a mouse press.*/
    enum class Gesture { None, Draw, Erase, Move, Scale, Rotate, RubberBand, Pan, ContextPending, TogglePending };

    /*! \brief Returns the topmost whiteboard item at \a viewPos.*/
    WbItem *wbItemAt(const QPoint &viewPos) const;
    /*! \brief Returns the selected whiteboard items.*/
    QList<WbItem *> selectedWbItems() const;
    /*! \brief Moves the selection by the difference of two scene points.*/
    void translateSelection(const QPointF &from, const QPointF &to);
    /*! \brief Rotates the selection by \a degrees around \a pivot (scene coordinates).*/
    void rotateSelection(qreal degrees, const QPointF &pivot);
    /*! \brief Scales the selection by \a factor around \a pivot (scene coordinates).*/
    void scaleSelection(qreal factor, const QPointF &pivot);
    /*! \brief Writes pending item transformations to the document.*/
    void commitTransformations();
    void deleteSelection();
    void duplicateSelection();
    void selectAll();
    /*! \brief Removes the items under \a viewPos.*/
    void eraseAt(const QPoint &viewPos);
    /*! \brief Zooms the view by \a factor. Zooming out never goes beyond the fitted canvas.*/
    void zoomBy(qreal factor, bool underMouse = true);
    /*! \brief Inserts an image at \a scenePos. Large images are scaled down.*/
    void insertImage(const QImage &image, const QPointF &scenePos);
    void insertImageFile(const QString &fileName, const QPointF &scenePos);
    /*! \brief Asks for a text and inserts it at \a scenePos as outlines (the profile represents text as paths).*/
    void insertText(const QPointF &scenePos);
    void setModeFromKeyboard(Mode mode);
    void updateCursor();

    /*! \brief True if the document may be changed.*/
    bool editable_ = true;
    /*! \brief True while the whole canvas is fitted into the view (no user zoom).*/
    bool fitToView_ = true;
    /*! \brief True while Space is held for panning.*/
    bool spaceHeld_ = false;
    Gesture         gesture_       = Gesture::None;
    Qt::MouseButton gestureButton_ = Qt::NoButton;
    QPoint          pressPos_;
    QPoint          lastViewPos_;
    QPointF         lastScenePos_;
    QPointF         pivot_;
    QPointer<WbItem> pressItem_;
    QRubberBand     *rubberBand_ = nullptr;

    /*! \brief Returns the item representing the node (if any).*/
    WbItem *wbItem(const QDomNode &node);

    /*! \brief The SXE document synchronized with the peer.*/
    XMPP::Jingle::SXE::Document *document_;
    /*! \brief The WbScene used for visualizing the document.*/
    WbScene *scene_;
    /*! \brief The user interaction mode the widget is in.*/
    Mode mode_;
    /*! \brief The stroke color used for new items.*/
    QColor strokeColor_;
    /*! \brief The fill color used for new items.*/
    QColor fillColor_;
    /*! \brief The stroke width used for new items.*/
    int strokeWidth_;

    /*! \brief A list of existing WbItems */
    QList<WbItem *> items_;
    // /*! \brief A list of WbItems to be deleted. */
    //     QList<WbItem*> deletionQueue_;
    /*! \brief A list of QDomNode's that were added since last documentUpdated() signal received. */
    QList<QDomNode> recentlyRelocatedNodes_;
    /*! \brief A list of WbItem's whose nodes don't have 'id' attributes. */
    QList<WbItem *> idlessItems_;
    /*! \brief Pointer to a new item that is being drawn.*/
    WbNewItem *newWbItem_;
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

#endif // WBWIDGET_H
