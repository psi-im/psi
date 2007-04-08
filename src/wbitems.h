/*
 * wbitems.h - the item classes for the SVG WB
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

#ifndef WBITEMS_H
#define WBITEMS_H
#include <QDomElement>
#include <QHash>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QColorDialog>
#include <QFontDialog>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtCrypto>
#include "iconaction.h"
#include <cmath>

/*! \brief A class used for storing an edit while it's being queued.*/
class Edit {
public:
	/*! \brief Describes which kind of edit item defines.*/
	enum Type {NewEdit, MoveEdit, RemoveEdit, AttributeEdit, ParentEdit, ContentEdit};
	/*! \brief Constructor
	 *  Constructs an edit of \a type with \a xml.
	 */
	Edit(Type type, const QDomElement &xml);
	/*! \brief Constructor
	 *  Constructs an AttributeEdit or ParentEdit with the given oldValue and target.
	 */
	Edit(Type type, const QString &target, const QDomElement &edit, const QString &oldValue);
	/*! \brief Constructor
	 *  Constructs a ContentEdit with the given oldContent and target.
	 */
	Edit(const QString &target, const QDomElement &edit, QDomNodeList oldContent);
	/*! \brief The type of edit.*/
	Type type;
	/*! \brief The XML element representing the edit.*/
	QDomElement xml;
	/*! \brief The target of the edit.
	 *  Only applicable with AttributeEdit, ContentEdit and ParentEdit.
	 */
	QString target;
	/*! \brief The attribute value before the edit.
	 *  Only applicable with AttributeEdit
	 */
	QString oldValue;
	/*! \brief The content before the edit.
	 *  Only applicable with ContentEdit
	 */
	QDomNodeList oldContent;
	/*! \brief The parent before the edit.
	 *  Only applicable with ContentEdit
	 */
	QString oldParent;
};

/*! \brief An undo structure to save a &lt;configure/&gt; undo item
 *  Stores the new version, the attribute name and the previous value.
 *  An instance of this class is created for each undo and stored in
 *  a list in the WbItem.
 *  \sa WbItem
 */
class EditUndo {
public:
	/*! \brief Constructor
	 *  Constructs an EditUndo from the given version and Edit.
	 */
	EditUndo(const int &version, const Edit &edit);
	/*! \brief Constructor
	 *  Constructs an AttributeEdit undo with the given version, attribute and oldValue.
	 */
	EditUndo(const int &version, const QString &attribute, const QString &oldValue);
	/*! \brief Constructor
	 *  Constructs a ParentEdit undo with the given version and oldParent.
	 */
	EditUndo(const int &version, const QString &oldParent);
	/*! \brief Constructor
	 *  Constructs a ContentEdit undo with the given version and oldContent.
	 */
	EditUndo(const int &version, QDomNodeList oldContent);
	/*! \brief The new version.*/
	int version;
	/*! \brief The type of undo.*/
	Edit::Type type;
	/*! \brief The changed attribute.*/
	QString attribute;
	/*! \brief The attribute value before the edit.*/
	QString oldValue;
	/*! \brief The content before the edit.*/
	QDomNodeList oldContent;
	/*! \brief The parent before the edit.*/
	QString oldParent;
};

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

/*! \brief The base class for all whiteboard items.
 *  Every whiteboard item class must inherit this along with QGraphicsItem
 *  if the class is visualized on the scene. The derived class must implement at least
 *  type() and, if it's visualised, graphicsItem() and QGraphicsItem::mouseMoveEvent()
 *  and QGraphicsItem::mouseReleaseEvent() which should pass the events to 
 *  handleMouseMoveEvent() and handleMouseReleaseEvent() respectively.
 *
 *  This class stores all SVG attributes and provides parsing for the common
 *  attributes. Also provide support for the basic operations (translate,
 *  rotate and scale).
 *
 *  \sa WbPath
 */
class WbItem : public QObject {
	Q_OBJECT

public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id and \a index.
	 */
	WbItem(const QString &id);

