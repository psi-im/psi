#include <QObject>
#include <QtTest/QtTest>

#include <QMap>
#include <QMapIterator>
#include <QDebug>
#include <QTime>

#include "qttestutil/qttestutil.h"

#include "optionstree.h"

class Benchmark
{
public:
	void end(const QString& msg)
	{
		float avg = 0.0;
		foreach(int i, results_) {
			avg += float(i);
		}
		avg /= float(results_.count());
		qWarning() << msg << avg << results_;
		// qWarning("BENCH: %s: %ds %dmsec", qPrintable(msg), msecs / 1000, msecs % 1000);
		results_.clear();
	}

	void startIteration()
	{
		time_ = QTime::currentTime();
	}

	void endIteration()
	{
		results_ << time_.msecsTo(QTime::currentTime());
	}

private:
	QTime time_;
	QList<int> results_;
};

class OptionsTreeMainTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase() {
		QVariantList l;
		l << QVariant(QString("item1")) << qVariantFromValue(QKeySequence("CTRL+L"));
		QStringList sl;
		sl << "String 1" << "String 2";
		l << sl;
		l << QRect(10, 20, 30, -666);
		l << l;

		goodValues_["ba"] = QByteArray(QString("0xDEADBEEF").toLatin1());
		goodValues_["paris"] = QVariant(QString("sirap"));
		goodValues_["Benvolio"] = QVariant(QString("Benvolio"));
		goodValues_["Benvolio"] = QVariant(QString("Not benvolio!!"));
		goodValues_["capulet.Juliet"] = QVariant(QString("girly"));
		goodValues_["verona.montague.romeo"] = QVariant(QString("poisoned"));
		goodValues_["capulet.Nursey"] = QVariant(QString("matchmaker"));
		goodValues_["verona.city"] = QVariant(true);
		goodValues_["verona.lovers"] = QVariant(2);
		goodValues_["verona.size"] = QVariant(QSize(210,295));
		goodValues_["verona.stuff"] = l;
		goodValues_["verona.stringstuff"] = sl;
		// qWarning() << goodValues_;

		badValues_["capulet.Juliet.dead"] = QVariant(true);
		// qWarning() << badValues_;

		comments_["verona"] = "Fair city";
		comments_["paris"] = "Bloke or city?";
		comments_["verona.montague.romeo"] = "Watch what this one drinks";
		// qWarning() << comments_;
	}

	void cleanupTestCase() {
	}

	void createTreeTest() {
		OptionsTree tree;
		initTree(&tree);
		verifyTree(&tree);

		// foreach(QString name, tree.allOptionNames()) {
		// 	qWarning() << name << " = " << tree.getOption(name).toString();
		// }
	}

	void saveLoadTreeTest() {
		OptionsTree tree;
		initTree(&tree);
		verifyTree(&tree);

		tree.saveOptions(dir() + "/options.xml","OptionsTest","http://psi-im.org/optionstest","0.1");

		OptionsTree tree2;
		tree2.loadOptions(dir() + "/options.xml","OptionsTest","http://psi-im.org/optionstest","0.1", true);
		// tree2.saveOptions(dir() + "/options2.xml","OptionsTest","http://psi-im.org/optionstest","0.1");
		tree2.saveOptions(dir() + "/options3.xml","OptionsTest","http://psi-im.org/optionstest","0.1", true);
		verifyTree(&tree2);
	}

#if 0
	void stressTest() {
		bench_.startIteration();
		QMap<QString, QVariant> data = generateStressTestValues(100, 100);
		bench_.endIteration();
		bench_.end("generateStressTestValues");

		OptionsTree tree;
		initTreeValues(&tree, data);

		for (int i = 0; i < 100; ++i) {
			bench_.startIteration();
			verifyTreeValues(&tree, data);
			bench_.endIteration();
		}
		bench_.end("verifyTreeValues");
	}
#endif

