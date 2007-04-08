/*
 * iconset.cpp - various graphics handling classes
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "iconset.h"
#include "zip.h"

#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <QIcon>
#include <QRegExp>
#include <Q3MimeSourceFactory>
#include <QDomDocument>
#include <QThread>

#include "anim.h"

// sound support
#ifndef NO_ICONSET_SOUND
#define ICONSET_SOUND
#endif

#ifdef ICONSET_SOUND
#	include <QDataStream>
#	include <qca_basic.h>
#endif

#include <QApplication>
#include <Q3Shared> // TODO FIXME: port all Q3Shared classes to QSharedData

static void moveToMainThread(QObject *obj)
{
	if (Anim::mainThread() && Anim::mainThread() != QThread::currentThread())
		obj->moveToThread(Anim::mainThread());
}

//----------------------------------------------------------------------------
// Impix
//----------------------------------------------------------------------------

/**
 * \class Impix iconset.h
 * \brief Combines a QPixmap and QImage into one class
 *
 * Normally, it is common to use QPixmap for all application graphics.
 * However, sometimes it is necessary to access pixel data, which means
 * a time-costly conversion to QImage.  Impix does this conversion on
 * construction, and keeps a copy of both a QPixmap and QImage for fast
 * access to each.  What you gain in speed you pay in memory, as an Impix
 * should occupy twice the space.
 *
 * An Impix can be conveniently created from either a QImage or QPixmap
 * source, and can be casted back to either type.
 *
 * You may also call unload() to free the image data.
 *
 * \code
 * Impix i = QImage("icon.png");
 * QLabel *iconLabel;
 * iconLabel->setPixmap(i.pixmap());
 * QImage image = i.image();
 * \endcode
 */

/**
 * \brief Construct a null Impix
 *
 * Constructs an Impix without any image data.
 */
Impix::Impix()
{
	d = new Private;
}

/**
 * \brief Construct an Impix based on a QPixmap
 *
 * Constructs an Impix by making a copy of a QPixmap and creating a QImage from it.
 * \param from - source QPixmap
 */
Impix::Impix(const QPixmap &from)
{
	d = new Private;
	*this = from;
}

/**
 * \brief Construct an Impix based on a QImage
 *
 * Constructs an Impix by making a copy of a QImage and creating a QPixmap from it.
 * \param from - source QImage
 */
Impix::Impix(const QImage &from)
{
	d = new Private;
	*this = from;
}

/**
 * Unloads image data, making it null.
 */
void Impix::unload()
{
	if ( isNull() )
		return;

	d->unload();
}

bool Impix::isNull() const
{
	return !d->image.isNull() || d->pixmap ? false: true;
}

const QPixmap & Impix::pixmap() const
{
	if (!d->pixmap) {
		d->pixmap = new QPixmap();
		if (!d->image.isNull())
			d->pixmap->convertFromImage(d->image);
	}
	return *d->pixmap;
}

const QImage & Impix::image() const
{
	if (d->image.isNull() && d->pixmap)
		d->image = d->pixmap->convertToImage();
	return d->image;
}

void Impix::setPixmap(const QPixmap &x)
{
	d->unload();
	d->pixmap = new QPixmap(x);
}

void Impix::setImage(const QImage &x)
{
	d->unload();
	d->image = x;
}

bool Impix::loadFromData(const QByteArray &ba)
{
	bool ret = false;

	QImage img;
	if ( img.loadFromData(ba) ) {
		setImage( img );
		ret = true;
	}

	return ret;
}

//----------------------------------------------------------------------------
// IconSharedObject
//----------------------------------------------------------------------------

class IconSharedObject : public QObject
{
	Q_OBJECT
public:
	IconSharedObject()
#ifdef ICONSET_SOUND
	: QObject(qApp, "IconSharedObject")
#endif
	{ }

	QString unpackPath;

signals:
	void playSound(QString);

private:
	friend class PsiIcon;
};

static IconSharedObject *iconSharedObject = 0;

//----------------------------------------------------------------------------
// PsiIcon
//----------------------------------------------------------------------------

/**
 * \class PsiIcon
 * \brief Can contain Anim and stuff
 *
 * This class can be used for storing application icons as well as emoticons
 * (it has special functions in order to do that).
 *
 * Icons can contain animation, associated sound files, its own names.
 *
 * For implementing emoticon functionality, PsiIcon can have associated text
 * values and QRegExp for easy searching.
 */

//! \if _hide_doc_
class PsiIcon::Private : public QObject, public Q3Shared
{
	Q_OBJECT
public:
	Private()
	{
		moveToMainThread(this);

		anim = 0;
		icon = 0;
		activatedCount = 0;
	}