	/*! \brief Return the ID of the parent item.*/
	QString parentWbItem();
	/*! \brief Set the parent.
	 *  Used secondarily, if graphicsItem()->parentItem == 0
	 */
	void setParentWbItem(const QString &, bool emitChanges = false);
	/*! \brief Return the ID of the item.*/
	QString id();
	/*! \brief Return the index of the item.*/
	qreal index();
	/*! \brief Set the index of the item.*/
	void setIndex(qreal index,  bool emitChanges = false);
	/*! \brief Construct a context menu with the default items.*/
	WbItemMenu* constructContextMenu();
	/*! \brief Set the 'stroke' attribute of the item.*/
	virtual void setStrokeColor(QColor color);
	/*! \brief Set the 'fill' attribute of the item.*/
	virtual void setFillColor(QColor color);
	/*! \brief Set the 'stroke-width' attribute of the item.*/
	virtual void setStrokeWidth(int width);
	/*! \brief Return the type of the item.*/
	virtual int type() const = 0;
	/*! \brief Return a pointer to the QGraphicsItem.*/
	virtual QGraphicsItem* graphicsItem() { return 0; };
	/*! \brief Return the item as an SVG element.*/
	virtual QDomElement svg();
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Regenerate and signal the SVG transformation matrix.*/
	virtual void regenerateTransform();
	/*! \brief Return the center of the item in item coordinates.*/
	virtual QPointF center();
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone() = 0;

	/*! \brief The current version of the item.*/
	int version;
	/*! \brief A list of EditUndo objects that contain the history of the item.*/
	QList<EditUndo> undos;

	/* Parsing */
	// General
//	static QString xmlSimplify(const QString &str);
	static QList<qreal> parseNumbersList(QString::const_iterator &itr);
	static QList<qreal> parsePercentageList(QString::const_iterator &itr);
	inline static bool isUnreserved(const QChar &c, bool first_char = false);
	static QString idFromUrl(const QString &url);
	static QMatrix parseTransformationMatrix(const QString &value);
	static QColor resolveColor(const QString &colorStr);
	static QColor constructColor(const QString &colorStr, const QString &opacity);
	static qreal parseLength(const QString &str);
	static QPen parseQPen(QString stroke, const QString &dashArray, const QString &dashOffset, const QString &linecap, const QString &linejoin, const QString &miterlimit, QString strokeopacity, QString opacity, const QString &width);
// 	static QPen defaultPen;

signals:
	/*! \brief Signals that an attribute of the element is changed.
	 *  \param id The ID of this element.
	 *  \param attribute The name of the changed attribute.
	 *  \param value The new value of the attribute.
	 *  \param oldvalue The old value of the attribute.
	 */
	void attributeChanged(const QString &id, const QString &attribute, const QString &value, const QString &oldvalue);
	/*! \brief Emitted when the parent of the element is changed.*/
	void parentChanged(const QString &id, const QString &value, const QString &oldvalue);
	/*! \brief Emitted when the content of the element is changed.*/
	void contentChanged(const QString &id, const QDomNodeList &value, const QDomNodeList &oldvalue);
	/*! \brief Emitted when the index of the element is changed.*/
	void indexChanged(const QString &id, const qreal &deltaIndex);

protected:
	/*! \brief Implements the default item interaction behaviour.
	 *  The action depends on the modifier keys being pressed:
	 *  \li Ctrl: Translate
	 *  \li Ctrl + Alt: Rotate
	 *  \li Ctrl + Shift: Scale
	 */
	void handleMouseMoveEvent(QGraphicsSceneMouseEvent * event);
	/*! \brief Implements the default item interaction behaviour.
	 *  The action depends on the modifier keys being pressed:
	 *  \li Ctrl: Translate
	 *  \li Ctrl + Alt: Rotate
	 *  \li Ctrl + Shift: Scale
	 */
	void handleMouseReleaseEvent(QGraphicsSceneMouseEvent * event);
	/*! \brief Contains the SVG attributes of the element.*/
	QHash<QString, QString> attributes;

private:
	/*! \brief The ID of the item.*/
	QString id_;
	/*! \brief The parent of the item.*/
	QString parent_;
	/*! \brief The index of the item.*/
	qreal index_;
};

