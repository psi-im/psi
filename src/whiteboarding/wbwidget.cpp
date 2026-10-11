/*
 * wbwidget.cpp - a widget for processing and showing whiteboard
 *                messages.
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

#include "wbwidget.h"

#include "wbnewpath.h"
#include "wbnewshape.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QFile>
#include <QImageReader>
#include <QInputDialog>
#include <QMimeData>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QScrollBar>
#include <QtMath>

#include <cmath>
#include <QRegularExpression>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

WbWidget::WbWidget(XMPP::Jingle::SXE::Document *document, QWidget *parent) : QGraphicsView(parent)
{
    newWbItem_   = nullptr;
    strokeColor_ = Qt::black;
    fillColor_   = Qt::transparent;
    strokeWidth_ = 1;
    document_    = document;

    //    setCacheMode(CacheBackground);
    setRenderHint(QPainter::Antialiasing);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // The whole canvas is always fitted into the view, so there is nothing to scroll.
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // create the scene
    scene_ = new WbScene(document_, this);
    scene_->setItemIndexMethod(QGraphicsScene::NoIndex);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorViewCenter);
    setScene(scene_);
    // All interaction is interpreted here; items don't handle mouse events themselves.
    setInteractive(false);
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    // Context menus are opened on a right click without dragging (a right drag rotates).
    setContextMenuPolicy(Qt::PreventContextMenu);
    viewport()->setContextMenuPolicy(Qt::PreventContextMenu);
    rubberBand_ = new QRubberBand(QRubberBand::Rectangle, viewport());

    // render the initial document
    rerender();
    // rerender on update
    connect(document_, SIGNAL(documentUpdated(bool)), SLOT(handleDocumentUpdated(bool)));

    // add the initial items
    const QDomNodeList children = document_->document().documentElement().childNodes();
    for (int i = 0; i < int(children.length()); i++) {
        const QDomNode node = children.at(i);
        if (node.isElement()) {
            queueNodeInspection(node.toElement());
        }
    }
    inspectNodes();

    // add new items as nodes are added
    // remove/add items if corresponding nodes are moved
    connect(document_, SIGNAL(nodeAdded(QDomNode, bool)), SLOT(queueNodeInspection(QDomNode)));
    connect(document_, SIGNAL(nodeMoved(QDomNode, bool)), SLOT(queueNodeInspection(QDomNode)));
    // remove items if corresponding nodes are deleted
    connect(document_, SIGNAL(nodeToBeRemoved(QDomNode, bool)), SLOT(removeWbItem(QDomNode)));
    connect(document_, SIGNAL(nodeToBeRemoved(QDomNode, bool)), SLOT(checkForRemovalOfId(QDomNode)));
    // adjust the viewBox as necessary
    connect(document_, SIGNAL(nodeAdded(QDomNode, bool)), SLOT(checkForViewBoxChange(QDomNode)));
    connect(document_, SIGNAL(nodeMoved(QDomNode, bool)), SLOT(checkForViewBoxChange(QDomNode)));
    connect(document_, SIGNAL(chdataChanged(QDomNode, bool)), SLOT(checkForViewBoxChange(QDomNode)));

    // set the default mode to select
    setMode(Mode::Select);

    // set the initial size
    if (document_->document().documentElement().hasAttribute("viewBox"))
        checkForViewBoxChange(document_->document().documentElement().attributeNode("viewBox"));
    else {
        QSize  size;
        QRectF rect = scene_->sceneRect();
        size.setWidth(int(rect.x() + rect.width()));
        size.setHeight(int(rect.y() + rect.height()));
        if (size.width() > 0 && size.height() > 0)
            setSize(size);
        else
            setSize(QSize(400, 600));
    }
}

XMPP::Jingle::SXE::Document *WbWidget::document() { return document_; }

WbWidget::Mode WbWidget::mode() { return mode_; }

void WbWidget::setMode(Mode mode)
{
    mode_ = mode;
    if (mode_ < Mode::DrawPath && newWbItem_) {
        delete newWbItem_;
        newWbItem_ = nullptr;
    }
    updateCursor();
}

void WbWidget::setEditable(bool editable)
{
    editable_ = editable;
    if (!editable_ && newWbItem_) {
        delete newWbItem_;
        newWbItem_ = nullptr;
    }
    updateCursor();
}

void WbWidget::fitToView()
{
    fitToView_          = true;
    const QRectF canvas = scene_->sceneRect();
    if (canvas.isEmpty() || viewport()->width() <= 0 || viewport()->height() <= 0)
        return;
    // Unlike fitInView() no margin is added: the background shows only where the aspect ratios differ.
    const qreal scale = qMin(viewport()->width() / canvas.width(), viewport()->height() / canvas.height());
    setTransform(QTransform::fromScale(scale, scale));
    centerOn(canvas.center());
}

void WbWidget::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, palette().color(QPalette::Mid));
    const QRectF canvas = scene_->sceneRect();
    painter->fillRect(canvas.intersected(rect), Qt::white);
    painter->save();
    QPen border(palette().color(QPalette::Dark));
    border.setCosmetic(true); // one device pixel at any zoom
    painter->setPen(border);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(canvas);
    painter->restore();
}

void WbWidget::setSize(const QSize &size)
{
    document_->setAttribute(document_->document().documentElement(), "viewBox",
                            QString("0 0 %1 %2").arg(size.width()).arg(size.height()));
    document_->flush();
}

/*! \brief Generates a QRectF based on \a string provided in the SVG viewBox format.
 *  Returns a null rectangle unless the value consists of exactly four numbers. */