	~Private()
	{
		unloadAnim();
		if ( icon )
			delete icon;
	}

	// copy all stuff, this constructor is called when detaching
	Private(const Private &from)
	: QObject(), Q3Shared()
	{
		moveToMainThread(this);

		name = from.name;
		regExp = from.regExp;
		text = from.text;
		sound = from.sound;
		impix = from.impix;
		anim = from.anim ? new Anim ( *from.anim ) : 0;
		icon = 0;
	}
	
	void unloadAnim()
	{
		if ( anim )
			delete anim;
		anim = 0;
	}
	
	void connectInstance(PsiIcon *icon)
	{
		connect(this, SIGNAL(pixmapChanged(const QPixmap &)), icon, SIGNAL(pixmapChanged(const QPixmap &)));
		connect(this, SIGNAL(iconModified(const QPixmap &)),  icon, SIGNAL(iconModified(const QPixmap &)));
	}
	
	void disconnectInstance(PsiIcon *icon)
	{
		disconnect(this, SIGNAL(pixmapChanged(const QPixmap &)), icon, SIGNAL(pixmapChanged(const QPixmap &)));
		disconnect(this, SIGNAL(iconModified(const QPixmap &)),  icon, SIGNAL(iconModified(const QPixmap &)));
	}

signals:
	void pixmapChanged(const QPixmap &);
	void iconModified(const QPixmap &);

public:
	const QPixmap &pixmap() const
	{
		if ( anim )
			return anim->framePixmap();
		return impix.pixmap();
	}

public slots:
	void animUpdate() { emit pixmapChanged( pixmap() ); }

public:
	QString name;
	QRegExp regExp;
	QHash<QString, QString> text;
	QString sound;

	Impix impix;
	Anim *anim;
	QIconSet *icon;

	int activatedCount;
	friend class PsiIcon;
};
//! \endif

/**
 * Constructs empty PsiIcon.
 */
PsiIcon::PsiIcon()
: QObject(0)
{
	moveToMainThread(this);

	d = new Private;
	d->connectInstance(this);
}

/**
 * Destroys PsiIcon.
 */
PsiIcon::~PsiIcon()
{
	if ( d->deref() )
		delete d;
}

/**
 * Creates new icon, that is a copy of \a from. Note, that if one icon will be changed,
 * other will be changed as well. (that's because image data is shared)
 */
PsiIcon::PsiIcon(const PsiIcon &from)
: QObject(0, 0)
{
	moveToMainThread(this);

	d = from.d;
	d->ref();
	d->connectInstance(this);
}

/**
 * Creates new icon, that is a copy of \a from. Note, that if one icon will be changed,
 * other will be changed as well. (that's because image data is shared)
 */
PsiIcon & PsiIcon::operator= (const PsiIcon &from)
{
	d->disconnectInstance(this);
	if ( d->deref() )
		delete d;

	d = from.d;
	d->ref();
	d->connectInstance(this);

	return *this;
}

PsiIcon *PsiIcon::copy() const
{
	PsiIcon *icon = new PsiIcon;
	delete icon->d;
	icon->d = new Private( *this->d );
	icon->d->connectInstance(icon);

	return icon;
}

void PsiIcon::detach()
{
	if ( d->count != 1 ) { // only if >1 reference
		PsiIcon *i = copy();
		*this = *i;
		delete i;
	}
}

/**
 * Returns \c true when icon contains animation.
 */
bool PsiIcon::isAnimated() const
{
	return d->anim != 0;
}

/**
 * Returns QPixmap of current frame.
 */
const QPixmap &PsiIcon::pixmap() const
{
	return d->pixmap();
}

/**
 * Returns QImage of current frame.
 */
const QImage &PsiIcon::image() const
{
	if ( d->anim )
		return d->anim->frameImage();
	return d->impix.image();
}

/**
 * Returns Impix of first animation frame.
 * \sa setImpix()
 */
const Impix &PsiIcon::impix() const
{
	return d->impix;
}

/**
 * Returns Impix of current animation frame.
 * \sa impix()
 */
const Impix &PsiIcon::frameImpix() const
{
	if ( d->anim )
		return d->anim->frameImpix();
	return d->impix;
}

/**
 * Returns QIconSet of first animation frame.
 * TODO: Add automatic greyscale icon generation.
 */
const QIcon &PsiIcon::icon() const
{
	if ( d->icon )
		return *d->icon;

	d->icon = new QIcon( d->impix.pixmap() );
	return *d->icon;
}

/**
 * Sets the PsiIcon impix to \a impix.
 * \sa impix()
 */
