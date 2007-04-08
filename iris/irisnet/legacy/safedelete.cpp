#include"safedelete.h"

//----------------------------------------------------------------------------
// SafeDelete
//----------------------------------------------------------------------------
SafeDelete::SafeDelete()
{
	lock = 0;
}

SafeDelete::~SafeDelete()
{
	if(lock)
		lock->dying();
}

void SafeDelete::deleteLater(QObject *o)
{
	if(!lock)
		deleteSingle(o);
	else
		list.append(o);
}

void SafeDelete::unlock()
{
	lock = 0;
	deleteAll();
}

void SafeDelete::deleteAll()
{
	if(list.isEmpty())
		return;

	for(int n = 0; n < list.size(); ++n)
		deleteSingle(list[n]);
	list.clear();
}

void SafeDelete::deleteSingle(QObject *o)
{
#if QT_VERSION < 0x030000
	// roll our own QObject::deleteLater()
	SafeDeleteLater *sdl = SafeDeleteLater::ensureExists();
	sdl->deleteItLater(o);
#else
	o->deleteLater();
#endif
}

//----------------------------------------------------------------------------
// SafeDeleteLock
//----------------------------------------------------------------------------
SafeDeleteLock::SafeDeleteLock(SafeDelete *sd)
{
	own = false;
	if(!sd->lock) {
		_sd = sd;
		_sd->lock = this;
	}
	else
		_sd = 0;
}

SafeDeleteLock::~SafeDeleteLock()
{
	if(_sd) {
		_sd->unlock();
		if(own)
			delete _sd;
	}
}

void SafeDeleteLock::dying()
{
	_sd = new SafeDelete(*_sd);
	own = true;
}

//----------------------------------------------------------------------------
// SafeDeleteLater
//----------------------------------------------------------------------------
SafeDeleteLater *SafeDeleteLater::self = 0;

SafeDeleteLater *SafeDeleteLater::ensureExists()
{
	if(!self)
		new SafeDeleteLater();
	return self;
}

SafeDeleteLater::SafeDeleteLater()
{
	self = this;
	QTimer::singleShot(0, this, SLOT(explode()));
}

SafeDeleteLater::~SafeDeleteLater()
{
	qDeleteAll(list);
	list.clear();
	self = 0;
}

void SafeDeleteLater::deleteItLater(QObject *o)
{
	list.append(o);
}

void SafeDeleteLater::explode()
{
	delete this;
}
