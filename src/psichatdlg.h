#ifndef PSICHATDLG_H
#define PSICHATDLG_H

#include "minicmd.h"
#include "mcmdsimplesite.h"
#include "mcmdmanager.h"
#include "chatdlg.h"
#include "mcmdcompletion.h"
#include "applicationinfo.h"

#include "ui_chatdlg.h"
#include "typeaheadfind.h"
#include "widgets/actionlineedit.h"

class IconAction;
class PsiContact;

class PsiChatDlg : public ChatDlg
{
	Q_OBJECT
public:
	PsiChatDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager);

protected:
	// reimplemented
	void contextMenuEvent(QContextMenuEvent *);
	void doSend();
	bool eventFilter(QObject *obj, QEvent *event);
	void ackLastMessages(int msgs);

private:
	void setContactToolTip(QString text);

private slots:
	void toggleSmallChat();
	void doClearButton();
	void doMiniCmd();
	void addContact();
	void buildMenu();
	void updateCounter();
	void updateIdentityVisibility();
	void updateCountVisibility();
	void updateContactAdding(PsiContact* c = 0);
	void updateContactAdding(const Jid &j);
	void contactChanged();
	QString makeContactName(const QString &name, const Jid &jid) const;
	void doSwitchJidMode();
	void copyUserJid();

	// reimplemented
	void chatEditCreated();

private:
	void initToolBar();
	void initToolButtons();

	// reimplemented
	void initUi();
	void capsChanged();
	bool isEncryptionEnabled() const;
	void updateJidWidget(const QList<UserListItem*> &ul, int status, bool fromPresence);
	void contactUpdated(UserListItem* u, int status, const QString& statusString);
	void updateAvatar();
	void optionsUpdate();
	void updatePGP();
	void setPGPEnabled(bool enabled);
	void activated();
	void setLooks();
	void setShortcuts();
	void appendSysMsg(const QString &);
	ChatView* chatView() const;
	ChatEdit* chatEdit() const;
	void updateAutojidIcon();
	void setJidComboItem(int pos, const QString &text, const Jid &jid, const QString &icon_str);

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
	IconAction* act_find_;
	IconAction* act_html_text;
	IconAction* act_add_contact;

	QAction *act_mini_cmd_;
	TypeAheadFindBar *typeahead_;

	ActionLineEdit *le_autojid;
	IconAction *act_autojid;

	MCmdManager mCmdManager_;
	MCmdSimpleSite mCmdSite_;

	MCmdTabCompletion tabCompletion;

	bool smallChat_;
	class ChatDlgMCmdProvider;

	const PsiIcon *current_status_icon;
	QString last_contact_tooltip;
	int unacked_messages;

	static PsiIcon *throbber_icon;
};

#endif
