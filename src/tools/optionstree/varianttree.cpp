/*
 * varianttree.cpp - Tree structure for storing QVariants and comments
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2006  Kevin Smith
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

#include "varianttree.h"

#include <QColor>
#include <QDomDocument>
#include <QDomDocumentFragment>
#include <QDomElement>
#include <QKeySequence>
#include <QRect>
#include <QSize>
#include <QStringList>

// FIXME: Helpers from xmpp_xmlcommon.h would be very appropriate for
// void VariantTree::variantToElement(const QVariant& var, QDomElement& e)
// void VariantTree::elementToVariant(const QVariant& var, QDomElement& e)

QDomDocument *VariantTree::unknownsDoc=nullptr;

/**
 * Default Constructor
 */
VariantTree::VariantTree(QObject *parent)
    : QObject(parent)
{

}

/**
 * Default Destructor

 */
VariantTree::~VariantTree()
{
    foreach(VariantTree* vt, trees_.values()) {
        delete vt;
    }
}

/**
 * Split a @a node into local key and rest
 * @param node
 * @param key part of the @a node before first dot
 * @param rest part of the @a node after first dot
 * @return
 */
bool VariantTree::getKeyRest(const QString& node, QString &key, QString &rest)
{
    int idx = node.indexOf(QChar('.'));
    if (idx != -1) {
        key=node.left(idx);
        rest=node.mid(idx+1);
        return true;
    }
    return false;
}

bool VariantTree::isValidNodeName(const QString &name)
{
/* XML backend:
[4]       NameChar       ::=        Letter | Digit | '.' | '-' | '_' | ':' | CombiningChar | Extender
[5]       Name       ::=       (Letter | '_' | ':') (NameChar)*
but we don't want to have namespaces in the node names....

    but for now just allow ascii subset of this:
    */
    if (name.isEmpty()) return false;
    int len = name.length();
    QString other(".-_");
    QChar ch = name[0];
    if (!((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch=='_'))) return false;
    for (int i=1; i < len; i++) {
        ch = name[i];
        if (!((ch>='A' && ch<='Z')
            || (ch>='a' && ch<='z')
            || (other.contains(ch))
            || (ch >= '0' && ch <= '9'))) return false;
    }
    return true;
}

/**
 * Set @a node to value @a value
 */
void VariantTree::setValue(QString node, QVariant value)
{
    QString key, subnode;
    if (getKeyRest(node, key, subnode)) {
        //not this tier
        Q_ASSERT(isValidNodeName(key));
        if (!trees_.contains(key))
        {
            if (values_.contains(key))
            {
                qWarning("Error: Trying to add option node %s but it already exists as a value", qPrintable(key));
                return;
            }
            //create a new tier
            trees_[key]=new VariantTree(this);
        }
        //pass it down a level
        trees_[key]->setValue(subnode,value);
    } else {
        //this tier
        Q_ASSERT(isValidNodeName(node));
        if (trees_.contains(node))
        {
            qWarning("Error: Trying to add option value %s but it already exists as a subtree", qPrintable(node));
            return;
        }
        values_[node]=value;
    }
}

/**
 * Get value at @a node
 * @return the value of @a node if @a node exists, otherwise VariantTree::missingValue
 */
QVariant VariantTree::getValue(const QString& node) const
{
    QString key,subnode;
    if (getKeyRest(node, key, subnode)) {
        //not this tier
        auto it = trees_.constFind(key);
        if (it != trees_.constEnd())
        {
            return it.value()->getValue(subnode);
        }
    } else {
        //this tier
        auto it = values_.constFind(node);
        if (it != values_.constEnd())
            return it.value();
    }
    return missingValue;
}

bool VariantTree::remove(const QString &node, bool internal_nodes)
{
    QString key,subnode;
    if (getKeyRest(node, key, subnode)) {
        //not this tier
        if (trees_.contains(key)) {
            return trees_[key]->remove(subnode, internal_nodes);
        }
    } else {
        VariantTree *tree;
        //this tier
        if (values_.contains(node)) {
            values_.remove(node);
            return true;
        } else if (internal_nodes && (tree = trees_.take(node))) {
            delete tree;
            return true;
        }
    }
    return false;
}

/**
 * @return true iff the node @a node is an internal node (i.e. has a child tree).
 */
bool VariantTree::isInternalNode(QString node) const
{
    QString key,subnode;
    if (getKeyRest(node, key, subnode)) {
        //not this tier
        auto it = trees_.constFind(key);
        if (it != trees_.constEnd()) {
            return it.value()->isInternalNode(subnode);
        }
        qWarning("isInternalNode called on non existent node: %s", qPrintable(node));
        return false;
    } else {
        return trees_.contains(node);
    }
}