static QRectF parseSvgViewBox(const QString &string)
{
    // SVG: four numbers separated by whitespace and/or a comma.
    static const QRegularExpression separators(QStringLiteral("[\\s,]+"));
    const auto parts = string.trimmed().split(separators, Qt::SkipEmptyParts);
    if (parts.size() != 4)
        return {};
    qreal numbers[4];
    for (int i = 0; i < 4; ++i) {
        bool ok    = false;
        numbers[i] = parts[i].toDouble(&ok);
        if (!ok)
            return {};
    }
    return QRectF(numbers[0], numbers[1], numbers[2], numbers[3]);
}

void WbWidget::checkForViewBoxChange(const QDomNode &node)
{
    if (node.isAttr() && node.nodeName().toLower() == "viewbox"
        && node.parentNode() == document_->document().documentElement()) {
        QRectF box = parseSvgViewBox(node.nodeValue());
        if (box.width() > 0 && box.height() > 0) {
            scene_->setSceneRect(box);
            if (fitToView_)
                fitToView();
        }
    }
}

void WbWidget::setStrokeColor(const QColor &color) { strokeColor_ = color; }

void WbWidget::setFillColor(const QColor &color) { fillColor_ = color; }

void WbWidget::setStrokeWidth(int width) { strokeWidth_ = width; }

void WbWidget::clear()
{
    const auto &gItems = scene_->items();
    for (QGraphicsItem *graphicsitem : gItems) {
        WbItem *wbitem = dynamic_cast<WbItem *>(graphicsitem);
        if (wbitem)
            document_->removeNode(wbitem->node());
    }
    document_->flush();
}

QSize WbWidget::sizeHint() const
{
    if (scene_)
        return scene_->sceneRect().size().toSize();
    else
        return QSize();
}

void WbWidget::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (fitToView_)
        fitToView();
}

namespace {
QPoint eventPos(const QMouseEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return event->pos();
#else
    return event->position().toPoint();
#endif
}

QPoint eventGlobalPos(const QMouseEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return event->globalPos();
#else
    return event->globalPosition().toPoint();
#endif
}

qreal angleOf(const QPointF &vector) { return qRadiansToDegrees(qAtan2(vector.y(), vector.x())); }
} // namespace

void WbWidget::mousePressEvent(QMouseEvent *event)
{
    if (gesture_ != Gesture::None)
        return; // one gesture at a time
    setFocus(Qt::MouseFocusReason);

    const auto pos  = eventPos(event);
    const auto mods = event->modifiers();
    pressPos_       = pos;
    lastViewPos_    = pos;
    lastScenePos_   = mapToScene(pos);
    gestureButton_  = event->button();
    pressItem_      = nullptr;

    // Panning works in every mode and also on a read-only board.
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && spaceHeld_)) {
        gesture_ = Gesture::Pan;
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }

    bool contextClick = event->button() == Qt::RightButton;
#ifdef Q_OS_MACOS
    // Control-click is the right click of one-button trackpads. Qt reports Control as Meta on macOS.
    contextClick = contextClick || (event->button() == Qt::LeftButton && (mods & Qt::MetaModifier));
