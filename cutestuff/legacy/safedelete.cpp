#include "safedelete.h"

#include <qtimer.h>

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

	QObjectList::Iterator it = list.begin();
	for(QObjectList::Iterator it = list.begin(); it != list.end(); ++it)
		deleteSingle(*it);
	list.clear();
}

void SafeDelete::deleteSingle(QObject *o)
{
	o->deleteLater();
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
	while (!list.isEmpty())
		delete list.takeFirst();
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