void PsiIcon::setImpix(const Impix &impix, bool doDetach)
{
	if ( doDetach )
		detach();

	d->impix = impix;
	emit d->pixmapChanged( pixmap() );
	emit d->iconModified( d->impix.pixmap() );
}

/**
 * Returns pointer to Anim object, or \a 0 if PsiIcon doesn't contain an animation.
 */
const Anim *PsiIcon::anim() const
{
	return d->anim;
}

/**
 * Sets the animation for icon to \a anim. Also sets Impix to be the first frame of animation.
 * If animation have less than two frames, it is deleted.
 * \sa anim()
 */
void PsiIcon::setAnim(const Anim &anim, bool doDetach)
{
	if ( doDetach )
		detach();

	d->unloadAnim();
	d->anim = new Anim(anim);

	if ( d->anim->numFrames() > 0 )
		setImpix( d->anim->frame(0) );

	if ( d->anim->numFrames() < 2 ) {
		delete d->anim;
		d->anim = 0;
	}

	if ( d->anim && d->activatedCount > 0 ) {
		d->activatedCount = 0;
		activated(false); // restart the animation, but don't play the sound
	}

	emit d->pixmapChanged( pixmap() );
	emit d->iconModified( d->impix.pixmap() );
}

/**
 * Removes animation from icon.
 * \sa setAnim()
 */
void PsiIcon::removeAnim(bool doDetach)
{
	if ( doDetach )
		detach();

	if ( !d->anim )
		return;

	d->activatedCount = 0;
	stop();

	delete d->anim;
	d->anim = 0;

	emit d->pixmapChanged( pixmap() );
	//emit d->iconModified( d->impix.pixmap() );
}

/**
 * Returns the number of current animation frame.
 * \sa setAnim()
 */
int PsiIcon::frameNumber() const
{
	if ( d->anim )
		return d->anim->frameNumber();

	return 0;
}

/**
 * Returns name of the PsiIcon.
 * \sa setName()
 */
const QString &PsiIcon::name() const
{
	return d->name;
}

/**
 * Sets the PsiIcon name to \a name
 * \sa name()
 */
void PsiIcon::setName(const QString &name)
{
	detach();

	d->name = name;
}

/**
 * Returns PsiIcon's QRegExp. It is used to store information for emoticons.
 * \sa setRegExp()
 */
const QRegExp &PsiIcon::regExp() const
{
	return d->regExp;
}

/**
 * Sets the PsiIcon QRegExp to \a regExp.
 * \sa regExp()
 * \sa text()
 */
void PsiIcon::setRegExp(const QRegExp &regExp)
{
	detach();

	d->regExp = regExp;
}

/**
 * Returns PsiIcon's text. It is used to store information for emoticons.
 * \sa setText()
 * \sa regExp()
 */
const QHash<QString, QString> &PsiIcon::text() const
{
	return d->text;
}

/**
 * Sets the PsiIcon text to \a t.
 * \sa text()
 */
void PsiIcon::setText(const QHash<QString, QString> &t)
{
	detach();

	d->text = t;
}

/**
 * Returns file name of associated sound.
 * \sa setSound()
 * \sa activated()
 */
const QString &PsiIcon::sound() const
{
	return d->sound;
}

/**
 * Sets the sound file name to be associated with this PsiIcon.
 * \sa sound()
 * \sa activated()
 */
void PsiIcon::setSound(const QString &sound)
{
	detach();

	d->sound = sound;
}

/**
 * Blocks the signals. See the Qt documentation for details.
 */
bool PsiIcon::blockSignals(bool b)
{
	return d->blockSignals(b);
}

/**
 * Initializes PsiIcon's Impix (or Anim, if \a isAnim equals \c true).
 * Iconset::load uses this function.
 */
bool PsiIcon::loadFromData(const QByteArray &ba, bool isAnim)
{
	detach();

	bool ret = false;
	if ( isAnim ) {
		Anim *anim = new Anim(ba);
		setAnim(*anim);
		ret = anim->numFrames() > 0;
		delete anim; // shared data rules ;)
	}

	if ( !ret && d->impix.loadFromData(ba) )
		ret = true;

	if ( ret ) {
		emit d->pixmapChanged( pixmap() );
		emit d->iconModified( d->impix.pixmap() );
	}

	return ret;
}

/**
 * You need to call this function, when PsiIcon is \e triggered, i.e. it is shown on screen
 * and it must start animation (if it has not animation, this function will do nothing).
 * When icon is no longer shown on screen you MUST call stop().
 * NOTE: For EACH activated() function call there must be associated stop() call, or the
 * animation will go crazy. You've been warned.
 * If \a playSound equals \c true, PsiIcon will play associated sound file.
 * \sa stop()
 */
