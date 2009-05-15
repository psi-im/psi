/**
 * Copyright (C) 2007, Remko Troncon
 * See COPYING file for the detailed license.
 */

// FIXME: This test suite is far from complete

#include <QObject>
#include <QtTest/QtTest>

#include "qttestutil/qttestutil.h"
#include "protocol/discoinfoquerier.h"
#include "xmpp/jid/jid.h"
#include "capabilities/capsmanager.h"

using namespace XMPP;

// -----------------------------------------------------------------------------

class TestDiscoInfoQuerier : public Protocol::DiscoInfoQuerier
{
public:
	struct DiscoInfo {
		XMPP::Jid jid;
		QString node;
		XMPP::DiscoItem item;
	};

	TestDiscoInfoQuerier() {
		nb_getdiscoinfo_called_ = 0;
	}

	void getDiscoInfo(const Jid& j, const QString& n) {
		emitDiscoInfo(j,n);
	}

	void emitDiscoInfo(const XMPP::Jid& j, const QString& n) {
		nb_getdiscoinfo_called_++;
		foreach(DiscoInfo info, infos_) {
			if (info.jid.compare(j,true) && info.node == n) {
				emit getDiscoInfo_success(j,n,info.item);
				return;
			}
		}
	}

	void addInfo(const XMPP::Jid& j, const QString& n, const XMPP::DiscoItem& it) {
		DiscoInfo i;
		i.jid = j;
		i.node = n;
		i.item = it;
		infos_ += i;
	}

	unsigned int nb_getdiscoinfo_called_;
	QList<DiscoInfo> infos_;
};

// -----------------------------------------------------------------------------

class CapsManagerTest : public QObject
{
		Q_OBJECT

	private slots:
		void initTestCase() {
			manager_ = NULL;
			querier_ = new TestDiscoInfoQuerier();
		}

		void cleanupTestCase() {
			delete manager_;
		}

		void testUpdateCaps()
		{
			QStringList capabilities;
			capabilities << "c1" << "c2" << "c3";
			addContact("you@example.com/a", "myclient", "myversion", capabilities);
			CapsManager* manager = createManager("me@example.com");

			manager->updateCaps("you@example.com/a", "myclient", "myversion", "c1 c2");
			XMPP::Features features(manager->features("you@example.com/a"));

			QCOMPARE(querier_->nb_getdiscoinfo_called_, 3U);
			QVERIFY(features.test(QStringList("myversion_f1")));
			QVERIFY(features.test(QStringList("c1_f1")));
			QVERIFY(features.test(QStringList("c1_f2")));
			QVERIFY(features.test(QStringList("c2_f1")));
			QVERIFY(features.test(QStringList("c2_f2")));
		}

		void testCapsEnabled() {
			QStringList capabilities;
			capabilities << "c1" << "c2";
			addContact("you@example.com/a", "myclient", "myversion", capabilities);
			CapsManager* manager = createManager("me@example.com");

			manager->updateCaps("you@example.com/a", "myclient", "myversion", "c1 c2");

			QVERIFY(manager->capsEnabled("you@example.com/a"));
		}

		void testCapsEnabled_NoCaps() {
			CapsManager* manager = createManager("me@example.com/a");

			QVERIFY(!manager->capsEnabled("you@example.com/b"));
		}

		void testDisableCaps() {
			QStringList capabilities;
			capabilities << "c1" << "c2";
			addContact("you@example.com/a", "myclient", "myversion", capabilities);
			CapsManager* manager = createManager("me@example.com");
			manager->updateCaps("you@example.com/a", "myclient", "myversion", "c1 c2");

			manager->disableCaps("you@example.com/a");

			QVERIFY(!manager->capsEnabled("you@example.com/a"));
		}

	private:
		void addContact(const QString& jid, const QString& client, const QString& version, const QStringList& capabilities) {
			XMPP::DiscoItem i;
			i.setFeatures(XMPP::Features(version + "_f1"));
			querier_->addInfo(jid, client + '#' + version, i);

			foreach(QString s, capabilities) {
				XMPP::DiscoItem i2;
				QStringList f;
				f << s + "_f1" << s + "_f2";
				i2.setFeatures(XMPP::Features(f));
				querier_->addInfo(jid, client + '#' + s, i2);
			}
		}

		CapsManager* createManager(const QString& jid) {
			manager_ = new CapsManager(jid, &registry_, querier_);
			return manager_;
		}

	private:
		CapsRegistry registry_;
		TestDiscoInfoQuerier* querier_;
		CapsManager* manager_;
};

QTTESTUTIL_REGISTER_TEST(CapsManagerTest);
#include "capsmanagertest.moc"