/*! \brief An item for storing the unkown SVG elements.
 *  Inherits WbItem.
 *
 *  The purpose of this item is to store the unkown element. It is not visualized.
 *  \sa WbItem
 */
class WbUnknown : public WbItem
{
public:
	/*! \brief Constructor
	 *  Constructs a new root item.
	 */
	WbUnknown(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);
	/*! \brief Returns the type of the item (0x87654999).*/
	virtual int type() const { return 87654999; };
	/*! \brief Returns the stored SVG element.*/
	virtual QDomElement svg();
	/*! \brief Saves the SVG element.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();

private:
	QDomElement svgElement_;
};

/*! \brief An item for storing the SVG root element.
 *  Inherits WbItem. 
 *
 *  The main function of the element is to store the root element's attributes.
 *  The item is not visualized on the scene.
 *  \sa WbItem
 */
class WbRoot : public WbItem
{
public:
	/*! \brief Constructor
	 *  Constructs a new root item.
	 */
	WbRoot(QGraphicsScene* scene);
	/*! \brief Returns the type of the item (0x87654000).*/
	virtual int type() const { return 87654000; };
	/*! \brief Saves the SVG element.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();
private:
	QGraphicsScene* scene_;
};

/*! \brief An item for representing an SVG path element.
 *  Inherits QGraphicsPathItem and WbItem. 
 *
 *  Reimplements the methods dictated in WbItem's description.
 *
 *  Provides parsing for the 'd' and 'fill-rule' attributes.
 *  Also provides lineTo() and quadTo() methods (note that they don't
 *  emit an attributeChanged().)
 *
 *  \sa WbItem
 */
class WbPath : public QGraphicsPathItem, public WbItem
{
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbPath(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654001).*/
	virtual int type() const { return 87654001; };
	/*! \brief Returns a corrected version of the shape.*/
	virtual QPainterPath shape() const;
	/*! \brief Returns a QGraphicsItem* to self.*/
	virtual QGraphicsItem* graphicsItem() { return this; };
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();

	/*! \brief Adds a line to to the end of the path.*/
	void lineTo(const QPointF &newPoint);
	/*! \brief Adds a quadratic bezier curve to the end of the path.*/
	void quadTo(const QPointF &controlPoint, const QPointF &newPoint);