void PsiIcon::activated(bool playSound)
{
	d->activatedCount++;

#ifdef ICONSET_SOUND
	if ( playSound && !d->sound.isNull() ) {
		if ( !iconSharedObject )
			iconSharedObject = new IconSharedObject();

		emit iconSharedObject->playSound(d->sound);
	}
#else
	Q_UNUSED(playSound);
	Q_UNUSED(iconSharedObject);
#endif

	if ( d->anim ) {
		d->anim->unpause();

		d->anim->disconnectUpdate (d, SLOT(animUpdate())); // ensure, that we're connected to signal exactly one time
		d->anim->connectUpdate (d, SLOT(animUpdate()));
	}
}

/**
 * You need to call this function when PsiIcon is no more shown on screen. It would save
 * processor time, if PsiIcon has animation.
 * NOTE: For EACH activated() function call there must be associated stop() call, or the
 * animation will go crazy. You've been warned.
 * \sa activated()
 */
void PsiIcon::stop()
{
	d->activatedCount--;

	if ( d->activatedCount <= 0 ) {
		d->activatedCount = 0;
		if ( d->anim ) {
			d->anim->pause();
			d->anim->restart();
		}
	}
}

/**
 * As the name says, this function removes the first animation frame. This is used to
 * create system Psi iconsets, where first frame is used separately for menus.
 */
void PsiIcon::stripFirstAnimFrame()
{
	detach();
	
	if ( d->anim )
		d->anim->stripFirstFrame();
}

//----------------------------------------------------------------------------
// IconsetFactory
//----------------------------------------------------------------------------

/**
 * \class IconsetFactory
 * \brief Class for easy application-wide PsiIcon searching
 *
 * You can add several Iconsets to IconsetFactory to use multiple Icons
 * application-wide with ease.
 *
 * You can reference Icons by their name from any place from your application.
 */

//! \if _hide_doc_
class IconsetFactoryPrivate
{
private:
	static QList<Iconset *> *iconsets;
	static QPixmap *emptyPixmap;

public:
	static void registerIconset(const Iconset *);
	static void unregisterIconset(const Iconset *);

public:
	static const PsiIcon *icon(const QString &name);

	friend class IconsetFactory;
};
//! \endif

QList<Iconset *> *IconsetFactoryPrivate::iconsets = 0;
QPixmap *IconsetFactoryPrivate::emptyPixmap = 0;

void IconsetFactoryPrivate::registerIconset(const Iconset *i)
{
	if ( !iconsets )
		iconsets = new QList<Iconset *>;

	if ( !iconsets->contains((Iconset *)i) ) {
		iconsets->append ( (Iconset *)i );
	}
}

void IconsetFactoryPrivate::unregisterIconset(const Iconset *i)
{
	if ( iconsets && iconsets->contains((Iconset *)i) ) {
		iconsets->remove ( (Iconset *)i );
	}

	if ( !iconsets || !iconsets->count() ) {
		if ( iconsets ) {
			delete iconsets;
			iconsets = 0;
		}

		if ( emptyPixmap ) {
			delete emptyPixmap;
			emptyPixmap = 0;
		}
	}
}

const PsiIcon *IconsetFactoryPrivate::icon(const QString &name)
{
	if ( !iconsets )
		return 0;

	const PsiIcon *i = 0;
	Iconset *iconset;
	foreach (iconset, *iconsets) {
		if ( iconset )
			i = iconset->icon(name);

		if ( i )
			break;
	}
	return i;
}

/**
 * Returns pointer to PsiIcon with name \a name, or \a 0 if PsiIcon with that name wasn't
 * found in IconsetFactory.
 */
const PsiIcon *IconsetFactory::iconPtr(const QString &name)
{
	const PsiIcon *i = IconsetFactoryPrivate::icon(name);
	if ( !i ) {
		qDebug("WARNING: IconsetFactory::icon(\"%s\"): icon not found", name.latin1());
	}
	return i;
}

/**
 * Returns PsiIcon with name \a name, or empty PsiIcon if PsiIcon with that name wasn't
 * found in IconsetFactory.
 */
PsiIcon IconsetFactory::icon(const QString &name)
{
	const PsiIcon *i = iconPtr(name);
	if ( i )
		return *i;
	return PsiIcon();
}

/**
 * Returns QPixmap of first animation frame of the specified PsiIcon, or the empty
 * QPixmap, if that PsiIcon wasn't found in IconsetFactory.
 * This function is faster than the call to IconsetFactory::icon() and cast to QPixmap,
 * because the intermediate PsiIcon object is not created and destroyed.
 */
