#ifndef WHATISMYIP_H
#define WHATISMYIP_H

#include <QObject>
#include <QString>
#include <QHostAddress>

class WhatIsMyIp : public QObject
{
	Q_OBJECT

public:
	WhatIsMyIp(QObject *parent = 0);
	~WhatIsMyIp();

	void start(const QString &stunHost, int stunPort);

signals:
	void addressReady(const QHostAddress &addr);

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
