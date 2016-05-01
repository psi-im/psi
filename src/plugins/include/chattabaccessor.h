#ifndef CHATTABACCESSOR_H
#define CHATTABACCESSOR_H

class QWidget;
class QString;
class QDomElement;

class ChatTabAccessor
{
public:
	virtual ~ChatTabAccessor() {}

	virtual void setupChatTab(QWidget* tab, int account, const QString& contact) = 0;
	virtual void setupGCTab(QWidget* tab, int account, const QString& contact) = 0;

	virtual bool appendingChatMessage(int account, const QString& contact,
					  QString& body, QDomElement& html, bool local) = 0;
};

Q_DECLARE_INTERFACE(ChatTabAccessor, "org.psi-im.ChatTabAccessor/0.1")

#endif // CHATTABACCESSOR_H