const QPixmap &IconsetFactory::iconPixmap(const QString &name)
{
	const PsiIcon *i = iconPtr(name);
	if ( i )
		return i->impix().pixmap();

	if ( !IconsetFactoryPrivate::emptyPixmap )
		IconsetFactoryPrivate::emptyPixmap = new QPixmap();
	return *IconsetFactoryPrivate::emptyPixmap;
}

/**
 * Returns list of all PsiIcon names that are in IconsetFactory.
 */
const QStringList IconsetFactory::icons()
{
	QStringList list;
	
	Iconset *iconset;
	foreach (iconset, *IconsetFactoryPrivate::iconsets) {
		QListIterator<PsiIcon *> it = iconset->iterator();
		while ( it.hasNext() )
			list << it.next()->name();
	}

	return list;
}

//----------------------------------------------------------------------------
// Iconset
//----------------------------------------------------------------------------

/**
 * \class Iconset
 * \brief Class for easy PsiIcon grouping
 *
 * This class supports loading Icons from .zip arhives. It also provides additional
 * information: name(), authors(), version(), description() and creation() date.
 *
 * This class is very handy to manage emoticon sets as well as usual application icons.
 *
 * \sa IconsetFactory
 */

//! \if _hide_doc_
class Iconset::Private : public Q3Shared
{
private:
	void init()
	{
		name = "Unnamed";
		//version = "1.0";
		//description = "No description";
		//authors << "I. M. Anonymous";
		//creation = "1900-01-01";
		homeUrl = QString::null;
	}

public:
	QString name, version, description, creation, homeUrl, filename;
	QStringList authors;
	QHash<QString, PsiIcon *> dict; // unsorted hash for fast search
	QList<PsiIcon *> list;          // sorted list
	QHash<QString, QString> info;

public:
	Private()
	{
		init();
	}

	Private(const Private &from)
	: Q3Shared()
	{
		init();

		setInformation(from);

		QListIterator<PsiIcon *> it( from.list );
		while ( it.hasNext() ) {
			PsiIcon *icon = new PsiIcon(*it.next());
			append(icon->name(), icon);
		}
	}

	~Private()
	{
		clear();
	}

	void append(QString n, PsiIcon *icon)
	{
		// all PsiIcon names in Iconset must be unique
		if ( dict.contains(n) )
			remove(n);
			
		dict[n] = icon;
		list.append(icon);
	}

	void clear()
	{
		dict.clear();
		while ( !list.isEmpty() )
			delete list.takeFirst();
	}
	
	void remove(QString name)
	{
		if ( dict.contains(name) ) {
			PsiIcon *i = dict[name];
			dict.erase( dict.find(name) );
			list.removeAll(i);
			delete i;
		}
	}

	QByteArray loadData(const QString &fileName, const QString &dir)
	{
		QByteArray ba;

		QFileInfo fi(dir);
		if ( fi.isDir() ) {
			QFile file ( dir + "/" + fileName );
			file.open (IO_ReadOnly);

			ba = file.readAll();
		}
		else if ( fi.extension(false) == "jisp" || fi.extension(false) == "zip" ) {
			UnZip z(dir);
			if ( !z.open() )
				return ba;

			QString n = fi.baseName(true) + "/" + fileName;
			if ( !z.readFile(n, &ba) ) {
				n = "/" + fileName;
				z.readFile(n, &ba);
			}
		}

		return ba;
	}

	void loadMeta(const QDomElement &i, const QString &dir)
	{
		Q_UNUSED(dir);

		for (QDomNode node = i.firstChild(); !node.isNull(); node = node.nextSibling()) {
			QDomElement e = node.toElement();
			if( e.isNull() )
				continue;

			QString tag = e.tagName();
			if ( tag == "name" ) {
				name = e.text();
			}
			else if ( tag == "version" ) {
				version = e.text();
			}
			else if ( tag == "description" ) {
				description = e.text();
			}
			else if ( tag == "author" ) {
				QString n = e.text();
				QString tmp = "<br>&nbsp;&nbsp;";
				if ( !e.attribute("email").isEmpty() ) {
					QString s = e.attribute("email");
					n += tmp + QString("Email: <a href='mailto:%1'>%2</a>").arg( s ).arg( s );
				}
				if ( !e.attribute("jid").isEmpty() ) {
					QString s = e.attribute("jid");
					n += tmp + QString("JID: <a href='jabber:%1'>%2</a>").arg( s ).arg( s );
				}
				if ( !e.attribute("www").isEmpty() ) {
					QString s = e.attribute("www");
					n += tmp + QString("WWW: <a href='%1'>%2</a>").arg( s ).arg( s );
				}
				authors += n;
			}
			else if ( tag == "creation" ) {
				creation = e.text();
			}
			else if ( tag == "home" ) {
				homeUrl = e.text();
			}
		}
	}