#endif
    if (contextClick) {
        gesture_   = Gesture::ContextPending;
        pressItem_ = wbItemAt(pos);
        return;
    }
    if (event->button() != Qt::LeftButton || !editable_) {
        gestureButton_ = Qt::NoButton;
        return;
    }

    // The selection modifier is Command on macOS: Qt maps it to ControlModifier there.
    if (mode_ == Mode::Select || (mods & Qt::ControlModifier)) {
        WbItem *item = wbItemAt(pos);
        pressItem_   = item;
        if (!item) {
            if (!(mods & Qt::ShiftModifier))
                scene_->clearSelection();
            gesture_ = Gesture::RubberBand;
            rubberBand_->setGeometry(QRect(pos, QSize()));
            rubberBand_->show();
        } else if (mods & Qt::ShiftModifier) {
            gesture_ = Gesture::TogglePending; // click toggles, drag on the selection scales
        } else {
            if (!item->isSelected()) {
                scene_->clearSelection();
                item->setSelected(true);
            }
            gesture_ = Gesture::Move;
        }
        return;
    }

    if (mode_ == Mode::Erase) {
        gesture_ = Gesture::Erase;
        eraseAt(pos);
    } else if (mode_ == Mode::DrawPath || mode_ == Mode::DrawLine) {
        delete newWbItem_;
        // Shift draws a straight line with the pen.
        newWbItem_ = new WbNewPath(scene_, lastScenePos_, strokeWidth_, strokeColor_, fillColor_,
                                   mode_ == Mode::DrawLine || (mods & Qt::ShiftModifier));
        gesture_   = Gesture::Draw;
    } else if (mode_ == Mode::DrawRectangle || mode_ == Mode::DrawEllipse || mode_ == Mode::DrawRhombus) {
        delete newWbItem_;
        const auto shape = mode_ == Mode::DrawRectangle ? WbNewShape::Shape::Rectangle
            : mode_ == Mode::DrawEllipse                ? WbNewShape::Shape::Ellipse
                                                        : WbNewShape::Shape::Rhombus;
        auto       item  = new WbNewShape(scene_, lastScenePos_, shape, strokeWidth_, strokeColor_, fillColor_);
        item->setConstrained(mods & Qt::ShiftModifier); // square, circle
        newWbItem_ = item;
        gesture_   = Gesture::Draw;
    } else if (mode_ == Mode::DrawText) {
        gestureButton_ = Qt::NoButton;
        insertText(lastScenePos_);
    } else if (mode_ == Mode::DrawImage) {
        gestureButton_       = Qt::NoButton;
        const auto scenePos  = lastScenePos_;
        const auto fileName  = QFileDialog::getOpenFileName(this, tr("Choose an image"), QString(),
                                                            tr("Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));
        if (!fileName.isEmpty())
            insertImageFile(fileName, scenePos);
    }
}

void WbWidget::mouseMoveEvent(QMouseEvent *event)
{
    const auto pos      = eventPos(event);
    const auto scenePos = mapToScene(pos);
    const bool dragged
        = (pos - pressPos_).manhattanLength() >= QApplication::startDragDistance();

    switch (gesture_) {
    case Gesture::None:
        return;
    case Gesture::Pan: {
        const auto delta = pos - lastViewPos_;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        break;
    }
    case Gesture::ContextPending:
        // A right drag over the selection rotates it.
        if (!dragged || !editable_ || selectedWbItems().isEmpty())
            return;
        gesture_ = Gesture::Rotate;
        pivot_   = scene_->selectionCenter();
        Q_FALLTHROUGH();
    case Gesture::Rotate:
        if ((lastScenePos_ - pivot_).manhattanLength() > 1 && (scenePos - pivot_).manhattanLength() > 1)
            rotateSelection(angleOf(scenePos - pivot_) - angleOf(lastScenePos_ - pivot_), pivot_);
        break;
    case Gesture::TogglePending:
        if (!dragged || !pressItem_ || !pressItem_->isSelected())
            return;
        gesture_ = Gesture::Scale;
        pivot_   = scene_->selectionCenter();
        Q_FALLTHROUGH();
    case Gesture::Scale: {
        const auto before = QLineF(pivot_, lastScenePos_).length();
        const auto after  = QLineF(pivot_, scenePos).length();
        if (before > 1 && after > 1)
            scaleSelection(after / before, pivot_);
        break;
    }
    case Gesture::Move:
        translateSelection(lastScenePos_, scenePos);
        break;
    case Gesture::RubberBand:
        rubberBand_->setGeometry(QRect(pressPos_, pos).normalized());
        break;
    case Gesture::Draw:
        if (auto shape = dynamic_cast<WbNewShape *>(newWbItem_))
            shape->setConstrained(event->modifiers() & Qt::ShiftModifier);
        if (newWbItem_)
            newWbItem_->parseCursorMove(scenePos);
        break;
    case Gesture::Erase:
        eraseAt(pos);
        break;
    }
    lastViewPos_  = pos;
    lastScenePos_ = scenePos;
}

void WbWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (gesture_ == Gesture::None || event->button() != gestureButton_)
        return;
    const auto gesture = gesture_;
    gesture_           = Gesture::None;
    gestureButton_     = Qt::NoButton;

    switch (gesture) {
    case Gesture::ContextPending:
        if (pressItem_ && editable_) {
            if (!pressItem_->isSelected()) {
                scene_->clearSelection();
                pressItem_->setSelected(true);
            }
            pressItem_->showContextMenu(eventGlobalPos(event));
        } else {
            emit canvasContextMenuRequested(eventGlobalPos(event));
        }
        break;
    case Gesture::TogglePending:
        if (pressItem_)
            pressItem_->setSelected(!pressItem_->isSelected());
        break;
    case Gesture::Move:
    case Gesture::Scale:
    case Gesture::Rotate:
        commitTransformations();
        break;
    case Gesture::RubberBand: {
        QPainterPath area;
        area.addPolygon(mapToScene(rubberBand_->geometry()));
        rubberBand_->hide();
        for (WbItem *item : std::as_const(items_)) {
            if (area.intersects(item->sceneBoundingRect()))
                item->setSelected(true);
        }
        break;
    }
    case Gesture::Draw:
        if (newWbItem_) {
            QDomDocument tempDoc;
            document_->insertNodeAfter(newWbItem_->serializeToSvg(&tempDoc), document_->document().documentElement());
            document_->flush();
            delete newWbItem_;
            newWbItem_ = nullptr;
        }
        break;
    case Gesture::Erase:
        document_->flush();
        break;
    case Gesture::Pan:
    case Gesture::None:
        break;
    }
    pressItem_ = nullptr;
    updateCursor();
}