	static bool parsePathDataFast(const QString &data, QPainterPath &path);
	static void pathArcSegment(QPainterPath &path, qreal xc, qreal yc, qreal th0, qreal th1, qreal rx, qreal ry, qreal xAxisRotation);
	static void pathArc(QPainterPath &path, qreal rx, qreal ry, qreal x_axis_rotation, int large_arc_flag, int sweep_flag, qreal x, qreal y, qreal curx, qreal cury);

protected:
	/*! \brief Constructs and popsup the default context menu.*/
	virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	/*! \brief Passes the event to WbItem::handleMousMoveEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousReleaseEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
};

/*! \brief An item for an SVG ellipse element.
 *  Inherits QGraphicsEllipseItem and WbItem. 
 *
 *  Reimplements the methods dictated in WbItem's description.
 *
 *  Provides parsing for the 'rx' and 'ry' attributes.
 *
 *  \sa WbItem
 */
class WbEllipse : public QGraphicsEllipseItem, public WbItem {
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbEllipse(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654002).*/
	virtual int type() const { return 87654002; };
	/*! \brief Returns a QGraphicsItem* to self.*/
	virtual QGraphicsItem* graphicsItem() { return this; };
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();

protected:
	/*! \brief Constructs and popsup the default context menu.*/
	virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	/*! \brief Passes the event to WbItem::handleMousMoveEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousReleaseEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
};

/*! \brief An item for an SVG circle element.
 *  Inherits WbEllipse.
 *
 *  Provides parsing for the r attribute.
 *
 *  \sa WbItem
 */
class WbCircle : public WbEllipse {
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbCircle(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654003).*/
	virtual int type() const { return 87654003; };
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();
};

/*! \brief An item for an SVG rectangle element.
 *  Inherits QGraphicsRectItem and WbItem. 
 *
 *  Reimplements the methods dictated in WbItem's description.
 *
 *  Provides parsing for the 'width' and 'height' attributes.
 *
 *  \sa WbItem
 */
class WbRectangle : public QGraphicsRectItem, public WbItem {
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbRectangle(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654002).*/
	virtual int type() const { return 87654004; };
	/*! \brief Returns a QGraphicsItem* to self.*/
	virtual QGraphicsItem* graphicsItem() { return this; };
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();

protected:
	/*! \brief Constructs and popsup the default context menu.*/
	virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	/*! \brief Passes the event to WbItem::handleMousMoveEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousReleaseEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
};

/*! \brief An item for representing an SVG line element.
 *  Inherits QGraphicsPathItem and WbItem. 
 *
 *  Reimplements the methods dictated in WbItem's description.
 *
 *  Provides parsing for the 'x1', 'y1', 'x2', 'y2' attributes.
 *
 *  \sa WbItem
 */
class WbLine : public QGraphicsPathItem, public WbItem
{
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbLine(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654005).*/
	virtual int type() const { return 87654005; };
	/*! \brief Returns a corrected version of the shape.*/
	virtual QPainterPath shape() const;
	/*! \brief Returns a QGraphicsItem* to self.*/
	virtual QGraphicsItem* graphicsItem() { return this; };
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();

protected:
	/*! \brief Constructs and popsup the default context menu.*/
	virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	/*! \brief Passes the event to WbItem::handleMousMoveEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousReleaseEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
};

/*! \brief An item for representing an SVG polyline element.
 *  Inherits QGraphicsPathItem and WbItem. 
 *
 *  Reimplements the methods dictated in WbItem's description.
 *
 *  Provides parsing for the 'points' attribute.
 *
 *  \sa WbItem
 */
class WbPolyline : public QGraphicsPathItem, public WbItem
{
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbPolyline(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654006).*/
	virtual int type() const { return 87654006; };
	/*! \brief Returns a corrected version of the shape.*/
	virtual QPainterPath shape() const;
	/*! \brief Returns a QGraphicsItem* to self.*/
	virtual QGraphicsItem* graphicsItem() { return this; };
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();

protected:
	/*! \brief Constructs and popsup the default context menu.*/
	virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	/*! \brief Passes the event to WbItem::handleMousMoveEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousReleaseEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
};

/*! \brief An item for an SVG circle element.
 *  Inherits WbEllipse.
 *
 *  Provides parsing for the r attribute.
 *
 *  \sa WbItem
 */
class WbPolygon : public WbPolyline {
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbPolygon(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654007).*/
	virtual int type() const { return 87654007; };
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();
};

class WbText;

/*! \brief The visualized part of WbText.
 *  This class provides the QGraphicsItem for the WbText because multiple inheritance doesn't work
 *  when both WbItem and and QGraphicsTextItem inherit from QObject.
 *
 *  \sa WbItem
 */
class WbGraphicsTextItem : public QGraphicsTextItem  {
public:
	/*! \brief Constructor
	 *  Constructs a new visualized item for \a wbtext on \a scene.
	 */
	WbGraphicsTextItem(WbText* wbtext, QGraphicsScene* scene = 0);

protected:
	/*! \brief Constructs and popsup the context menu.*/
	virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	/*! \brief Causes the text to be checked for changes.*/
	virtual void focusOutEvent (QFocusEvent *);
	/*! \brief Makes the text editable.*/
	virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousMoveEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousReleaseEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);

private:
	/*! \brief A pointer to the "parent" WbItem.*/
	WbText* wbText_;
};

/*! \brief An item for an SVG text element.
 *  Inherits WbItem and implements WbGraphicsTextItem as a friend class.
 *
 *  Reimplements the methods dictated in WbItem's description.
 *
 *  Provides parsing for the text content of the text element.
 *
 *  \sa WbItem
 */
class WbText : public WbItem  {
	Q_OBJECT
	friend class WbGraphicsTextItem;
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbText(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);
	/*! \brief Destructor*/
	virtual ~WbText();