	static int icon_counter; // used to give unique names to icons

	// will return 'true' when icon is loaded ok
	bool loadIcon(const QDomElement &i, const QString &dir)
	{
		PsiIcon icon;
		icon.blockSignals(true);

		QHash<QString, QString> text, graphic, sound, object;

		QString name;
		name.sprintf("icon_%04d", icon_counter++);
		bool isAnimated = false;
		bool isImage = false;

		for(QDomNode n = i.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement e = n.toElement();
			if ( e.isNull() )
				continue;

			QString tag = e.tagName();
			if ( tag == "text" ) {
				QString lang = e.attribute("xml:lang");
				if ( lang.isEmpty() )
					lang = ""; // otherwise there would be many warnings :-(
				text.insertMulti(lang, e.text());
			}
			else if ( tag == "object" ) {
				object[e.attribute("mime")] = e.text();
			}
			else if ( tag == "x" ) {
				QString attr = e.attribute("xmlns");
				if ( attr == "name" ) {
					name = e.text();
				}
				else if ( attr == "type" ) {
					if ( e.text() == "animation" )
						isAnimated = true;
					else if ( e.text() == "image" )
						isImage = true;
				}
			}
			// leaved for compatibility with old JEP
			else if ( tag == "graphic" ) {
				graphic[e.attribute("mime")] = e.text();
			}
			else if ( tag == "sound" ) {
				sound[e.attribute("mime")] = e.text();
			}
		}

		icon.setText(text);
		icon.setName(name);

		QStringList graphicMime, soundMime, animationMime;
		graphicMime << "image/png"; // first item have higher priority than latter
		//graphicMime << "video/x-mng"; // due to very serious issue in Qt 4.1.0, this format was disabled
		graphicMime << "image/gif";
		graphicMime << "image/x-xpm";
		graphicMime << "image/bmp";
		graphicMime << "image/jpeg";
		graphicMime << "image/svg+xml"; // TODO: untested

		soundMime << "audio/x-wav";
		soundMime << "audio/x-ogg";
		soundMime << "audio/x-mp3";
		soundMime << "audio/x-midi";

		// MIME-types, that support animations
		animationMime << "image/gif";
		//animationMime << "video/x-mng";

		if ( !object.isEmpty() ) {
			// fill the graphic & sound tables, if there are some
			// 'object' entries. inspect the supported mimetypes
			// and copy mime info and file path to 'graphic' and
			// 'sound' dictonaries.

			QStringList::Iterator it = graphicMime.begin();
			for ( ; it != graphicMime.end(); ++it)
				if ( object.contains(*it) && !object[*it].isNull() )
					graphic[*it] = object[*it];

			it = soundMime.begin();
			for ( ; it != soundMime.end(); ++it)
				if ( object.contains(*it) && !object[*it].isNull() )
					sound[*it] = object[*it];
		}

		bool loadSuccess = false;

		{
			QStringList::Iterator it = graphicMime.begin();
			for ( ; it != graphicMime.end(); ++it) {
				if ( graphic.contains(*it) && !graphic[*it].isNull() ) {
					// if format supports animations, then load graphic as animation, and
					// if there is only one frame, then later it would be converted to single Impix
					if ( !isAnimated && !isImage ) {
						QStringList::Iterator it2 = animationMime.begin();
						for ( ; it2 != animationMime.end(); ++it2)
							if ( *it == *it2 ) {
								isAnimated = true;
								break;
							}
					}

					if ( icon.loadFromData( loadData(graphic[*it], dir), isAnimated ) ) {
						loadSuccess = true;
						break;
					}
					else {
						qDebug("Iconset::load(): Couldn't load %s graphic for the %s icon for the %s iconset", (*it).latin1(), name.latin1(), this->name.latin1());
						loadSuccess = false;
					}
				}
			}
		}

		{
			QFileInfo fi(dir);
			QStringList::Iterator it = soundMime.begin();
			for ( ; it != soundMime.end(); ++it) {
				if ( sound.contains(*it) && !sound[*it].isNull() ) {
					if ( !fi.isDir() ) { // it is a .zip file then
#ifdef ICONSET_SOUND
						if ( !iconSharedObject )
							iconSharedObject = new IconSharedObject();

						QString path = iconSharedObject->unpackPath;
						if ( path.isEmpty() )
							break;

						if( !QCA::isSupported("sha1") )
							continue;

						QFileInfo ext(sound[*it]);
						path += "/" + QCA::SHA1().hashToString(QString(fi.absFilePath() + "/" + *sound[*it]).utf8()) + "." + ext.extension();

						QFile file ( path );
						file.open ( IO_WriteOnly );
						QDataStream out ( &file );

						QByteArray data = loadData(sound[*it], dir);
						out.writeRawBytes (data, data.size());

						icon.setSound ( path );
						break;
#endif
					}
					else {
						icon.setSound ( fi.absFilePath() + "/" + *sound[*it] );
						break;
					}
				}
			}
		}

		if ( text.count() )
		{	// construct RegExp
			QString regexp;
			
			QHashIterator<QString, QString> it( text);
			while ( it.hasNext() ) {
				it.next();
				
				if ( !regexp.isEmpty() )
					regexp += '|';

				regexp += QRegExp::escape(it.value());
			}
			
			// make sure there is some form of whitespace on at least one side of the text string
			//regexp = QString("(\\b(%1))|((%2)\\b)").arg(regexp).arg(regexp);
			icon.setRegExp ( QRegExp(regexp) );
		}

		icon.blockSignals(false);

		if ( loadSuccess )
			append( name, new PsiIcon(icon) );

		return loadSuccess;
	}