WbItem *WbWidget::wbItem(const QDomNode &node)
{
    for (WbItem *wbitem : std::as_const(items_)) {
        if (wbitem->node() == node)
            return wbitem;
    }
    return nullptr;
}

void WbWidget::handleDocumentUpdated(bool remote)
{
    Q_UNUSED(remote);
    inspectNodes();
    rerender();
}

void WbWidget::inspectNodes()
{
    while (!recentlyRelocatedNodes_.isEmpty()) {
        QDomNode node = recentlyRelocatedNodes_.takeFirst();

        if (!node.isElement())
            continue;

        // check if we already have a WbItem for the node
        WbItem *item = wbItem(node);

        // We don't need to do anything iff node is child of <svg/> and item exists or vice versa
        if ((item != nullptr) == (node.parentNode() == document_->document().documentElement()))
            continue;

        // Otherwise, either item exists and needs to be removed
        //              or it doesn't exist and needs to be added
        if (item)
            removeWbItem(item);
        else
            items_.append(new WbItem(document_, &renderer_, node.toElement(), scene_, this));
    }
}

void WbWidget::queueNodeInspection(const QDomNode &node)
{
    if (node.isElement())
        recentlyRelocatedNodes_.append(node);
}

void WbWidget::removeWbItem(const QDomNode &node) { removeWbItem(wbItem(node)); }

void WbWidget::removeWbItem(WbItem *wbitem)
{
    if (wbitem) {
        qDebug("delete WbItem");
        // Remove from the lookup table to avoid infinite loop of deletes
        items_.removeAll(wbitem);
        // items_.takeAt(items_.indexOf(wbitem));

        idlessItems_.removeAll(wbitem);

        delete wbitem;
    }
}

void WbWidget::addIds()
{
    while (!idlessItems_.isEmpty())
        idlessItems_.takeFirst()->addToScene();

    document_->flush();
}

void WbWidget::addToIdLess(const QDomElement &element)
{
    if (element.parentNode() != document_->document().documentElement())
        return;

    WbItem *item = wbItem(element);
    if (item && !idlessItems_.contains(item)) {
        // Remove from the scene until a new 'id' attribute is added
        item->removeFromScene();

        idlessItems_.append(item);

        // Try adding the 'id' attribute after a random delay of 0 to 2s
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        QTimer::singleShot(QRandomGenerator::global()->bounded(2000), this, SLOT(addIds()));
#else
        QTimer::singleShot(2000 * qrand() / RAND_MAX, this, SLOT(addIds()));
#endif
    }
}

void WbWidget::checkForRemovalOfId(QDomNode node)
{
    if (node.isAttr()) {
        if (node.nodeName() == "id") {
            while (!(node.isElement() || node.isNull()))
                node = node.parentNode();

            if (!node.isNull())
                addToIdLess(node.toElement());
        }
    }
}

