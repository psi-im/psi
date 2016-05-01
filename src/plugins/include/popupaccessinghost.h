#ifndef POPUPACCESSINGHOST_H
#define POPUPACCESSINGHOST_H

class QString;

class PopupAccessingHost
{
public:
	virtual ~PopupAccessingHost() {}

	virtual void initPopup(const QString& text, const QString& title, const QString& icon = "psi/headline", int type = 0) = 0;
	virtual void initPopupForJid(int account, const QString& jid, const QString& text, const QString& title, const QString& icon = "psi/headline", int type = 0) = 0;
	virtual int registerOption(const QString& name, int initValue = 5, const QString& path = QString()) = 0;
	virtual int popupDuration(const QString& name) = 0;
	virtual void setPopupDuration(const QString& name, int value) = 0;
	virtual void unregisterOption(const QString& name) = 0;
};

Q_DECLARE_INTERFACE(PopupAccessingHost, "org.psi-im.PopupAccessingHost/0.1");

#endif
