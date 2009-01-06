#ifndef PSICHATDLG_H
#define PSICHATDLG_H

#include "chatdlg_b.h"

#include "ui_chatdlg.h"

class QLabel;
class IconAction;
class ClickableLabel;
class InviteDlg;

QString PsiChatDlgB_getDisclaimer(ChatDlg *c, const Jid &jid);

class PsiChatDlgB : public ChatDlg
{
	Q_OBJECT
public:
	PsiChatDlgB(const Jid& jid, PsiAccount* account, TabManager* tabManager);
	~PsiChatDlgB();

	QString getDisclaimer(const Jid &jid);

	// reimplemented
	virtual bool readyToHide();

protected:
	// reimplemented
	void contextMenuEvent(QContextMenuEvent *);

private:
	void setContactToolTip(QString text);

private slots:
	void toggleSmallChat();
	void doClearButton();
	void buildMenu();
	void updateCounter();
	void updateIdentityVisibility();
	void updateCountVisibility();

	// reimplemented
	void chatEditCreated();

	void doTabToggle();
	void reset_conversation_time();
	void vcardChanged(const Jid &);
	void avatar_clicked();
	void doInvite();

private:
	void initToolBar();
	void initToolButtons();

	// reimplemented
	void initUi();
	void capsChanged();
	bool isEncryptionEnabled() const;
	void contactUpdated(UserListItem* u, int status, const QString& statusString);
	void updateAvatar();
	void optionsUpdate();
	void updatePGP();
	void setPGPEnabled(bool enabled);
	void activated();
	void setLooks();
	void setShortcuts();
	QString colorString(bool local, SpooledType spooled) const;
	void appendSysMsg(const QString &);
	void appendEmoteMessage(SpooledType spooled, const QDateTime& time, bool local, QString txt);
	void appendNormalMessage(SpooledType spooled, const QDateTime& time, bool local, QString txt);
	void appendMessageFields(const Message& m);
	void updateLastMsgTime(QDateTime t);
	ChatView* chatView() const;
	ChatEdit* chatEdit() const;

private:
	Ui::ChatDlg ui_;

	QMenu* pm_settings_;

	IconAction* act_clear_;
	IconAction* act_history_;
	IconAction* act_info_;
	IconAction* act_pgp_;
	IconAction* act_icon_;
	IconAction* act_file_;
	IconAction* act_compact_;
	IconAction* act_voice_;
	IconAction* act_tabtoggle_;
	IconAction* act_invite_;
	QPointer<InviteDlg> invite_;

	bool smallChat_;
	QDateTime lastMsgTime_;

	QLabel *lb_peerpic;
	ClickableLabel *lb_selfpic;
	bool conversation_time_up;
};

#endif