void WbWidget::rerender()
{
    QString     xmldump;
    QTextStream stream(&xmldump);
    document_->document().save(stream, 1);

    // qDebug("Document in WbWidget:");
    // qDebug() << xmldump.toLatin1();

    renderer_.load(xmldump.toLatin1());

    // Update all positions if changed
    for (WbItem *wbitem : std::as_const(items_)) {
        // resetting elementId is necessary for rendering some updates to the element (e.g. adding child elements to
        // <g/>)
        wbitem->setElementId(wbitem->id());

        // qDebug() << QString("Rerendering %1").arg((unsigned int) wbitem).toLatin1();
        wbitem->resetPos();
    }
}

namespace {
bool isShortcut(const QKeyEvent *event, const char *sequence)
{
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QKeySequence pressed(QKeyCombination(modifiers, Qt::Key(event->key())));
#else
    const QKeySequence pressed(int(modifiers) | event->key());
#endif
    return pressed == QKeySequence(QString::fromLatin1(sequence));
}

QPoint dropPos(const QDropEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return event->pos();
#else
    return event->position().toPoint();
#endif
}

bool hasImagePayload(const QMimeData *mime)
{
    if (mime->hasImage())
        return true;
    const auto urls = mime->urls();
    return std::any_of(urls.cbegin(), urls.cend(), [](const QUrl &url) { return url.isLocalFile(); });
}

void renewIds(QDomElement element)
{
    if (element.hasAttribute(QStringLiteral("id")))
        element.setAttribute(QStringLiteral("id"), QLatin1String("e") + XMPP::Jingle::SXE::Document::generateUUID());
    for (auto child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        renewIds(child);
}
} // namespace

void WbWidget::wheelEvent(QWheelEvent *event)
{
    const auto delta = event->angleDelta();
    // Some platforms turn Alt+wheel into horizontal scrolling.
    const int steps = delta.y() != 0 ? delta.y() : delta.x();
    if (event->modifiers() & Qt::ControlModifier) {
        if (steps)
            zoomBy(qPow(1.0015, steps));
        event->accept();
        return;
    }
    if ((event->modifiers() & Qt::AltModifier) && editable_ && !selectedWbItems().isEmpty()) {
        if (steps) {
            rotateSelection(steps / 120.0 * 5, scene_->selectionCenter());
            commitTransformations();
        }
        event->accept();
        return;
    }
    if (fitToView_) {
        event->accept(); // nothing to scroll while the whole canvas is visible
        return;
    }
    QGraphicsView::wheelEvent(event);
}

bool WbWidget::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto gesture = static_cast<QNativeGestureEvent *>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            zoomBy(1 + gesture->value());
            return true;
        }
        if (gesture->gestureType() == Qt::SmartZoomNativeGesture) {
            if (fitToView_)
                zoomBy(2);
            else
                fitToView();
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

void WbWidget::keyPressEvent(QKeyEvent *event)
{
    const int  key       = event->key();
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool plain     = !(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));

    if (key == Qt::Key_Space && modifiers == Qt::NoModifier) {
        if (!event->isAutoRepeat()) {
            spaceHeld_ = true;
            updateCursor();
        }
        return;
    }
    if (event->matches(QKeySequence::ZoomIn) || isShortcut(event, "Ctrl+=")) {
        zoomBy(1.25, false);
        return;
    }
    if (event->matches(QKeySequence::ZoomOut)) {
        zoomBy(0.8, false);
        return;
    }
    if (isShortcut(event, "Ctrl+0")) {
        fitToView();
        return;
    }
    if (key == Qt::Key_Escape) {
        if (gesture_ == Gesture::Draw || gesture_ == Gesture::RubberBand) {
            delete newWbItem_;
            newWbItem_ = nullptr;
            rubberBand_->hide();
            gesture_ = Gesture::None;
        } else if (!scene_->selectedItems().isEmpty()) {
            scene_->clearSelection();
        } else {
            event->ignore(); // let the dialog handle it
        }
        return;
    }
    if (!editable_) {
        QGraphicsView::keyPressEvent(event);
        return;
    }

    if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        return;
    }
    if (event->matches(QKeySequence::Delete) || key == Qt::Key_Backspace) {
        deleteSelection();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        const auto image = QApplication::clipboard()->image();
        if (!image.isNull())
            insertImage(image, mapToScene(viewport()->rect().center()));
        return;
    }
    if (isShortcut(event, "Ctrl+D")) {
        duplicateSelection();
        return;
    }
    if (isShortcut(event, "Ctrl+G")) {
        scene_->group();
        return;
    }
    if (isShortcut(event, "Ctrl+Shift+G")) {
        scene_->ungroup();
        return;
    }

    if (plain && !selectedWbItems().isEmpty()) {
        const qreal step = (modifiers & Qt::ShiftModifier) ? 10 : 1;
        QPointF     nudge;
        switch (key) {
        case Qt::Key_Left:
            nudge = { -step, 0 };
            break;
        case Qt::Key_Right:
            nudge = { step, 0 };
            break;
        case Qt::Key_Up:
            nudge = { 0, -step };
            break;
        case Qt::Key_Down:
            nudge = { 0, step };
            break;
        case Qt::Key_PageUp:
            scene_->bringForward();
            return;
        case Qt::Key_PageDown:
            scene_->sendBackwards();
            return;
        case Qt::Key_Home:
            scene_->bringToFront();
            return;
        case Qt::Key_End:
            scene_->sendToBack();
            return;
        case Qt::Key_Comma:
        case Qt::Key_Period:
        case Qt::Key_Less:
        case Qt::Key_Greater: {
            // , and . rotate by 15 degrees, < and > (Shift) by 1 degree.
            const bool  fine  = key == Qt::Key_Less || key == Qt::Key_Greater || (modifiers & Qt::ShiftModifier);
            const bool  back  = key == Qt::Key_Comma || key == Qt::Key_Less;
            const qreal angle = fine ? 1 : 15;
            rotateSelection(back ? -angle : angle, scene_->selectionCenter());
            commitTransformations();
            return;
        }
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            scaleSelection(1.1, scene_->selectionCenter());
            commitTransformations();
            return;
        case Qt::Key_Minus:
            scaleSelection(1 / 1.1, scene_->selectionCenter());
            commitTransformations();
            return;
        default:
            break;
        }
        if (!nudge.isNull()) {
            translateSelection(QPointF(), nudge);
            commitTransformations();
            return;
        }
    }

    if (plain) {
        switch (key) {
        case Qt::Key_P:
            setModeFromKeyboard(Mode::DrawPath);
            return;
        case Qt::Key_E:
            setModeFromKeyboard(Mode::Erase);
            return;
        case Qt::Key_V:
            setModeFromKeyboard(Mode::Select);
            return;
        case Qt::Key_I:
            setModeFromKeyboard(Mode::DrawImage);
            return;
        case Qt::Key_L:
            setModeFromKeyboard(Mode::DrawLine);
            return;
        case Qt::Key_R:
            setModeFromKeyboard(Mode::DrawRectangle);
            return;
        case Qt::Key_O:
            setModeFromKeyboard(Mode::DrawEllipse);
            return;
        case Qt::Key_D:
            setModeFromKeyboard(Mode::DrawRhombus);
            return;
        case Qt::Key_T:
            setModeFromKeyboard(Mode::DrawText);
            return;
        case Qt::Key_BracketLeft:
        case Qt::Key_BracketRight:
            strokeWidth_ = qBound(1, strokeWidth_ + (key == Qt::Key_BracketRight ? 1 : -1), 64);
            emit strokeWidthChanged(strokeWidth_);
            return;
        default:
            break;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void WbWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spaceHeld_ = false;
        updateCursor();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void WbWidget::focusOutEvent(QFocusEvent *event)
{
    spaceHeld_ = false;
    updateCursor();
    QGraphicsView::focusOutEvent(event);
}

void WbWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (editable_ && hasImagePayload(event->mimeData()))
        event->acceptProposedAction();
    else
        event->ignore();
}

void WbWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (editable_ && hasImagePayload(event->mimeData()))
        event->acceptProposedAction();
    else
        event->ignore();
}