// #if 0
	QString dir() {
		return "/Users/mblsha/src/psi/src/tools/optionstree/unittest";
	}

	void benchLoadOptions() {
		// sleep(1);
		// OptionsTree tree;
		// QBENCHMARK {
		//     tree.loadOptions(dir() + "/mbl_options.xml", "options", "http://psi-im.org/options", "0.1");
		// }

		// for (int i = 0; i < 100; ++i) {
		QBENCHMARK {
		OptionsTree tree;
		tree.loadOptions(dir() + "/mbl_options.xml", "options", "http://psi-im.org/options", "0.1");
		}
	}

	void benchLoadOptionsStream() {
		QBENCHMARK {
		OptionsTree tree;
		tree.loadOptions(dir() + "/mbl_options.xml", "options", "http://psi-im.org/options", "0.1", true);
		}
	}

	void benchLoadAccounts() {
		// sleep(1);
		// OptionsTree tree;
		// QBENCHMARK {
		//     tree.loadOptions(dir() + "/mbl_accounts.xml", "accounts", "http://psi-im.org/options", "0.1");
		// }

		// for (int i = 0; i < 100; ++i) {
		QBENCHMARK {
		OptionsTree tree;
		tree.loadOptions(dir() + "/mbl_accounts.xml", "accounts", "http://psi-im.org/options", "0.1");
		}
	}

	void benchLoadAccountsStream() {
		QBENCHMARK {
		OptionsTree tree;
		tree.loadOptions(dir() + "/mbl_accounts.xml", "accounts", "http://psi-im.org/options", "0.1", true);
		}
	}

	void benchSaveAccounts() {
		OptionsTree tree;
		tree.loadOptions(dir() + "/mbl_accounts.xml", "accounts", "http://psi-im.org/options", "0.1");
		QBENCHMARK {
		tree.saveOptions(dir() + "/mbl_accounts2.xml", "accounts", "http://psi-im.org/options", "0.1");
		}
	}

	void benchSaveAccountsStream() {
		OptionsTree tree;
		tree.loadOptions(dir() + "/mbl_accounts.xml", "accounts", "http://psi-im.org/options", "0.1");
		QBENCHMARK {
		tree.saveOptions(dir() + "/mbl_accounts2.xml", "accounts", "http://psi-im.org/options", "0.1", true);
		}
	}
	// #endif

// #endif

private:
	QMap<QString, QVariant> goodValues_;
	QMap<QString, QVariant> badValues_;
	QMap<QString, QString> comments_;

	QMap<QString, QVariant> generateStressTestValues(int count, int depth) {
		QMap<QString, QVariant> result;
		for (int i = 1; i <= count; ++i) {
			QMap<QString, QVariant> r = generateStressTestValues(QString("i%1").arg(i), depth);
			QMapIterator<QString, QVariant> it(r);
			while (it.hasNext()) {
				it.next();
				result[it.key()] = it.value();
			}
		}
		return result;
	}

	QMap<QString, QVariant> generateStressTestValues(const QString& name, int depth) {
		Q_ASSERT(depth >= 1);
		QMap<QString, QVariant> result;
		for (int i = 1; i <= depth; ++i) {
			QStringList children;
			for (int j = 1; j < i; ++j) {
				children << "child";
			}

			QString n = name;
			if (!children.isEmpty())
				n += '.' + children.join('.');
			n += ".value";
			result[n] = QVariant(i);
		}
		return result;
	}

	void initTree(OptionsTree* tree) {
		initTreeValues(tree, goodValues_);
		initTreeValues(tree, badValues_);
		initTreeComments(tree, comments_);
	}

	void initTreeValues(OptionsTree* tree, const QMap<QString, QVariant>& values) {
		Q_ASSERT(tree);
		Q_ASSERT(!values.isEmpty());
		QMapIterator<QString, QVariant> it(values);
		while (it.hasNext()) {
			it.next();
			tree->setOption(it.key(), it.value());
		}
	}

	void initTreeComments(OptionsTree* tree, const QMap<QString, QString>& comments) {
		Q_ASSERT(tree);
		Q_ASSERT(!comments.isEmpty());
		QMapIterator<QString, QString> it(comments);
		while (it.hasNext()) {
			it.next();
			tree->setComment(it.key(), it.value());
		}
	}

	void verifyTree(const OptionsTree* tree) {
		verifyTreeValues(tree, goodValues_);
		verifyTreeBadValues(tree, badValues_);
		verifyTreeComments(tree, comments_);
	}

	void verifyTreeValues(const OptionsTree* tree, const QMap<QString, QVariant>& values) {
		Q_ASSERT(tree);
		Q_ASSERT(!values.isEmpty());
		QMapIterator<QString, QVariant> it(values);
		while (it.hasNext()) {
			it.next();
			QCOMPARE(tree->getOption(it.key()), it.value());
			Q_ASSERT(!tree->getOption(it.key()).isNull());
		}
	}

	void verifyTreeBadValues(const OptionsTree* tree, const QMap<QString, QVariant>& values) {
		Q_ASSERT(tree);
		Q_ASSERT(!values.isEmpty());
		QMapIterator<QString, QVariant> it(values);
		while (it.hasNext()) {
			it.next();
			Q_ASSERT(tree->getOption(it.key()).isNull());
		}
	}

	void verifyTreeComments(const OptionsTree* tree, const QMap<QString, QString>& comments) {
		Q_ASSERT(tree);
		Q_ASSERT(!comments.isEmpty());
		QMapIterator<QString, QString> it(comments);
		while (it.hasNext()) {
			it.next();
			QCOMPARE(tree->getComment(it.key()), it.value());
		}
	}

	Benchmark bench_;
};

QTTESTUTIL_REGISTER_TEST(OptionsTreeMainTest);
#include "OptionsTreeMainTest.moc"