	/*! \brief Returns the type of the item (0x87654101).*/
	virtual int type() const { return 87654101; };
	/*! \brief Returns a QGraphicsItem* to self.*/
	virtual QGraphicsItem* graphicsItem() { return graphicsitem_; };
	/*! \brief Return the item as an SVG element.*/
	virtual QDomElement svg();
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();
	/*! \brief Checks if the text has changed and emits a signal accordingly.*/
	void checkTextChanges();

public slots:
	/*! \brief Popsup a font dialog and sets the selected font.*/
	void setFont();

private:
	/*! \brief The character data content of the element.*/
	QString text_;
	WbGraphicsTextItem* graphicsitem_;
};

/*! \brief An item for representing an SVG image element.
 *  Inherits QGraphicsPixmapItem and WbItem. 
 *
 *  Reimplements the methods dictated in WbItem's description.
 *
 *  Provides parsing for the 'points' attribute.
 *
 *  \sa WbItem
 */
class WbImage : public QGraphicsPixmapItem, public WbItem
{
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbImage(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654102).*/
	virtual int type() const { return 87654102; };
	/*! \brief Returns a QGraphicsItem* to self.*/
	virtual QGraphicsItem* graphicsItem() { return this; };
	/*! \brief Parses the item specific SVG properties and returns the list of changed attributes.*/
	virtual QList<QString> parseSvg(QDomElement &, bool emitChanges = false);
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();

protected:
	/*! \brief Constructs and popsup the default context menu.*/
	virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	/*! \brief Passes the event to WbItem::handleMousMoveEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousReleaseEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
};

/*! \brief An item for representing an SVG group element.
 *  Inherits QGraphicsGroupItem and WbItem. 
 *
 *  Reimplements the methods dictated in WbItem's description.
 *
 *  Note that calling svg() of this class won't return the child elements
 *
 *  \sa WbItem
 */
class WbGroup : public QGraphicsItem, public WbItem
{
public:
	/*! \brief Constructor
	 *  Constructs a new item with values \a id, \a index, \a parent and
	 *  \a scene with other attributes parsed from the SVG element \a svg.
	 */
	WbGroup(QDomElement &svg, const QString &id, const qreal &index, const QString &parent = "root", QGraphicsScene * scene = 0);

	/*! \brief Returns the type of the item (0x87654201).*/
	virtual int type() const { return 87654201; };
	/*! \brief Returns a QGraphicsItem* to self.*/
	virtual QGraphicsItem* graphicsItem() { return this; };
	/*! \brief Returns a deep copy of this object but without a scene association.*/
	virtual WbItem* clone();

	/*! \brief Paints the bounding rect if selected.*/
	virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	/*! \brief Returns the bounding rectangle containing all group member items.*/
	virtual QRectF boundingRect() const;

protected:
	/*! \brief Updates the bounding rectangle when children are added or removed.*/
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);

	/*! \brief Constructs and popsup the default context menu.*/
	virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *);
	/*! \brief Passes the event to WbItem::handleMousMoveEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);
	/*! \brief Passes the event to WbItem::handleMousReleaseEvent.
	 *  The event is also passed to QGraphicsItem::mouseMoveEvent if not accepted..
	 */
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);

private:
	void addToBoundingRect(const QRectF &itemRect);
	QRectF boundingRect_;
};

#endif
