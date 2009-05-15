#include "../../psirichtext.cpp"
#include <QTextDocument>
#include <QTextCursor>

#include <QtTest/QtTest>

class TestRichText : public QObject
{
	Q_OBJECT
private:
	int countText(QTextDocument *doc, QString text) {
		int result = 0;
		QTextCursor cursor(doc);
		cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
		
		forever {
			cursor = doc->find(text, cursor);
			if (cursor.isNull())
				break;
			
			result++;
		}
		
		return result;
	}
	
private slots:
	void testSetText() {
		QTextDocument doc;
		PsiRichText::setText(&doc, "Test <icon name=\"foo\" text=\"bar\">");
		QCOMPARE(countText(&doc, QString(QChar::ObjectReplacementCharacter)), 1);
	}
};

QTEST_MAIN(TestRichText)
#include "main.moc"

