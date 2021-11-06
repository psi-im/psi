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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

// sound support
#ifndef NO_ICONSET_SOUND
#define ICONSET_SOUND
#endif

#ifndef NO_ICONSET_ZIP
#define ICONSET_ZIP
#endif

#include "iconset.h"

#include "anim.h"
#ifdef ICONSET_ZIP
#include "zip/zip.h"
#endif
#include "svgiconengine.h"

#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QIconEngine>
#include <QLocale>
#include <QObject>
#include <QPainter>
#include <QRegExp>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QSvgRenderer>
#include <QTextCodec>
#include <QThread>
#include <QTimer>
#ifdef ICONSET_SOUND
#include <QDataStream>
#include <qca_basic.h>
#endif

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
Impix::Impix() { d = new Private; }

/**
 * \brief Construct an Impix based on a QPixmap
 *
 * Constructs an Impix by making a copy of a QPixmap and creating a QImage from it.
 * \param from - source QPixmap
 */
Impix::Impix(const QPixmap &from)
{
    d     = new Private;
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
    d     = new Private;
    *this = from;
}

/**
 * Unloads image data, making it null.
 */
void Impix::unload()
{
    if (isNull()) {
        return;
    }

    d->unload();
}

bool Impix::isNull() const { return !(!d->image.isNull() || d->pixmap); }

const QPixmap &Impix::pixmap() const
{
    if (!d->pixmap) {
        d->pixmap = new QPixmap();
        if (!d->image.isNull()) {
            *d->pixmap = QPixmap::fromImage(d->image);
        }
    }
    return *d->pixmap;
}

