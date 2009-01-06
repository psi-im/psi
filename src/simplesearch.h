#ifndef SIMPLESEARCH_H
#define SIMPLESEARCH_H

#include <QtCore>
#include "xmpp_jid.h"

class PsiAccount;

class SimpleSearch : public QObject
{
	Q_OBJECT

public:
	class Result
	{
	public:
		XMPP::Jid jid;
		QString name;
	};

	SimpleSearch(PsiAccount *pa);
	~SimpleSearch();

	void search(const QString &query);
	void stop();

signals:
	void results(const QList<SimpleSearch::Result> &list);

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
