#include "simplesearch.h"

#include "xmpp.h"
#include "psiaccount.h"
#include "xmpp_tasks.h"

class SimpleSearch::Private : public QObject
{
	Q_OBJECT

public:
	SimpleSearch *q;
	PsiAccount *pa;
	QString query;
	JT_Search *jt_search;
	QTimer startTimeout;

	Private(SimpleSearch *_q, PsiAccount *_pa) :
		QObject(_q),
		q(_q),
		pa(_pa),
		startTimeout(this)
	{
		jt_search = 0;
		connect(&startTimeout, SIGNAL(timeout()), SLOT(do_start()));
		startTimeout.setSingleShot(true);
		startTimeout.setInterval(2000); // 2 seconds
	}

	~Private()
	{
		delete jt_search;
	}

	void start(const QString &_query)
	{
		query = _query;

		if(jt_search)
		{
			delete jt_search;
			jt_search = 0;
		}

		// FIXME
		//   we should emit this late, but only before search
		//   results are returned, to be DOR compliant
		emit q->results(QList<SimpleSearch::Result>());

		if(!pa->isActive())
			return;

		startTimeout.start();
	}

private slots:
	void do_start()
	{
		jt_search = new JT_Search(pa->client()->rootTask());
		connect(jt_search, SIGNAL(finished()), SLOT(search_finished()));

		XData::Field field;
		field.setVar("any");
		field.setValue(QStringList() << query);
		XData form;
		form.setType(XData::Data_Submit);
		form.setFields(XData::FieldList() << field);

		jt_search->set(QString("vjud.") + pa->jid().domain(), form);
		jt_search->go(true);
	}

public:
	void stop()
	{
		if(query.isEmpty())
			return;

		query.clear();

		if(jt_search)
		{
			delete jt_search;
			jt_search = 0;
		}

		// FIXME
		//   we should emit this late, but only before search
		//   results are returned, to be DOR compliant
		emit q->results(QList<SimpleSearch::Result>());
	}

private slots:
	void search_finished()
	{
		bool haveXData = false;
		QList<SearchResult> results;
		QList<XData::ReportItem> reportItems;
		if(jt_search->hasXData())
		{
			reportItems = jt_search->xdata().reportItems();
			haveXData = true;
		}
		else
			results = jt_search->results();
		jt_search = 0;

		if(!haveXData)
			return;

		QList<SimpleSearch::Result> list;
		//printf("%d results\n", reportItems.count());
		foreach(const XData::ReportItem &i, reportItems)
		{
			SimpleSearch::Result r;
			r.jid = i["jid"];
			r.name = i["fn"];
			//printf(" %s [%s]\n", qPrintable(r.name), qPrintable(r.jid.full()));
			list += r;
		}

		// add some fake results
		/*{
			SimpleSearch::Result r;
			r.jid = "jerry@example.com";
			r.name = "Jerry Seinfeld";
			list += r;
		}
		{
			SimpleSearch::Result r;
			r.jid = "george@example.com";
			r.name = "George Costanza";
			list += r;
		}
		{
			SimpleSearch::Result r;
			r.jid = "elaine@example.com";
			r.name = "Elaine Benes";
			list += r;
		}
		{
			SimpleSearch::Result r;
			r.jid = "kramer@example.com";
			r.name = "Cosmo Kramer";
			list += r;
		}*/

		emit q->results(list);
	}
};

SimpleSearch::SimpleSearch(PsiAccount *pa)
{
	d = new Private(this, pa);
}

SimpleSearch::~SimpleSearch()
{
	delete d;
}

void SimpleSearch::search(const QString &query)
{
	QString simp = query.simplified();

	if(simp.isEmpty())
	{
		stop();
		return;
	}

	// searching for the same thing, do nothing
	if(simp == d->query)
		return;

	d->start(simp);
}

void SimpleSearch::stop()
{
	d->stop();
}

#include "simplesearch.moc"