void WbWidget::dropEvent(QDropEvent *event)
{
    if (!editable_)
        return;
    const auto mime = event->mimeData();
    QPointF    pos  = mapToScene(dropPos(event));
    if (mime->hasImage()) {
        insertImage(qvariant_cast<QImage>(mime->imageData()), pos);
    } else {
        const auto urls = mime->urls();
        for (const auto &url : urls) {
            if (!url.isLocalFile())
                continue;
            insertImageFile(url.toLocalFile(), pos);
            pos += QPointF(20, 20);
        }
    }
    event->acceptProposedAction();
}

WbItem *WbWidget::wbItemAt(const QPoint &viewPos) const
{
    const auto under = items(viewPos);
    for (QGraphicsItem *item : under) {
        if (auto wbitem = dynamic_cast<WbItem *>(item))
            return wbitem;
    }
    return nullptr;
}

QList<WbItem *> WbWidget::selectedWbItems() const
{
    QList<WbItem *> result;
    const auto      selected = scene_->selectedItems();
    for (QGraphicsItem *item : selected) {
        if (auto wbitem = dynamic_cast<WbItem *>(item))
            result.append(wbitem);
    }
    return result;
}

void WbWidget::translateSelection(const QPointF &from, const QPointF &to)
{
    const auto selected = selectedWbItems();
    for (WbItem *item : selected) {
        const QPointF d = item->mapFromScene(to) - item->mapFromScene(from);
        item->setTransform(QTransform::fromTranslate(d.x(), d.y()), true);
        scene_->queueTransformationRegeneration(item);
    }
}