const QImage &Impix::image() const
{
    if (d->image.isNull() && d->pixmap) {
        d->image = d->pixmap->toImage();
    }
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

QSize Impix::size() const
{
    if (d->pixmap)
        return d->pixmap->size();
    if (!d->image.isNull())
        return d->image.size();
    return {};
}

bool Impix::loadFromData(const QByteArray &ba)
{
    bool ret = false;

    delete d->pixmap;
    d->pixmap = nullptr;

    QImage img;
    if (img.loadFromData(ba)) {
        Q_ASSERT(img.width() > 0);
        Q_ASSERT(img.height() > 0);
        setImage(img);
        ret = true;
    }

    Q_ASSERT(ret);
    return ret;
}

//----------------------------------------------------------------------------
// IconSharedObject
//----------------------------------------------------------------------------

class IconSharedObject : public QObject {
    Q_OBJECT
public:
    IconSharedObject()
#ifdef ICONSET_SOUND
        :
        QObject(qApp)
#endif
    {
        setObjectName("IconSharedObject");
    }

    QString unpackPath;

signals:
    void playSound(QString);

private:
    friend class PsiIcon;
};

static IconSharedObject *iconSharedObject = nullptr;

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
class PsiIcon::Private : public QObject, public QSharedData {
    Q_OBJECT
public:
    Private()
    {
        moveToMainThread(this);

        icon           = nullptr;
        activatedCount = 0;
    }

    ~Private()
    {
        anim.reset();
        if (icon) {
            delete icon;
        }
    }

    // copy all stuff, this constructor is called when detaching
    Private(const Private &from) : QObject(), QSharedData()
    {
        moveToMainThread(this);

        name        = from.name;
        mime        = from.mime;
        regExp      = from.regExp;
        text        = from.text;
        sound       = from.sound;
        impix       = from.impix;
        rawData     = from.rawData;
        scalable    = from.scalable;
        svgRenderer = from.svgRenderer;
        anim.reset(from.anim ? new Anim(*from.anim) : nullptr);
        icon           = nullptr;
        activatedCount = from.activatedCount;
    }

    void connectInstance(PsiIcon *icon)
    {
        connect(this, SIGNAL(pixmapChanged()), icon, SIGNAL(pixmapChanged()));
        connect(this, SIGNAL(iconModified()), icon, SIGNAL(iconModified()));
    }

    void disconnectInstance(PsiIcon *icon)
    {
        disconnect(this, SIGNAL(pixmapChanged()), icon, SIGNAL(pixmapChanged()));
        disconnect(this, SIGNAL(iconModified()), icon, SIGNAL(iconModified()));
    }

signals:
    void pixmapChanged();
    void iconModified();

public:
    QPixmap pixmap(const QSize &desiredSize = QSize()) const
    {
        if (svgRenderer) {
            QSize   sz = desiredSize.isEmpty() ? svgRenderer->defaultSize()
                                               : svgRenderer->defaultSize().scaled(desiredSize, Qt::KeepAspectRatio);
            QPixmap pix(sz);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            svgRenderer->render(&p);
            return pix;
        }

        QPixmap pix = anim ? anim->framePixmap() : impix.pixmap();
        if (scalable)
            return pix.scaled(desiredSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return pix;
    }

public slots:
    void animUpdate() { emit pixmapChanged(); }

public:
    QString         name;
    QRegExp         regExp;
    QList<IconText> text;
    QString         sound;
    QString         mime;

    Impix                         impix;
    std::unique_ptr<Anim>         anim;
    std::shared_ptr<QSvgRenderer> svgRenderer;
    QIcon *                       icon = nullptr;
    mutable QByteArray            rawData;
    bool                          scalable = false;

    int activatedCount = 0;
    friend class PsiIcon;
};
//! \endif

/**
 * Constructs empty PsiIcon.
 */
PsiIcon::PsiIcon() : QObject(nullptr)
{
    moveToMainThread(this);

    d = new Private;
    d->connectInstance(this);
}

/**
 * Destroys PsiIcon.
 */
PsiIcon::~PsiIcon() { }

/**
 * Creates new icon, that is a copy of \a from. Note, that if one icon will be changed,
 * other will be changed as well. (that's because image data is shared)
 */
PsiIcon::PsiIcon(const PsiIcon &from) : QObject(nullptr), d(from.d)
{
    moveToMainThread(this);

    d->connectInstance(this);
}

/**
 * Creates new icon, that is a copy of \a from. Note, that if one icon will be changed,
 * other will be changed as well. (that's because image data is shared)
 */
PsiIcon &PsiIcon::operator=(const PsiIcon &from)
{
    d->disconnectInstance(this);
    d = from.d;
    d->connectInstance(this);

    return *this;
}

PsiIcon *PsiIcon::copy() const
{
    PsiIcon *icon = new PsiIcon;
    icon->d       = new Private(*this->d.data());
    icon->d->connectInstance(icon);

    return icon;
}

void PsiIcon::detach() { d.detach(); }

/**
 * Returns \c true when icon contains animation.
 */
bool PsiIcon::isAnimated() const { return d->anim != nullptr; }

/**
 * Returns QPixmap of current frame.
 */
QPixmap PsiIcon::pixmap(const QSize &desiredSize) const { return d->pixmap(desiredSize); }

/**
 * Returns QImage of current frame.
 */
QImage PsiIcon::image(const QSize &desiredSize) const
{
    if (d->anim) {
        return d->anim->frameImage();
    }

    auto img = d->impix.image();
    if (d->svgRenderer) {
        QSize sz = d->svgRenderer->defaultSize().scaled(desiredSize, Qt::KeepAspectRatio);
        if (!img.isNull() && img.size() == sz)
            return img;
        img = QImage(sz, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        d->svgRenderer->render(&p);
        // d->impix.setImage(img); // we need some mutable member to chache this.
    }
    return img;
}

/**
 * Returns Impix of first animation frame.
 * \sa setImpix()
 */
const Impix &PsiIcon::impix() const { return d->impix; }

/**
 * Returns Impix of current animation frame.
 * \sa impix()
 */
const Impix &PsiIcon::frameImpix() const
{
    if (d->anim) {
        return d->anim->frameImpix();
    }
    return d->impix;
}

/**
 * Returns QIcon of first animation frame.
 * TODO: Add automatic greyscale icon generation.
 */
QIcon PsiIcon::icon() const
{
    if (d->icon) {
        return *d->icon;
    }

    if (d->svgRenderer) {
        auto eng = new SvgIconEngine(d->name, d->svgRenderer);
        return QIcon(eng);
    }
    const_cast<Private *>(d.data())->icon = new QIcon(d->impix.pixmap());
    return *d->icon;
}

/**
 * Returns original image data
 */
const QByteArray &PsiIcon::raw() const
{
    if (d->rawData.isEmpty()) {
        QPixmap pix = impix().pixmap();
        if (!pix.isNull()) {
            QBuffer buffer(&d->rawData);
            buffer.open(QIODevice::WriteOnly);
            pix.save(&buffer, "PNG");
        }
    }
    return d->rawData;
}

QSize PsiIcon::size(const QSize &desiredSize) const
{
    if (d->scalable) {
        QSize origSize = d->svgRenderer ? d->svgRenderer->defaultSize() : d->impix.size();
        if (!desiredSize.width() && !desiredSize.height())
            return origSize;
        if (!desiredSize.width()) {
            return origSize.scaled(origSize.width(), desiredSize.height(), Qt::KeepAspectRatio);
        }
        if (!desiredSize.height()) {
            return origSize.scaled(desiredSize.width(), origSize.height(), Qt::KeepAspectRatio);
        }
        return origSize.scaled(desiredSize, Qt::KeepAspectRatio);
    }
    return d->impix.size();
}

bool PsiIcon::isScalable() const { return d->scalable; }

const QString &PsiIcon::mimeType() const { return d->mime; }

/**
 * Sets the PsiIcon impix to \a impix.
 * \sa impix()
 */
void PsiIcon::setImpix(const Impix &impix, bool doDetach)
{
    if (doDetach) {
        detach();
    }

    d->impix = impix;
    if (d->icon) {
        delete d->icon;
        d->icon = nullptr;
    }

    emit d->pixmapChanged();
    emit d->iconModified();
}

/**
 * Returns pointer to Anim object, or \a 0 if PsiIcon doesn't contain an animation.
 */
const Anim *PsiIcon::anim() const { return d->anim.get(); }

/**
 * Sets the animation for icon to \a anim. Also sets Impix to be the first frame of animation.
 * If animation have less than two frames, it is deleted.
 * \sa anim()
 */
void PsiIcon::setAnim(const Anim &anim, bool doDetach)
{
    if (doDetach) {
        detach();
    }

    d->anim.reset(new Anim(anim));

    if (d->anim->numFrames() > 0) {
        setImpix(d->anim->frame(0));
    }

    if (d->anim->numFrames() < 2) {
        d->anim.reset();
    }

    if (d->anim && d->activatedCount > 0) {
        d->activatedCount = 0;
        activated(false); // restart the animation, but don't play the sound
    }

    emit d->pixmapChanged();
    emit d->iconModified();
}

/**
 * Removes animation from icon.
 * \sa setAnim()
 */
void PsiIcon::removeAnim(bool doDetach)
{
    if (doDetach) {
        detach();
    }

    if (!d->anim) {
        return;
    }

    d->activatedCount = 0;
    stop();

    d->anim.reset();

    emit d->pixmapChanged();
    // emit d->iconModified();
}

/**
 * Returns the number of current animation frame.
 * \sa setAnim()
 */
int PsiIcon::frameNumber() const
{
    if (d->anim) {
        return d->anim->frameNumber();
    }

    return 0;
}

/**
 * Returns name of the PsiIcon.
 * \sa setName()
 */
const QString &PsiIcon::name() const { return d->name; }

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
const QRegExp &PsiIcon::regExp() const { return d->regExp; }

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
const QList<PsiIcon::IconText> &PsiIcon::text() const { return d->text; }

/**
 * Sets the PsiIcon text to \a t.
 * \sa text()
 */
void PsiIcon::setText(const QList<PsiIcon::IconText> &t)
{
    detach();

    d->text = t;
}

/**
 * Returns default icon text that could be used as a default value
 * for e.g. emoticon selector.
 */
QString PsiIcon::defaultText() const
{
    if (text().isEmpty()) {
        return QString();
    }

    // first, try to get the text by priorities
    QStringList lang { QLocale().name().section('_', 0, 0), // most prioritent, is the local language
                       "",                                  // and then the language without name goes (international?)
                       "en" };                              // then real English

    QString               str;
    QStringList::Iterator it = lang.begin();
    for (; it != lang.end(); ++it) {
        for (const IconText &t : text()) {
            if (t.lang == *it) {
                str = t.text;
                break;
            }
        }
    }

    // if all fails, just get the first text
    if (str.isEmpty()) {
        for (const IconText &t : text()) {
            if (!t.text.isEmpty()) {
                str = t.text;
                break;
            }
        }
    }

    return str;
}

/**
 * Returns file name of associated sound.
 * \sa setSound()
 * \sa activated()
 */
const QString &PsiIcon::sound() const { return d->sound; }

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
bool PsiIcon::blockSignals(bool b) { return d->blockSignals(b); }

/**
 * Initializes PsiIcon's Impix (or Anim, if \a isAnim equals \c true).
 * Iconset::load uses this function.
 */
bool PsiIcon::loadFromData(const QString &mime, const QByteArray &ba, bool isAnim, bool isScalable)
{
    bool ret = false;
    if (ba.isEmpty())
        return ret;

    detach();
    d->rawData     = ba;
    d->scalable    = isScalable;
    d->svgRenderer = nullptr;
    if (d->scalable) {
        d->svgRenderer = std::make_shared<QSvgRenderer>(ba);
        if (!d->svgRenderer->isValid()) {
            d->svgRenderer.reset();
            d->svgRenderer = nullptr;
        }
    }
    if (d->svgRenderer) {
        ret = true;
    } else {
        if (isAnim) {
            Anim *anim = new Anim(ba);
            setAnim(*anim);
            ret = anim->numFrames() > 0;
            delete anim; // shared data rules ;)
        }

        if (!ret && d->impix.loadFromData(ba))
            ret = true;
    }

    if (ret) {
        d->mime = mime;
        emit d->pixmapChanged();
        emit d->iconModified();
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
    if (playSound && !d->sound.isNull()) {
        if (!iconSharedObject) {
            iconSharedObject = new IconSharedObject();
        }

        emit iconSharedObject->playSound(d->sound);
    }
#else
    Q_UNUSED(playSound);
    Q_UNUSED(iconSharedObject);
#endif

    if (d->anim) {
        d->anim->unpause();

        d->anim->disconnectUpdate(d, SLOT(animUpdate())); // ensure, that we're connected to signal exactly one time
        d->anim->connectUpdate(d, SLOT(animUpdate()));
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

    if (d->activatedCount <= 0) {
        d->activatedCount = 0;
        if (d->anim) {
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

    if (d->anim) {
        d->anim->stripFirstFrame();
    }
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
class IconsetFactoryPrivate : public QObject {
private:
    IconsetFactoryPrivate() : QObject(QCoreApplication::instance()), iconsets_(nullptr), emptyPixmap_(nullptr) { }

    ~IconsetFactoryPrivate()
    {
        if (iconsets_) {
            while (!iconsets_->empty())
                delete iconsets_->takeFirst();
            delete iconsets_;
            iconsets_ = nullptr;
        }

        if (emptyPixmap_) {
            delete emptyPixmap_;
            emptyPixmap_ = nullptr;
        }
    }

    static IconsetFactoryPrivate *instance_;
    QList<Iconset *> *            iconsets_;
    mutable QPixmap *             emptyPixmap_;

public:
    const QPixmap &emptyPixmap() const
    {
        if (!emptyPixmap_) {
            emptyPixmap_ = new QPixmap();
        }
        return *emptyPixmap_;
    }

    const QStringList icons() const
    {
        QStringList list;

        for (const Iconset *iconset : qAsConst(*iconsets_)) {
            QListIterator<PsiIcon *> it = iconset->iterator();
            while (it.hasNext()) {
                list << it.next()->name();
            }
        }

        return list;
    }

    void registerIconset(const Iconset *);
    void unregisterIconset(const Iconset *);

public:
    static IconsetFactoryPrivate *instance()
    {
        if (!instance_) {
            instance_ = new IconsetFactoryPrivate();
        }
        return instance_;
    }

    const PsiIcon *icon(const QString &name) const;

    static void reset()
    {
        delete instance_;
        instance_ = nullptr;
    }
};
//! \endif

IconsetFactoryPrivate *IconsetFactoryPrivate::instance_ = nullptr;

void IconsetFactoryPrivate::registerIconset(const Iconset *i)
{
    if (!iconsets_) {
        iconsets_ = new QList<Iconset *>;
    }

    if (!iconsets_->contains(const_cast<Iconset *>(i))) {
        iconsets_->append(const_cast<Iconset *>(i));
    }
}

void IconsetFactoryPrivate::unregisterIconset(const Iconset *i)
{
    if (iconsets_ && iconsets_->contains(const_cast<Iconset *>(i))) {
        iconsets_->removeAll(const_cast<Iconset *>(i));
    }
}

const PsiIcon *IconsetFactoryPrivate::icon(const QString &name) const
{
    if (!iconsets_) {
        return nullptr;
    }

    const PsiIcon *out = nullptr;
    for (const Iconset *const iconset : qAsConst(*iconsets_)) {
        if (iconset) {
            out = iconset->icon(name);
        }

        if (out) {
            break;
        }
    }
    return out;
}

void IconsetFactory::reset() { IconsetFactoryPrivate::reset(); }

/**
 * Returns pointer to PsiIcon with name \a name, or \a 0 if PsiIcon with that name wasn't
 * found in IconsetFactory.
 */
const PsiIcon *IconsetFactory::iconPtr(const QString &name)
{
    if (name.isEmpty()) {
        return nullptr;
    }

    const PsiIcon *i = IconsetFactoryPrivate::instance()->icon(name);
    if (!i) {
        qDebug("WARNING: IconsetFactory::icon(\"%s\"): icon not found", qPrintable(name));
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
    if (i) {
        return *i;
    }
    return PsiIcon();
}

/**
 * Returns QPixmap of first animation frame of the specified PsiIcon, or the empty
 * QPixmap, if that PsiIcon wasn't found in IconsetFactory.
 * This function is faster than the call to IconsetFactory::icon() and cast to QPixmap,
 * because the intermediate PsiIcon object is not created and destroyed.
 */
QPixmap IconsetFactory::iconPixmap(const QString &name, const QSize desiredSize)
{
    const PsiIcon *i = iconPtr(name);
    if (i) {
        if (!desiredSize.isEmpty() && i->isScalable())
            return i->pixmap(desiredSize);
        return i->impix().pixmap();
    }

    return IconsetFactoryPrivate::instance()->emptyPixmap();
}

/**
 * Returns list of all PsiIcon names that are in IconsetFactory.
 */
const QStringList IconsetFactory::icons() { return IconsetFactoryPrivate::instance()->icons(); }

/**
 * Returs image raw data aka original image
 */
const QByteArray IconsetFactory::raw(const QString &name)
{
    const PsiIcon *i = iconPtr(name);
    if (i) {
        return i->raw();
    }
    return QByteArray();
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
class Iconset::Private : public QSharedData {
private:
    void init()
    {
        name = "Unnamed";
        // version = "1.0";
        // description = "No description";
        // authors << "I. M. Anonymous";
        // creation = "1900-01-01";
        homeUrl   = QString();
        iconSize_ = 16;
    }

public:
    QString                    id, name, version, description, creation, homeUrl, filename;
    QStringList                authors;
    QHash<QString, PsiIcon *>  dict; // unsorted hash for fast search
    QList<PsiIcon *>           list; // sorted list
    QHash<QString, QString>    info;
    int                        iconSize_;
    QHash<QString, QByteArray> zipCache;

public:
    Private() { init(); }

    Private(const Private &from) : QSharedData()
    {
        init();

        setInformation(from);

        QListIterator<PsiIcon *> it(from.list);
        while (it.hasNext()) {
            PsiIcon *icon = new PsiIcon(*it.next());
            append(icon->name(), icon);
        }
    }

    ~Private() { clear(); }

    void append(QString n, PsiIcon *icon)
    {
        // all PsiIcon names in Iconset must be unique
        remove(dict.find(n));
        dict[n] = icon;
        list.append(icon);
    }

    void clear()
    {
        dict.clear();
        while (!list.isEmpty()) {
            delete list.takeFirst();
        }
    }

    void remove(QString name) { remove(dict.find(name)); }

    void remove(QHash<QString, PsiIcon *>::iterator it)
    {
        if (it != dict.end()) {
            PsiIcon *i = it.value();
            dict.erase(it);
            list.removeAll(i);
            delete i;
        }
    }

    QByteArray loadData(const QString &fileName, const QString &dir)
    {
        QByteArray ba;

        QFileInfo fi(dir);
        if (!Iconset::isSourceAllowed(fi)) {
            qWarning("%s is invalid icons source", qPrintable(dir));
            return ba;
        }
        if (fi.isDir()) {
            QFile file(dir + '/' + fileName);
            if (!file.open(QIODevice::ReadOnly)) {
                qWarning("%s is not found in %s", qPrintable(fileName), qPrintable(dir));
                return ba;
            }

            ba = file.readAll();
        }
#ifdef ICONSET_ZIP
        else { // else its zip or jisp file
            UnZip z(dir);
            if (zipCache.isEmpty())
                zipCache = z.unpackAll();
            ba = zipCache.value(fi.completeBaseName() + '/' + fileName);
        }
#endif

        return ba;
    }

    void loadMeta(const QDomElement &i, const QString &dir)
    {
        Q_UNUSED(dir);

        iconSize_ = 16;

        for (QDomNode node = i.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement e = node.toElement();
            if (e.isNull()) {
                continue;
            }

            QString tag = e.tagName();
            if (tag == "name") {
                name = e.text();
            } else if (tag == "size") {
                QString str = e.text();
                iconSize_   = str.toInt();
            } else if (tag == "version") {
                version = e.text();
            } else if (tag == "description") {
                description = e.text();
            } else if (tag == "author") {
                QString n   = e.text();
                QString tmp = "<br>&nbsp;&nbsp;";
                if (!e.attribute("email").isEmpty()) {
                    QString s = e.attribute("email");
                    n += tmp + QString("Email: <a href='mailto:%1'>%2</a>").arg(s, s);
                }
                if (!e.attribute("jid").isEmpty()) {
                    QString s = e.attribute("jid");
                    n += tmp + QString("JID: <a href='xmpp:%1'>%2</a>").arg(s, s);
                }
                if (!e.attribute("www").isEmpty()) {
                    QString s = e.attribute("www");
                    n += tmp + QString("WWW: <a href='%1'>%2</a>").arg(s, s);
                }
                authors += n;
            } else if (tag == "creation") {
                creation = e.text();
            } else if (tag == "home") {
                homeUrl = e.text();
            }
        }
    }

    static int icon_counter; // used to give unique names to icons

    // will return 'true' when icon is loaded ok
    bool loadKdeEmoticon(const QDomElement &emot, const QString &dir, QSize &size)
    {
        static const QStringList exts   = { "png", "gif", "svg", "svgz", "mng" };
        auto                     baseFN = emot.attribute(QLatin1String("file"));

        PsiIcon icon;
        icon.blockSignals(true);

        QList<PsiIcon::IconText> text;

        for (QDomElement i = emot.firstChildElement(QLatin1String("string")); !i.isNull();
             i             = i.nextSiblingElement(QLatin1String("string"))) {
            text.append({ "", i.text() });
        }

        if (text.isEmpty()) {
            qWarning("text is empty for emoticon: %s", qPrintable(baseFN));
            return false;
        }

        auto      baseFNPath = dir + QLatin1String("/") + baseFN;
        QFileInfo finfo(baseFNPath);
        bool      found = finfo.isReadable();
        if (!found)
            for (auto const &fext : exts) {
                auto tmpFN = baseFNPath + QLatin1String(".") + fext;
                finfo      = QFileInfo(tmpFN);
                found      = finfo.isReadable();
                if (found) {
                    break;
                }
            }

        if (!found) {
            qWarning("file not found for emoticon: %s", qPrintable(baseFN));
            return false;
        }

        icon.setText(text);
        icon.setName(baseFN);

        QString mime;
        bool    isScalable = false;
        bool    isAnimated = false;
        if (finfo.suffix() == "svg" || finfo.suffix() == "svgz") {
            isScalable = true;
            mime       = QLatin1String("image/svg+xml");
        } else if (finfo.suffix() == "png") {
            mime = QLatin1String("image/png");
        } else if (finfo.suffix() == "gif") {
            mime       = QLatin1String("image/gif");
            isAnimated = true;
        } else {
            mime = QLatin1String("image/mng");
        }

        QFile file(finfo.filePath());
        if (!(file.open(QIODevice::ReadOnly) && icon.loadFromData(mime, file.readAll(), isAnimated, isScalable))) {
            qWarning("failed to read emoticon: %s", qPrintable(baseFN));
            return false;
        }

        // construct RegExp
        if (text.count()) {
            QStringList regexp;
            for (const PsiIcon::IconText &t : qAsConst(text)) {
                regexp += QRegExp::escape(t.text);
            }

            // make sure there is some form of whitespace on at least one side of the text string
            // regexp = QString("(\\b(%1))|((%2)\\b)").arg(regexp).arg(regexp);
            icon.setRegExp(QRegExp(regexp.join("|")));
        }
        size = icon.size();

        icon.blockSignals(false);

        append(baseFN, new PsiIcon(icon));
        return true;
    }

    // will return 'true' when icon is loaded ok
    bool loadIcon(const QDomElement &i, const QString &dir)
    {
        PsiIcon icon;
        icon.blockSignals(true);

        QList<PsiIcon::IconText> text;
        QHash<QString, QString>  graphic, sound, object; // mime => filename

        QString name       = QString::asprintf("icon_%04d", icon_counter++);
        bool    isAnimated = false;
        bool    isImage    = false;
        bool    isScalable = false;

        for (QDomNode n = i.firstChild(); !n.isNull(); n = n.nextSibling()) {
            QDomElement e = n.toElement();
            if (e.isNull()) {
                continue;
            }

            QString tag = e.tagName();
            if (tag == "text") {
                QString lang = e.attribute("xml:lang");
                if (lang.isEmpty()) {
                    lang = ""; // otherwise there would be many warnings :-(
                }
                QString t = e.text();
                if (!t.isEmpty()) {
                    text.append(PsiIcon::IconText(lang, t));
                }
            } else if (tag == "object") {
                object[e.attribute("mime")] = e.text();
            } else if (tag == "x") {
                QString attr = e.attribute("xmlns");
                if (attr == "name") {
                    name = e.text();
                } else if (attr == "type") {
                    if (e.text() == "animation") {
                        isAnimated = true;
                    } else if (e.text() == "image") {
                        isImage = true;
                    } else if (e.text() == "scalable") { // force scalability. should work well for big enough images
                        isScalable = true;
                    }
                }
            }
            // leaved for compatibility with old JEP
            else if (tag == "graphic") {
                graphic[e.attribute("mime")] = e.text();
            } else if (tag == "sound") {
                sound[e.attribute("mime")] = e.text();
            }
        }

        icon.setText(text);
        icon.setName(name);

        // first item have higher priority than latter
        static const QStringList graphicMime { "image/svg+xml", "image/png", "image/gif",
                                               "image/x-xpm",   "image/bmp", "image/jpeg" };
        static const QStringList scalableMime { "image/svg+xml" };
        static const QStringList soundMime { "audio/x-wav", "audio/x-ogg", "audio/x-mp3", "audio/x-midi" };
        // MIME-types, that support animations
        static const QStringList animationMime { "image/gif" };

        QStringList preferredGraphic;
        QStringList preferredSound;
        if (!object.isEmpty()) {
            // fill the graphic & sound tables, if there are some
            // 'object' entries. inspect the supported mimetypes
            // and copy mime info and file path to 'graphic' and
            // 'sound' dictonaries.
            for (auto const &mime : graphicMime) {
                auto it = object.find(mime);
                if (it != object.end()) {
                    graphic.insert(mime, it.value());
                    preferredGraphic.append(mime);
                }
            }

            for (auto const &mime : soundMime) {
                auto it = object.find(mime);
                if (it != object.end()) {
                    sound.insert(mime, it.value());
                    preferredSound.append(mime);
                }
            }
        }

        bool loadSuccess = std::any_of(preferredGraphic.begin(), preferredGraphic.end(), [&, this](const auto &mime) {
            QString fileName = graphic.value(mime);
            // if format supports animations, then load graphic as animation, and
            // if there is only one frame, then later it would be converted to single Impix
            QByteArray ba = this->loadData(fileName, dir);
            Q_ASSERT(!dir.startsWith(QLatin1String(":/")) || !ba.isEmpty());
            if (!ba.isEmpty()
                && icon.loadFromData(mime, ba, isAnimated || (!isImage && animationMime.indexOf(mime) != -1),
                                     isScalable || scalableMime.indexOf(mime) != -1))
                return true;

            qDebug("Iconset::load(): Couldn't load %s (%s) graphic for the %s icon for the %s iconset",
                   qPrintable(mime), qPrintable(fileName), qPrintable(name), qPrintable(this->name));
            return false;
        });

        loadSuccess = loadSuccess && std::any_of(preferredSound.begin(), preferredSound.end(), [&, this](const auto &mime) {
            QFileInfo fi(dir);
            QString   fileName = sound[mime];
            if (!fi.isDir()) { // it is a .zip file then
#ifdef ICONSET_SOUND
                if (!iconSharedObject) {
                    iconSharedObject = new IconSharedObject();
                }

                QString path = iconSharedObject->unpackPath;
                if (path.isEmpty()) {
                    qDebug("Iconset::load(): Couldn't load %s (%s) audio for the %s icon for the %s iconset. "
                           "unpack path is empty",
                           qPrintable(mime), qPrintable(fileName), qPrintable(name), qPrintable(this->name));
                    return false;
                }

                QFileInfo ext(fileName);
                path += "/" + QCA::Hash("sha1").hashToString(QString(fi.absoluteFilePath() + '/' + fileName).toUtf8())
                    + '.' + ext.suffix();

                QFile file(path);
                file.open(QIODevice::WriteOnly);
                QDataStream out(&file);

                QByteArray data = this->loadData(fileName, dir); // "this" for compatibility with old gcc
                if (data.isEmpty()) {
                    qDebug("Iconset::load(): Couldn't load %s (%s) audio for the %s icon for the %s iconset. "
                           "file is empty",
                           qPrintable(mime), qPrintable(fileName), qPrintable(name), qPrintable(this->name));
                    return false;
                }

                out.writeRawData(data, data.size());
                icon.setSound(path);
                return true;
#endif
            } else {
                QString absFN = fi.absoluteFilePath() + '/' + fileName;
                if (QFileInfo(absFN).isReadable()) {
                    icon.setSound(absFN);
                    return true;
                }
                qDebug("Iconset::load(): Couldn't load %s (%s) audio for the %s icon for the %s iconset. "
                       "not readable",
                       qPrintable(mime), qPrintable(fileName), qPrintable(name), qPrintable(this->name));
                return false;
            }
        });

        // construct RegExp
        if (text.count()) {
            QStringList regexp;
            for (const PsiIcon::IconText &t : qAsConst(text)) {
                regexp += QRegExp::escape(t.text);
            }

            // make sure there is some form of whitespace on at least one side of the text string
            // regexp = QString("(\\b(%1))|((%2)\\b)").arg(regexp).arg(regexp);
            icon.setRegExp(QRegExp(regexp.join("|")));
        }

        icon.blockSignals(false);

        if (loadSuccess) {
            append(name, new PsiIcon(icon));
        } else {
            qWarning("can't load icon because of unknown type");
        }

        return loadSuccess;
    }

    bool loadKdeEmoticons(const QDomDocument &doc, const QString dir)
    {
        QDomElement base = doc.documentElement();
        if (base.tagName() != "messaging-emoticon-map") {
            qWarning("failed to load iconset invalid toplevel xml element");
            return false;
        }

        bool success = false;
        name         = QFileInfo(dir).fileName();
        version      = "unknown";
        description  = "KDE Emoticons";
        iconSize_    = 0;

        for (QDomElement i = base.firstChildElement(QLatin1String("emoticon")); !i.isNull();
             i             = i.nextSiblingElement(QLatin1String("emoticon"))) {

            QSize s;
            bool  ret = loadKdeEmoticon(i, dir, s);
            if (ret) {
                success = true;
                if (s.height() > iconSize_) {
                    iconSize_ = s.height();
                }
            }
        }

        return success;
    }

    // would return 'true' on success
    bool load(const QDomDocument &doc, const QString dir)
    {
        QDomElement base = doc.documentElement();
        if (base.tagName() != "icondef") {
            qWarning("failed to load iconset invalid toplevel xml element");
            return false;
        }

        bool success = true;

        for (QDomNode node = base.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement i = node.toElement();
            if (i.isNull()) {
                continue;
            }

            QString tag = i.tagName();
            if (tag == "meta") {
                loadMeta(i, dir);
            } else if (tag == "icon") {
                bool ret = loadIcon(i, dir);
                if (!ret) {
                    success = false;
                }
            } else if (tag == "x") {
                info[i.attribute("xmlns")] = i.text();
            }
        }

        return success;
    }

    void setInformation(const Private &from)
    {
        name        = from.name;
        version     = from.version;
        description = from.description;
        creation    = from.creation;
        homeUrl     = from.homeUrl;
        filename    = from.filename;
        authors     = from.authors;
        info        = from.info;
        iconSize_   = from.iconSize_;
    }
};
//! \endif

int Iconset::Private::icon_counter = 0;

// static int iconset_counter = 0;

/**
 * Creates empty Iconset.
 */
Iconset::Iconset()
{
    // iconset_counter++;

    d = new Private;
}

/**
 * Creates shared copy of Iconset \a from.
 */
Iconset::Iconset(const Iconset &from)
{
    // iconset_counter++;

    d = from.d;
}

/**
 * Destroys Iconset, and frees all allocated Icons.
 */
Iconset::~Iconset() { IconsetFactoryPrivate::instance()->unregisterIconset(this); }

/**
 * Copies all Icons as well as additional information from Iconset \a from.
 */
Iconset &Iconset::operator=(const Iconset &from)
{
    d = from.d;

    return *this;
}

Iconset Iconset::copy() const
{
    Iconset is;
    is.d = new Private(*this->d.data());

    return is;
}

void Iconset::detach() { d.detach(); }

/**
 * Appends icons from Iconset \a from to this Iconset.
 */
Iconset &Iconset::operator+=(const Iconset &i)
{
    detach();

    QListIterator<PsiIcon *> it(i.d->list);
    while (it.hasNext()) {
        PsiIcon *icon = new PsiIcon(*it.next());
        d->append(icon->name(), icon);
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
int Iconset::count() const { return d->list.count(); }

/**
 * Loads Icons and additional information from directory \a dir. Directory can usual directory,
 * or a .zip/.jisp archive. There must exist file named \c icondef.xml in that directory.
 */
bool Iconset::load(const QString &dir, Format format)
{
    if (dir.isEmpty()) {
        return false;
    }
    Q_ASSERT(!dir.isEmpty());

    detach();

    // make it run okay on windows 9.x (where the pixmap memory is limited)
    // QPixmap::Optimization optimization = QPixmap::defaultOptimization();
    // QPixmap::setDefaultOptimization( QPixmap::MemoryOptim );

    bool ret = false;

    QByteArray ba;

    QString fileName;
    switch (format) {
    case Format::Psi:
        d->id    = dir.section('/', -2);
        fileName = QLatin1String("icondef.xml");
        break;
    case Format::KdeEmoticons:
        d->id    = dir.section('/', -1);
        fileName = QLatin1String("emoticons.xml");
        break;
    default:
        return false;
    }

    ba = d->loadData(fileName, dir);
    if (!ba.isEmpty()) {
        QDomDocument doc;
        if (doc.setContent(ba, false)) {
            if ((format == Format::Psi && d->load(doc, dir))
                || (format == Format::KdeEmoticons && d->loadKdeEmoticons(doc, dir))) {
                d->filename = dir;
                ret         = true;
            }
        } else {
            qWarning("Iconset::load(\"%s\"): Failed to load iconset: icondef.xml is invalid XML", qPrintable(dir));
        }
    } else {
        Q_ASSERT_X(!dir.startsWith(QLatin1String(":/")) || !ba.isEmpty(), qPrintable(dir),
                   "Failed to load icondef.xml");
        qWarning("Iconset::load(\"%s\"): Failed to load icondef.xml", qPrintable(dir));
    }
    d->zipCache.clear();

    // QPixmap::setDefaultOptimization( optimization );

    return ret;
}

/**
 * Returns pointer to PsiIcon, if PsiIcon with name \a name was found in Iconset, or \a 0 otherwise.
 * \sa setIcon()
 */
const PsiIcon *Iconset::icon(const QString &name) const
{
    if (!d || d->dict.isEmpty()) {
        return nullptr;
    }

    return d->dict.value(name);
}

/**
 * Appends PsiIcon to Iconset. If the PsiIcon with that name already exists, it is removed.
 */
void Iconset::setIcon(const QString &name, const PsiIcon &icon)
{
    detach();

    PsiIcon *newIcon = new PsiIcon(icon);

    d->remove(name);
    d->append(name, newIcon);
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
 * Returns the Iconset unique identifier (ex: "system/default").
 */
const QString &Iconset::id() const { return d->id; }

/**
 * Returns the Iconset name.
 */
const QString &Iconset::name() const { return d->name; }

/**
 * Returns the icons size from Iconset.
 */
const int &Iconset::iconSize() const { return d->iconSize_; }

/**
 * Returns the Iconset version.
 */
const QString &Iconset::version() const { return d->version; }

/**
 * Returns the Iconset description string.
 */
const QString &Iconset::description() const { return d->description; }

/**
 * Returns the Iconset authors list.
 */
const QStringList &Iconset::authors() const { return d->authors; }

/**
 * Returns the Iconset creation date.
 */
const QString &Iconset::creation() const { return d->creation; }

/**
 * Returns the Iconsets' home URL.
 */
const QString &Iconset::homeUrl() const { return d->homeUrl; }

QListIterator<PsiIcon *> Iconset::iterator() const
{
    QListIterator<PsiIcon *> it(d->list);
    return it;
}

QList<PsiIcon *>::const_iterator Iconset::begin() const { return d->list.cbegin(); }

QList<PsiIcon *>::const_iterator Iconset::end() const { return d->list.cend(); }

/**
 * Returns directory (or .zip/.jisp archive) name from which Iconset was loaded.
 */
const QString &Iconset::fileName() const { return d->filename; }

/**
 * Sets the Iconset directory (.zip archive) name.
 */
void Iconset::setFileName(const QString &f) { d->filename = f; }

/**
 * Sets the information (meta-data) of this iconset to the information from the given iconset.
 */
void Iconset::setInformation(const Iconset &from)
{
    detach();
    d->setInformation(*(from.d));
}

/**
 * Returns additional Iconset information.
 * \sa setInfo()
 */
const QHash<QString, QString> Iconset::info() const { return d->info; }

/**
 * Sets additional Iconset information.
 * \sa info()
 */
void Iconset::setInfo(const QHash<QString, QString> &i) { d->info = i; }

/**
 * Adds Iconset to IconsetFactory.
 */
void Iconset::addToFactory() const { IconsetFactoryPrivate::instance()->registerIconset(this); }

/**
 * Removes Iconset from IconsetFactory.
 */
void Iconset::removeFromFactory() const { IconsetFactoryPrivate::instance()->unregisterIconset(this); }

bool Iconset::isSourceAllowed(const QFileInfo &fi)
{
#ifdef ICONSET_ZIP
    if (fi.isDir())
        return true;
    QString lower = fi.suffix().toLower();
    return lower == "jisp" || lower == ".zip";
#else
    // files supported ony for zipped iconsets
    return fi.isDir();
#endif
}

/**
 * Use this function before creation of Iconsets and it will enable the
 * sound playing ability when Icons are activated().
 *
 * \a unpackPath is the path, to where the sound files will be unpacked, if
 * required (i.e. when sound is stored inside packed iconset.zip file). Unpacked
 * file name will be unique, and you can empty the unpack directory at the application
 * exit. Calling application MUST ensure that the \a unpackPath is already created.
 * If specified \a unpackPath is empty ("" or QString()) then sounds from archives
 * will not be unpacked, and only sounds from already unpacked iconsets (that are not
 * stored in archives) will be played.
 * \a receiver and \a slot are used to specify the object that will be connected to
 * the signal 'playSound(QString fileName)'. Slot should play the specified sound
 * file.
 */
void Iconset::setSoundPrefs(QString unpackPath, QObject *receiver, const char *slot)
{
#ifdef ICONSET_SOUND
    if (!iconSharedObject) {
        iconSharedObject = new IconSharedObject();
    }

    iconSharedObject->unpackPath = unpackPath;
    QObject::connect(iconSharedObject, SIGNAL(playSound(QString)), receiver, slot);
#else
    Q_UNUSED(unpackPath);
    Q_UNUSED(receiver);
    Q_UNUSED(slot);
#endif
}

#include "iconset.moc"
