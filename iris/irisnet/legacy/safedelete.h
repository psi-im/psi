#ifndef SAFEDELETE_H
#define SAFEDELETE_H

#include <QtCore>

class SafeDelete;
class SafeDeleteLock
{
public:
	SafeDeleteLock(SafeDelete *sd);
	~SafeDeleteLock();

private:
	SafeDelete *_sd;
	bool own;
	friend class SafeDelete;
	void dying();
};

class SafeDelete
{
public:
	SafeDelete();
	~SafeDelete();

	void deleteLater(QObject *o);

	// same as QObject::deleteLater()
	static void deleteSingle(QObject *o);

private:
	QObjectList list;
	void deleteAll();

	friend class SafeDeleteLock;
	SafeDeleteLock *lock;
	void unlock();
};

class SafeDeleteLater : public QObject
{
	Q_OBJECT
public:
	static SafeDeleteLater *ensureExists();
	void deleteItLater(QObject *o);

private slots:
	void explode();

private:
	SafeDeleteLater();
	~SafeDeleteLater();

	QObjectList list;
	friend class SafeDelete;
	static SafeDeleteLater *self;
};

#endif