void WbWidget::rotateSelection(qreal degrees, const QPointF &pivot)
{
    QTransform delta;
    delta.rotate(degrees);
    const auto selected = selectedWbItems();
    for (WbItem *item : selected) {
        const QPointF itemPivot = item->mapFromScene(pivot);
        QTransform    translation;
        translation.translate(-itemPivot.x(), -itemPivot.y());
        item->setTransform(translation * delta * translation.inverted(), true);
        scene_->queueTransformationRegeneration(item);
    }
}

void WbWidget::scaleSelection(qreal factor, const QPointF &pivot)
{
    QTransform delta;
    delta.scale(factor, factor);
    const auto selected = selectedWbItems();
    for (WbItem *item : selected) {
        const QPointF itemPivot = item->mapFromScene(pivot);
        QTransform    translation;
        translation.translate(-itemPivot.x(), -itemPivot.y());
        item->setTransform(translation * delta * translation.inverted(), true);
        scene_->queueTransformationRegeneration(item);
    }
}

void WbWidget::commitTransformations() { scene_->regenerateTransformations(); }

void WbWidget::deleteSelection()
{
    QList<QDomNode> nodes;
    const auto      selected = selectedWbItems();
    for (WbItem *item : selected)
        nodes.append(item->node());
    for (const auto &node : std::as_const(nodes))
        document_->removeNode(node);
    document_->flush();
}

void WbWidget::duplicateSelection()
{
    QList<QDomElement> elements;
    const auto         selected = selectedWbItems();
    for (WbItem *item : selected)
        elements.append(item->node().toElement());
    const auto root = document_->document().documentElement();
    for (const auto &element : std::as_const(elements)) {
        auto clone = element.cloneNode(true).toElement();
        renewIds(clone);
        // Offset the copy in canvas coordinates, after its own transformation.
        const auto transform = WbItem::parseSvgTransform(clone.attribute(QStringLiteral("transform")))
            * QTransform::fromTranslate(10, 10);
        clone.setAttribute(QStringLiteral("transform"), WbItem::toSvgTransform(transform));
        document_->insertNodeAfter(clone, root, element);
    }
    scene_->clearSelection();
    document_->flush();
}

void WbWidget::selectAll()
{
    for (WbItem *item : std::as_const(items_))
        item->setSelected(true);
}

void WbWidget::eraseAt(const QPoint &viewPos)
{
    const QPointF p = mapToScene(viewPos);
    // Erase within the stroke width, but at least a few screen pixels.
    const qreal r = qMax<qreal>(strokeWidth_, 4 / qMax<qreal>(transform().m11(), 0.01));
    // Collect the nodes first: removing a node deletes its item.
    QList<QDomNode> nodes;
    const auto      under = scene_->items(QRectF(p.x() - r, p.y() - r, 2 * r, 2 * r));
    for (QGraphicsItem *item : under) {
        if (auto wbitem = dynamic_cast<WbItem *>(item))
            nodes.append(wbitem->node());
    }
    for (const auto &node : std::as_const(nodes))
        document_->removeNode(node);
}

void WbWidget::zoomBy(qreal factor, bool underMouse)
{
    const QRectF canvas = scene_->sceneRect();
    if (canvas.isEmpty() || factor <= 0)
        return;
    const qreal fitScale = qMin(viewport()->width() / canvas.width(), viewport()->height() / canvas.height());
    const qreal current  = transform().m11();
    const qreal target   = qMin(current * factor, fitScale * 32);
    if (target <= fitScale * 1.001) {
        fitToView();
        return;
    }

    QPoint anchor = viewport()->rect().center();
    if (underMouse) {
        const auto cursor = viewport()->mapFromGlobal(QCursor::pos());
        if (viewport()->rect().contains(cursor))
            anchor = cursor;
    }
    const QPointF anchorScene = mapToScene(anchor);
    fitToView_                = false;
    setTransformationAnchor(NoAnchor);
    scale(target / current, target / current);
    setTransformationAnchor(AnchorUnderMouse);
    // Keep the canvas point under the anchor where it was.
    const QPoint shift = mapFromScene(anchorScene) - anchor;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + shift.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + shift.y());
}

