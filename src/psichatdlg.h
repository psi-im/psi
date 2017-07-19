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
#include "actionlist.h"

class IconAction;
class PsiContact;

class PsiChatDlg : public ChatDlg
{
	Q_OBJECT
public:
	PsiChatDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager);
	~PsiChatDlg();

protected:
	// reimplemented
	void contextMenuEvent(QContextMenuEvent *);
	void doSend();
	bool eventFilter(QObject *obj, QEvent *event);

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
	void updateToolbuttons();
	void doSwitchJidMode();
	void copyUserJid();
	void actActiveContacts();
	void actPgpToggled(bool);

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
	void checkPGPAutostart();
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

	ActionList* actions_;
	QAction *act_mini_cmd_;
	TypeAheadFindBar *typeahead_;

	ActionLineEdit *le_autojid;
	IconAction *act_autojid;
	IconAction *act_active_contacts;

	MCmdManager mCmdManager_;
	MCmdSimpleSite mCmdSite_;

	MCmdTabCompletion tabCompletion;

	bool autoPGP_;
	bool smallChat_;
	class ChatDlgMCmdProvider;

	static PsiIcon *throbber_icon;
};

#endif