/**
 * \brief Sets the comment of the specified node.
 * \param name "Path" to the node
 * \param comment the comment to store
 */
void VariantTree::setComment(QString node, QString comment)
{
    if (node.contains(QChar('.')))
    {
        //not this tier
        QString key=node.left(node.indexOf(QChar('.')));
        QString subnode=node.remove(0,node.indexOf(QChar('.'))+1);
        Q_ASSERT(isValidNodeName(key));
        if (!trees_.contains(key))
        {
            if (values_.contains(key))
            {
                qWarning("Error: Trying to add option node %s but it already exists as a value", qPrintable(key));
                return;
            }
            //create a new tier
            trees_[key]=new VariantTree(this);
        }
        //pass it down a level
        trees_[key]->setComment(subnode,comment);
    } else {
        //this tier
        Q_ASSERT(isValidNodeName(node));
        comments_[node]=comment;
    }
}

/**
 * Returns the comment associated with a node.
 * (or a null QString if the node has no comment)
 */
QString VariantTree::getComment(QString node) const
{
    int dotIdx = node.indexOf(QChar('.'));
    if (dotIdx != -1)
    {
        //not this tier
        QString key=node.left(dotIdx);
        QString subnode=node.remove(0, dotIdx + 1);
        auto it = trees_.constFind(key);
        if (it != trees_.constEnd())
        {
            return it.value()->getComment(subnode);
        }
        return QString();
    }
    //this tier
    return comments_.value(node);
}

/**
 * Find all the children of the provided node \a node or, if no node is provided,
 * all children.
 *
 * \param direct only return direct children
 * \param internal_nodes include internal (non-final) nodes
 */
QStringList VariantTree::nodeChildren(const QString& node, bool direct, bool internal_nodes) const
{
    QStringList children;
    QString key = node;
    if (!node.isEmpty()) {
        // Go down further
        QString subnode;
        int dotIdx = node.indexOf(QChar('.'));
        if (dotIdx != -1) {
            key = node.left(dotIdx);
            subnode = node.right(node.length() - dotIdx - 1);
        }
        if (trees_.contains(key)) {
            children = trees_[key]->nodeChildren(subnode,direct,internal_nodes);
        }
    }
    else {
        // Current tree
        for (auto it = trees_.constBegin(); it != trees_.constEnd(); ++it) {
            if (internal_nodes)
                children << it.key();

            if (!direct)
                children += nodeChildren(it.key(),direct,internal_nodes);
        }

        for (auto it = values_.constBegin(); it != values_.constEnd(); ++it) {
            children << it.key();
        }
    }

    if (key.isEmpty()) {
        return children;
    }
    else {
        QStringList long_children;
        foreach (const QString &child, children) {
            QString long_child = QString("%1.%2").arg(key, child);
            long_children << long_child;
        }
        return long_children;
    }
}

/**
 *
 */
void VariantTree::toXml(QDomDocument &doc, QDomElement& ele) const
{
    // Subtrees
    for (auto it = trees_.constBegin(); it != trees_.constEnd(); ++it) {
        Q_ASSERT(!it.key().isEmpty());
        QDomElement nodeEle = doc.createElement(it.key());
        it.value()->toXml(doc, nodeEle);
        auto cit = comments_.constFind(it.key());
        if (cit != comments_.constEnd())
            nodeEle.setAttribute("comment",cit.value());
        ele.appendChild(nodeEle);
    }

    // Values
    for (auto it = values_.constBegin(); it != values_.constEnd(); ++it) {
        Q_ASSERT(!it.key().isEmpty());
        QVariant var = it.value();
        QDomElement valEle = doc.createElement(it.key());
        variantToElement(var,valEle);
        ele.appendChild(valEle);
        auto cit = comments_.constFind(it.key());
        if (cit != comments_.constEnd())
            valEle.setAttribute("comment",cit.value());
    }

    // unknown types passthrough
    foreach (QDomDocumentFragment df, unknowns_) {
        ele.appendChild(doc.importNode(df, true));
    }
}

/**
 *
 * @param ele
 */