void WbWidget::insertImageFile(const QString &fileName, const QPointF &scenePos)
{
    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        qDebug("Whiteboard: can't read image %s: %s", qPrintable(fileName), qPrintable(reader.errorString()));
        return;
    }
    insertImage(image, scenePos);
}

void WbWidget::insertImage(const QImage &source, const QPointF &scenePos)
{
    if (source.isNull())
        return;
    // Images travel inline as data: URIs; keep them reasonably small.
    constexpr int maxSide = 1600;
    QImage        image   = source;
    if (image.width() > maxSide || image.height() > maxSide)
        image = image.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QByteArray data;
    QBuffer    buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QString mime;
    if (image.hasAlphaChannel()) {
        image.save(&buffer, "PNG");
        mime = QStringLiteral("image/png");
    } else {
        image.save(&buffer, "JPEG", 85);
        mime = QStringLiteral("image/jpeg");
    }

    // Fit large images into the canvas.
    QSizeF       size   = image.size();
    const QRectF canvas = scene_->sceneRect();
    if (!canvas.isEmpty() && (size.width() > canvas.width() * 0.8 || size.height() > canvas.height() * 0.8))
        size.scale(canvas.size() * 0.8, Qt::KeepAspectRatio);

    QDomDocument temp;
    auto         element = temp.createElement(QStringLiteral("image"));
    element.setAttribute(QStringLiteral("id"), QLatin1String("e") + XMPP::Jingle::SXE::Document::generateUUID());
    element.setAttribute(QStringLiteral("x"), scenePos.x() - size.width() / 2);
    element.setAttribute(QStringLiteral("y"), scenePos.y() - size.height() / 2);
    element.setAttribute(QStringLiteral("width"), size.width());
    element.setAttribute(QStringLiteral("height"), size.height());
    element.setAttributeNS(QStringLiteral("http://www.w3.org/1999/xlink"), QStringLiteral("xlink:href"),
                           QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(data.toBase64())));
    document_->insertNodeAfter(element, document_->document().documentElement());
    document_->flush();
}

namespace {
QString svgPathData(const QPainterPath &path)
{
    auto    number = [](qreal value) { return QString::number(std::round(value * 100) / 100); };
    QString data;
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);
        switch (e.type) {
        case QPainterPath::MoveToElement:
            data += QStringLiteral("M%1 %2").arg(number(e.x), number(e.y));
            break;
        case QPainterPath::LineToElement:
            data += QStringLiteral("L%1 %2").arg(number(e.x), number(e.y));
            break;
        case QPainterPath::CurveToElement: {
            const auto c2 = path.elementAt(i + 1);
            const auto to = path.elementAt(i + 2);
            data += QStringLiteral("C%1 %2 %3 %4 %5 %6")
                        .arg(number(e.x), number(e.y), number(c2.x), number(c2.y), number(to.x), number(to.y));
            i += 2;
            break;
        }
        case QPainterPath::CurveToDataElement:
            break;
        }
    }
    return data;
}
} // namespace

void WbWidget::insertText(const QPointF &scenePos)
{
    bool          ok   = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Add text"), tr("Text:"), QString(), &ok);
    if (!ok || text.trimmed().isEmpty())
        return;

    // The size follows the stroke width so [ and ] work for text too.
    QFont font;
    font.setPixelSize(12 + 4 * strokeWidth_);
    const QFontMetricsF metrics(font);
    QPainterPath        path;
    const auto          lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i)
        path.addText(scenePos + QPointF(0, metrics.ascent() + i * metrics.lineSpacing()), font, lines[i]);
    if (path.isEmpty())
        return;

    QDomDocument temp;
    auto         element = temp.createElement(QStringLiteral("path"));
    element.setAttribute(QStringLiteral("id"), QLatin1String("e") + XMPP::Jingle::SXE::Document::generateUUID());
    element.setAttribute(QStringLiteral("d"), svgPathData(path));
    element.setAttribute(QStringLiteral("fill"), strokeColor_.name());
    element.setAttribute(QStringLiteral("stroke"), QStringLiteral("none"));
    document_->insertNodeAfter(element, document_->document().documentElement());
    document_->flush();
}

void WbWidget::setModeFromKeyboard(Mode mode)
{
    setMode(mode);
    emit modeChanged(mode);
}

void WbWidget::updateCursor()
{
    if (spaceHeld_ || gesture_ == Gesture::Pan)
        viewport()->setCursor(gesture_ == Gesture::Pan ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    else if (!editable_ || mode_ == Mode::Select)
        viewport()->setCursor(Qt::ArrowCursor);
    else
        viewport()->setCursor(Qt::CrossCursor);
}
