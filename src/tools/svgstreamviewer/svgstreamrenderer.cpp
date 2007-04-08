/*
 * svgstreamrenderer.cpp - a renderer for changing SVG documents
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

#include "svgstreamrenderer.h"
#include <QDebug>
#define EMPTYSVG "<?xml version=\"1.0\" standalone=\"no\"?><svg viewBox=\"0 0 1000 1000\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.2\" baseProfile=\"tiny\"></svg>"

/*!
	Constructor: Stores and empty SVG tree.
*/
SvgStreamRenderer::SvgStreamRenderer( QObject * parent ) : QSvgRenderer( parent )
{
	doc.setContent(QString(EMPTYSVG));
}

/*!
	Constructor: Loads and stores content specified by \a filename.
*/
SvgStreamRenderer::SvgStreamRenderer( const QString & filename, QObject * parent ) : QSvgRenderer( filename, parent )
{
	store(filename);
}

/*!
	Constructor: Loads and stores content specified by \a filename.
*/
SvgStreamRenderer::SvgStreamRenderer( const QByteArray & contents, QObject * parent ) : QSvgRenderer( contents, parent )
{
	doc.setContent(contents);
}

/*!
	Loads and stores the content of \a contents overwriting everything.
*/
bool SvgStreamRenderer::load(const QString &filename) {
	if(QSvgRenderer::load(filename)) {
		if(store(filename)) {
			return true;
		} else {
			QSvgRenderer::load(QByteArray(EMPTYSVG));
		}
	}
	return false;
}

/*!
	Loads and stores the content of \a contents overwriting everything.
*/
bool SvgStreamRenderer::load(const QByteArray &contents) {
	if(QSvgRenderer::load(contents)) {
		if(doc.setContent(contents)) {
			return true;
		} else {
			QSvgRenderer::load(QByteArray(EMPTYSVG));
		}
	}
	return false;
}

/*!
	Converts QString to QDomElement and calls setElement(const QDomElement &element, bool forcenew).
*/
bool SvgStreamRenderer::setElement(const QString &element, bool forcenew)
{
	QDomDocument in;
//	QString * err = new QString(); // DEBUG
//	in.setContent(element, err);
	in.setContent(element);
//	qDebug() << err; // DEBUG
//	qDebug() << in.toString().; // DEBUG
	return setElement(in.documentElement(), forcenew);
}

/*!
	Replaces the element with tagname and id equal to what's given in \a element or appends \a element
	if element with such id doesn't exist or \a forcenew is true.
*/
bool SvgStreamRenderer::setElement(const QDomElement &element, bool forcenew)
{
	bool append = true;
	QString id = element.attribute("id");
	if(!forcenew && id.length() > 0) {
		QDomNode old;
		QDomNodeList children = doc.elementsByTagName(element.tagName());
		// Traverse index top to bottom. More likely to be faster assuming recent element are modified more often than old.
		for(int i=children.length()-1; i >= 0  && append; i--) {
			if(children.item(i).isElement())
				if(children.item(i).toElement().attribute("id") == id) {
					append=false;
					old=children.item(i);
				}
		}
		if(!append)
			doc.documentElement().replaceChild(element, old);
	}
	if(append)
		doc.documentElement().appendChild(element);
	if(load(doc.toByteArray())) {
		emit(repaintNeeded());
		return true;
	} else
		doc.documentElement().removeChild(element);
	return false;
}

/*!
	Change the stacking order of the element with \a id by \a n levels. Return the number of levels moved.
*/
int SvgStreamRenderer::moveElement(const QString &id, const int &n)
{
	int newIndex;
	QDomNodeList children = doc.documentElement().childNodes();
	// Traverse index top to bottom. More likely to be faster assuming recent element are modified more often than old ones.
	for(int i=children.length()-1; i >= 0; i--) {
		if(children.item(i).isElement()) {
			if(children.item(i).toElement().attribute("id") == id) {
				if( i+n >= (int)children.length() - 1) {
					newIndex = children.length();
					doc.documentElement().insertAfter(children.item(i), QDomNode());
				} else if( i+n <= 0) {
					// Accoring to Qt 4.1 API children.item(i+n) should return QDomNode() if i+n < 0 didn't work for me (Joonas).
					newIndex = 0;
					doc.documentElement().insertBefore(children.item(i), QDomNode());
				} else {
					newIndex = i+n;
					if(n < 0)
						doc.documentElement().insertBefore(children.item(i), children.item(i+n));
					else
						doc.documentElement().insertAfter(children.item(i), children.item(i+n));
				}
				if(load(doc.toByteArray())) {
					emit(repaintNeeded());
					// return the number of levels moved
					return newIndex-i;
				} else {
					// roll back changes in doc and return 0
					if(i < newIndex)
						doc.documentElement().insertBefore(children.item(newIndex), children.item(i));
					else
						doc.documentElement().insertAfter(children.item(newIndex), children.item(i));
					return 0;
				}
			}
		}
	}
	return 0;
}

/*!
	Deletes the element with \a id. 
*/
bool SvgStreamRenderer::removeElement(const QString &id)
{
	QDomNodeList children = doc.documentElement().childNodes();
	// Traverse index top to bottom. More likely to be faster assuming recent element are modified more often than old ones.
	for(int i=children.length()-1; i >= 0; i--) {
		if(children.item(i).isElement()) {
			if(children.item(i).toElement().attribute("id") == id) {
				// remove the element from doc
				QDomNode removedNode = doc.documentElement().removeChild(children.item(i));
				// re-render
				if(load(doc.toByteArray())) {
					emit(repaintNeeded());
					return true;
				} else {
					// insert the child back
					doc.documentElement().insertBefore(removedNode, children.item(i));
					return false;
				}
			}
		}
	}
	return false;
}

/*!
	Returns a clone of the element with \a id.
*/
QDomElement SvgStreamRenderer::element(QString id) const {
	qDebug() << id;
	QDomNodeList children = doc.documentElement().childNodes();
	// Traverse index top to bottom. More likely to be faster assuming recent element are modified more often than old ones.
	for(int i=children.length()-1; i >= 0; i--) {
		if(children.item(i).isElement()) {
			if(children.item(i).toElement().attribute("id") == id) {
				return children.item(i).cloneNode().toElement();
			}
		}
	}
	return QDomElement();
}

/*!
	Stores the content specified by \a filename in doc.
*/
bool SvgStreamRenderer::store(const QString &filename) {
	QFile file(filename);
	if( !file.open( QIODevice::ReadOnly ) )
		return false;
	if( !doc.setContent( &file ) )
	{
		file.close();
		return false;
	}
	file.close();
	emit(repaintNeeded());
	return true;
}