void VariantTree::fromXml(const QDomElement &ele)
{
    QDomElement child = ele.firstChildElement();
    while (!child.isNull()) {
        bool isunknown=false;
        QString name = child.nodeName();
        Q_ASSERT(!name.isEmpty());
        if (!child.hasAttribute("type")) {
            // Subnode
            if ( !trees_.contains(name) )
                trees_[name] = new VariantTree(this);
            trees_[name]->fromXml(child);
        }
        else {
            // Value
            QVariant val;
            val = elementToVariant(child);
            if (val.isValid()) {
                values_[name] = val;
            } else {
                isunknown = true;
                if (!unknownsDoc) unknownsDoc = new QDomDocument();
                QDomDocumentFragment frag(unknownsDoc->createDocumentFragment());
                frag.appendChild(unknownsDoc->importNode(child, true));
                unknowns_[name] = frag;
            }
        }

        // Comments
        if (!isunknown && child.hasAttribute("comment")) {
            QString comment=child.attribute("comment");
            comments_[name]=comment;
        }
        child=child.nextSiblingElement();
    }
}

/**
 * Extracts a variant from an element.
 * The attribute of the element is used to determine the type.
 * The tagname of the element is ignored.
 */
QVariant VariantTree::elementToVariant(const QDomElement& e)
{
    // next declaration sorted from most popular
    static QString boolType(QString::fromLatin1("bool"));
    static QString stringType(QString::fromLatin1("QString"));
    static QString keyseqType(QString::fromLatin1("QKeySequence"));
    static QString intType(QString::fromLatin1("int"));
    static QString ulonglongType(QString::fromLatin1("qulonglong"));
    static QString colorType(QString::fromLatin1("QColor"));
    static QString stringListType(QString::fromLatin1("QStringList"));
    static QString rectType(QString::fromLatin1("QRect"));
    static QString variantListType(QString::fromLatin1("QVariantList"));
    static QString sizeType(QString::fromLatin1("QSize"));
    static QString variantMapType(QString::fromLatin1("QVariantMap"));
    static QString variantHashType(QString::fromLatin1("QVariantHash"));
    static QString byteArrayType(QString::fromLatin1("QByteArray"));

    static QString typeAttr(QString::fromLatin1("type"));

    QVariant value;
    QString type = e.attribute(typeAttr);

    { // let's start from basic most popular types
        QVariant::Type varianttype;
        bool known = true;

        if (type==boolType) {
            varianttype = QVariant::Bool;
        } else if (type == stringType) {
            varianttype = QVariant::String;
        } else if (type==intType) {
            varianttype = QVariant::Int;
        } else if (type==ulonglongType) {
            varianttype = QVariant::ULongLong;
        } else if (type == keyseqType) {
            varianttype = QVariant::KeySequence;
        } else if (type == colorType) {
            varianttype = QVariant::Color;
        } else {
            known = false;
        }

        if (known) {
            for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
                if ( node.isText() )
                    value=node.toText().data();
            }

            if (!value.isValid())
                value = QString("");

            value.convert(varianttype);
            return value;
        }
    }

    if (type == stringListType) {
        QStringList list;
        for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement e = node.toElement();
            if (!e.isNull() && e.tagName() == QLatin1String("item")) {
                list += e.text();
            }
        }
        value = list;
    }
    else if (type == variantListType) {
        QVariantList list;
        for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement e = node.toElement();
            if (!e.isNull() && e.tagName() == QLatin1String("item")) {
                QVariant v = elementToVariant(e);
                if (v.isValid())
                    list.append(v);
            }
        }
        value = list;
    }
    else if (type == variantMapType) {
        QVariantMap map;
        for (QDomElement ine = e.firstChildElement(); !ine.isNull(); ine = ine.nextSiblingElement()) {
            QVariant v = elementToVariant(ine);
            if (v.isValid())
                map.insert(ine.tagName(), v);
        }
        value = map;
    }
    else if (type == variantHashType) {
        QVariantHash map;
        for (QDomElement ine = e.firstChildElement(); !ine.isNull(); ine = ine.nextSiblingElement()) {
            QVariant v = elementToVariant(ine);
            if (v.isValid())
                map.insert(ine.tagName(), v);
        }
        value = map;
    }
    else if (type == sizeType) {
        int width = 0, height = 0;
        for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement e = node.toElement();
            if (!e.isNull()) {
                if (e.tagName() == QLatin1String("width")) {
                    width = e.text().toInt();
                }
                else if (e.tagName() == QLatin1String("height")) {
                    height = e.text().toInt();
                }
            }
        }
        value = QVariant(QSize(width,height));
    }
    else if (type == rectType) {
        int x = 0, y = 0, width = 0, height = 0;
        for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement e = node.toElement();
            if (!e.isNull()) {
                if (e.tagName() == QLatin1String("width")) {
                    width = e.text().toInt();
                }
                else if (e.tagName() == QLatin1String("height")) {
                    height = e.text().toInt();
                }
                else if (e.tagName() == QLatin1String("x")) {
                    x = e.text().toInt();
                }
                else if (e.tagName() == QLatin1String("y")) {
                    y = e.text().toInt();
                }
            }
        }
        value = QVariant(QRect(x,y,width,height));
    }
    else if (type == byteArrayType) {
        value = QByteArray();
        for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
            if (node.isText()) {
                value = QByteArray::fromBase64(node.toText().data().toLatin1());
                break;
            }
        }
    }

    return value;
}

