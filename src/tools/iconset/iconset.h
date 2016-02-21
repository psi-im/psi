/*
 * iconset.h - various graphics handling classes
 * Copyright (C) 2001-2006  Justin Karneges, Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef ICONSET_H
#define ICONSET_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QRegExp>
#include <QString>
#include <QStringList>
#include <QSharedData>

#include <QPixmap>
#include <QImage>

class QIcon;
class QFileInfo;
class Anim;

class Impix
{
public:
	Impix();
	Impix(const QPixmap &);
	Impix(const QImage &);

	void unload();
	bool isNull() const;

	const QPixmap & pixmap() const;
	const QImage & image() const;
	void setPixmap(const QPixmap &);
	void setImage(const QImage &);

	operator const QPixmap &() const { return pixmap(); }
	operator const QImage &() const { return image(); }
	Impix & operator=(const QPixmap &from) { setPixmap(from); return *this; }
	Impix & operator=(const QImage &from) { setImage(from); return *this; }

	bool loadFromData(const QByteArray &);

private:
	class Private : public QSharedData
	{
	public:
		QPixmap* pixmap;
		QImage image;

		Private()
		{
			pixmap = 0;
		}

		Private(const Private& from)
			: QSharedData(from)
		{
			pixmap = from.pixmap ? new QPixmap(*from.pixmap) : 0;
			image  = from.image;
		}

		~Private()
		{
			unload();
		}

		void unload()
		{
			if (pixmap)
				delete pixmap;
			pixmap = 0;
			image  = QImage();
		}
	};

	mutable QSharedDataPointer<Private> d;
};

class PsiIcon : public QObject
{
	Q_OBJECT
public:
	PsiIcon();
	PsiIcon(const PsiIcon &);
	~PsiIcon();

	PsiIcon & operator= (const PsiIcon &);

	//!
	//! Returns impix().pixmap().
	// operator const QPixmap &() const { return impix().pixmap(); }

	//!
	//! Returns impix().image().
	// operator const QImage &() const { return impix().image(); }

	//!
	//! see icon().
	// operator const QIcon &() const { return icon(); }

	virtual bool isAnimated() const;
	virtual const QPixmap &pixmap() const;
	virtual const QImage &image() const;
	virtual const QIcon & icon() const;
#ifdef WEBKIT
	virtual const QByteArray & raw() const;
#endif

	virtual const Impix &impix() const;
	virtual const Impix &frameImpix() const;
	void setImpix(const Impix &, bool doDetach = true);

	const Anim *anim() const;
	void setAnim(const Anim &, bool doDetach = true);
	void removeAnim(bool doDetach = true);

	virtual int frameNumber() const;

	virtual const QString &name() const;
	void setName(const QString &);

	const QRegExp &regExp() const;
	void setRegExp(const QRegExp &);

	struct IconText {
		IconText(QString _lang, QString _text)
			: lang(_lang), text(_text)
		{}

		QString lang;
		QString text;
	};

	const QList<IconText> &text() const;
	void setText(const QList<IconText> &);

	QString defaultText() const;

	const QString &sound() const;
	void setSound(const QString &);

	bool blockSignals(bool);
	bool loadFromData(const QByteArray &, bool isAnimation);

	void stripFirstAnimFrame();

	virtual PsiIcon *copy() const;
	void detach();

signals:
	void pixmapChanged();
	void iconModified();

public slots:
	virtual void activated(bool playSound = true);	// it just has been inserted in the text, or now it's being displayed by
	                                                // some widget. icon should play sound and start animation

	virtual void stop();	// this icon is no more displaying. stop animation

public:
	class Private;
private:
	QSharedDataPointer<Private> d;
};

class Iconset
{
public:
	Iconset();
	Iconset(const Iconset &);
	~Iconset();

	Iconset &operator=(const Iconset &);
	Iconset &operator+=(const Iconset &);

	void clear();
	int count() const;

	bool load(const QString &dir);

	const PsiIcon *icon(const QString &) const;
	void setIcon(const QString &, const PsiIcon &);
	void removeIcon(const QString &);

	const QString &id() const;
	const QString &name() const;
	const QString &version() const;
	const QString &description() const;
	const QStringList &authors() const;
	const QString &creation() const;
	const QString &homeUrl() const;
	const int &iconSize() const;

	const QString &fileName() const;
	void setFileName(const QString &);

	void setInformation(const Iconset &from);

	const QHash<QString, QString> info() const;
	void setInfo(const QHash<QString, QString> &);

	QListIterator<PsiIcon *> iterator() const;

	void addToFactory() const;
	void removeFromFactory() const;

	static bool isSourceAllowed(const QFileInfo &fi);
	static void setSoundPrefs(QString unpackPath, QObject *receiver, const char *slot);

	Iconset copy() const;
	void detach();

private:
	class Private;
	QSharedDataPointer<Private> d;
};

class IconsetFactory
{
public:
	static void reset();

	static PsiIcon icon(const QString &name);
	static const QPixmap &iconPixmap(const QString &name);

	static const PsiIcon *iconPtr(const QString &name);
	static const QStringList icons();
#ifdef WEBKIT
	static const QByteArray raw(const QString &name);
#endif
};

#endif
