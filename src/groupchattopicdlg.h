#ifndef GROUPCHATTOPICDLG_H
#define GROUPCHATTOPICDLG_H

#include "languagemanager.h"

#include <QPointer>
#include <QDialog>

namespace Ui {
	class GroupchatTopicDlg;
	class GroupChatTopicAddLangDlg;
}

class GCMainDlg;

class GroupchatTopicDlg : public QDialog {
	Q_OBJECT
public:
	GroupchatTopicDlg(GCMainDlg *parent = 0);
	~GroupchatTopicDlg();
	QMap<LanguageManager::LangId,QString> subjectMap() const;
	void setSubjectMap(const QMap<LanguageManager::LangId,QString> &topics);

protected:
	void changeEvent(QEvent *e);
private:
	Ui::GroupchatTopicDlg *m_ui;
	Ui::GroupChatTopicAddLangDlg *m_addLangUi;
	QPointer<QDialog> addLangDlg;

	void addLanguage(const LanguageManager::LangId &id, const QString &text = QString());
	void populateCountryAndScript();
};

#endif // GROUPCHATTOPICDLG_H