	// would return 'true' on success
	bool load(const QDomDocument &doc, const QString dir)
	{
		QDomElement base = doc.documentElement();
		if ( base.tagName() != "icondef" )
			return false;

		bool success = true;

		for(QDomNode node = base.firstChild(); !node.isNull(); node = node.nextSibling()) {
			QDomElement i = node.toElement();
			if( i.isNull() )
				continue;

			QString tag = i.tagName();
			if ( tag == "meta" ) {
				loadMeta (i, dir);
			}
			else if ( tag == "icon" ) {
				bool ret = loadIcon (i, dir);
				if ( !ret )
					success = false;
			}
			else if ( tag == "x" ) {
				info[i.attribute("xmlns")] = i.text();
			}
		}

		return success;
	}
	
	void setInformation(const Private &from) {
		name = from.name;
		version = from.version;
		description = from.description;
		creation = from.creation;
		homeUrl = from.homeUrl;
		filename = from.filename;
		authors = from.authors;
		info = from.info;
	}
};
//! \endif

int Iconset::Private::icon_counter = 0;

//static int iconset_counter = 0;

/**
 * Creates empty Iconset.
 */
Iconset::Iconset()
{
	//iconset_counter++;

	d = new Private;
}

/**
 * Creates shared copy of Iconset \a from.
 */
Iconset::Iconset(const Iconset &from)
{
	//iconset_counter++;

	d = from.d;
	d->ref();
}

/**
 * Destroys Iconset, and frees all allocated Icons.
 */
Iconset::~Iconset()
{
	if ( d->deref() )
		delete d;

	IconsetFactoryPrivate::unregisterIconset(this);
}

/**
 * Copies all Icons as well as additional information from Iconset \a from.
 */
Iconset &Iconset::operator=(const Iconset &from)
{
	if ( d->deref() )
		delete d;

	d = from.d;
	d->ref();

	return *this;
}

Iconset Iconset::copy() const
{
	Iconset is;
	delete is.d;
	is.d = new Private( *this->d );

	return is;
}

void Iconset::detach()
{
	if ( d->count > 1 )
		*this = copy();
}

/**
 * Appends icons from Iconset \a from to this Iconset.
 */
Iconset &Iconset::operator+=(const Iconset &i)
{
	detach();

	QListIterator<PsiIcon *> it( i.d->list );
	while ( it.hasNext() ) {
		PsiIcon *icon = new PsiIcon(*it.next());
		d->append( icon->name(), icon );
	}

	return *this;
}

/**
 * Frees all allocated Icons.
 */
void Iconset::clear()
{
	detach();

	d->clear();
}

/**
 * Returns the number of Icons in Iconset.
 */
int Iconset::count() const
{
	return d->list.count();
}

/**
 * Loads Icons and additional information from directory \a dir. Directory can usual directory,
 * or a .zip/.jisp archive. There must exist file named \c icondef.xml in that directory.
 */
bool Iconset::load(const QString &dir)
{
	detach();

	// make it run okay on windows 9.x (where the pixmap memory is limited)
	//QPixmap::Optimization optimization = QPixmap::defaultOptimization();
	//QPixmap::setDefaultOptimization( QPixmap::MemoryOptim );

	bool ret = false;

	QByteArray ba;
	ba = d->loadData ("icondef.xml", dir);
	if ( !ba.isEmpty() ) {
		QDomDocument doc;
		if ( doc.setContent(ba, false) ) {
			if ( d->load(doc, dir) ) {
				d->filename = dir;
				ret = true;
			}
		}
		else
			qWarning("Iconset::load(): Failed to load iconset: icondef.xml is invalid XML");
	}

	//QPixmap::setDefaultOptimization( optimization );

	return ret;
}