/**
 * Modifies the element e to represent the variant var.
 * This method adds an attribute 'type' and contents to the element.
 */
void VariantTree::variantToElement(const QVariant& var, QDomElement& e)
{
    switch (var.type()) {
    case QVariant::List:
        foreach(QVariant v, var.toList()) {
            QDomElement item_element = e.ownerDocument().createElement(QLatin1String("item"));
            variantToElement(v,item_element);
            e.appendChild(item_element);
        }
        break;
    case QVariant::Map:
    {
        QVariantMap map = var.toMap();
        QVariantMap::ConstIterator it = map.constBegin();
        for (; it != map.constEnd(); ++it) {
            QDomElement item_element = e.ownerDocument().createElement(it.key());
            variantToElement(it.value(),item_element);
            e.appendChild(item_element);
        }
        break;
    }
    case QVariant::Hash:
    {
        QVariantHash map = var.toHash();
        QVariantHash::ConstIterator it = map.constBegin();
        for (; it != map.constEnd(); ++it) {
            QDomElement item_element = e.ownerDocument().createElement(it.key());
            variantToElement(it.value(),item_element);
            e.appendChild(item_element);
        }
        break;
    }
    case QVariant::StringList:
        foreach(const QString &s, var.toStringList()) {
            QDomElement item_element = e.ownerDocument().createElement(QLatin1String("item"));
            QDomText text = e.ownerDocument().createTextNode(s);
            item_element.appendChild(text);
            e.appendChild(item_element);
        }
        break;
    case QVariant::Size:
    {
        QSize size = var.toSize();
        QDomElement width_element = e.ownerDocument().createElement(QLatin1String("width"));
        width_element.appendChild(e.ownerDocument().createTextNode(QString::number(size.width())));
        e.appendChild(width_element);
        QDomElement height_element = e.ownerDocument().createElement(QLatin1String("height"));
        height_element.appendChild(e.ownerDocument().createTextNode(QString::number(size.height())));
        e.appendChild(height_element);
        break;
    }
    case QVariant::Rect:
    {
        QRect rect = var.toRect();
        QDomElement x_element = e.ownerDocument().createElement(QLatin1String("x"));
        x_element.appendChild(e.ownerDocument().createTextNode(QString::number(rect.x())));
        e.appendChild(x_element);
        QDomElement y_element = e.ownerDocument().createElement(QLatin1String("y"));
        y_element.appendChild(e.ownerDocument().createTextNode(QString::number(rect.y())));
        e.appendChild(y_element);
        QDomElement width_element = e.ownerDocument().createElement(QLatin1String("width"));
        width_element.appendChild(e.ownerDocument().createTextNode(QString::number(rect.width())));
        e.appendChild(width_element);
        QDomElement height_element = e.ownerDocument().createElement(QLatin1String("height"));
        height_element.appendChild(e.ownerDocument().createTextNode(QString::number(rect.height())));
        e.appendChild(height_element);
        break;
    }
    case QVariant::ByteArray:
    {
        QDomText text = e.ownerDocument().createTextNode(var.toByteArray().toBase64());
        e.appendChild(text);
        break;
    }
    case QVariant::KeySequence:
    {
        QKeySequence k = var.value<QKeySequence>();
        QDomText text = e.ownerDocument().createTextNode(k.toString());
        e.appendChild(text);
        break;
    }
    case QVariant::Color:
    { // save invalid colors as empty string
        if (var.value<QColor>().isValid()) {
            QDomText text = e.ownerDocument().createTextNode(var.toString());
            e.appendChild(text);
        }
        break;
    }
    default:
    {
        QDomText text = e.ownerDocument().createTextNode(var.toString());
        e.appendChild(text);
        break;
    }
    }

    e.setAttribute(QLatin1String("type"), var.typeName());
}

const QVariant VariantTree::missingValue=QVariant(QVariant::Invalid);