/**
 * Returns pointer to PsiIcon, if PsiIcon with name \a name was found in Iconset, or \a 0 otherwise.
 * \sa setIcon()
 */
const PsiIcon *Iconset::icon(const QString &name) const
{
	if ( !d || d->dict.isEmpty() )
		return 0;

	return d->dict[name];
}

/**
 * Appends PsiIcon to Iconset. If the PsiIcon with that name already exists, it is removed.
 */
void Iconset::setIcon(const QString &name, const PsiIcon &icon)
{
	detach();

	PsiIcon *newIcon = new PsiIcon(icon);
	
	d->remove(name);
	d->append( name, newIcon );
}

/**
 * Removes PsiIcon with the name \a name from Iconset.
 */
void Iconset::removeIcon(const QString &name)
{
	detach();

	d->remove(name);
}

/**
 * Returns the Iconset name.
 */
const QString &Iconset::name() const
{
	return d->name;
}

/**
 * Returns the Iconset version.
 */
const QString &Iconset::version() const
{
	return d->version;
}

/**
 * Returns the Iconset description string.
 */
const QString &Iconset::description() const
{
	return d->description;
}

/**
 * Returns the Iconset authors list.
 */
const QStringList &Iconset::authors() const
{
	return d->authors;
}

/**
 * Returns the Iconset creation date.
 */
const QString &Iconset::creation() const
{
	return d->creation;
}

/**
 * Returns the Iconsets' home URL.
 */
const QString &Iconset::homeUrl() const
{
	return d->homeUrl;
}

QListIterator<PsiIcon *> Iconset::iterator() const
{
	QListIterator<PsiIcon *> it( d->list );
	return it;
}

/**
 * Returns directory (or .zip/.jisp archive) name from which Iconset was loaded.
 */
const QString &Iconset::fileName() const
{
	return d->filename;
}

/**
 * Sets the Iconset directory (.zip archive) name.
 */
void Iconset::setFileName(const QString &f)
{
	d->filename = f;
}

/**
 * Sets the information (meta-data) of this iconset to the information from the given iconset.
 */
void Iconset::setInformation(const Iconset &from) {
	detach();
	d->setInformation( *(from.d) );
}

/**
 * Returns additional Iconset information.
 * \sa setInfo()
 */
const QHash<QString, QString> Iconset::info() const
{
	return d->info;
}

/**
 * Sets additional Iconset information.
 * \sa info()
 */
void Iconset::setInfo(const QHash<QString, QString> &i)
{
	d->info = i;
}

/**
 * Created Q3MimeSourceFactory with names of icons and their images.
 */
Q3MimeSourceFactory *Iconset::createMimeSourceFactory() const
{
	Q3MimeSourceFactory *m = new Q3MimeSourceFactory;

	PsiIcon *icon;
	foreach (icon, d->list)
		m->setImage(icon->name(), icon->image());

	return m;
}

/**
 * Adds Iconset to IconsetFactory.
 */
void Iconset::addToFactory() const
{
	IconsetFactoryPrivate::registerIconset(this);
}

/**
 * Removes Iconset from IconsetFactory.
 */
void Iconset::removeFromFactory() const
{
	IconsetFactoryPrivate::unregisterIconset(this);
}

/**
 * Use this function before creation of Iconsets and it will enable the
 * sound playing ability when Icons are activated().
 *
 * \a unpackPath is the path, to where the sound files will be unpacked, if
 * required (i.e. when sound is stored inside packed iconset.zip file). Unpacked
 * file name will be unique, and you can empty the unpack directory at the application
 * exit. Calling application MUST ensure that the \a unpackPath is already created.
 * If specified \a unpackPath is empty ("" or QString::null) then sounds from archives
 * will not be unpacked, and only sounds from already unpacked iconsets (that are not
 * stored in archives) will be played.
 * \a receiver and \a slot are used to specify the object that will be connected to
 * the signal 'playSound(QString fileName)'. Slot should play the specified sound
 * file.
 */
void Iconset::setSoundPrefs(QString unpackPath, QObject *receiver, const char *slot)
{
#ifdef ICONSET_SOUND
	if ( !iconSharedObject )
		iconSharedObject = new IconSharedObject();

	iconSharedObject->unpackPath = unpackPath;
	QObject::connect(iconSharedObject, SIGNAL(playSound(QString)), receiver, slot);
#else
	Q_UNUSED(unpackPath);
	Q_UNUSED(receiver);
	Q_UNUSED(slot);
#endif
}

#include "iconset.moc"
