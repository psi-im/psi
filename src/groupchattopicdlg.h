#ifndef GROUPCHATTOPICDLG_H
#define GROUPCHATTOPICDLG_H

#include <QtGui/QDialog>

namespace Ui {
	class GroupchatTopicDlg;
}

class GCMainDlg;

class GroupchatTopicDlg : public QDialog {
	Q_OBJECT
public:
	GroupchatTopicDlg(GCMainDlg *parent = 0);
	~GroupchatTopicDlg();
	QString topic() const;

protected:
	void changeEvent(QEvent *e);

private:
	Ui::GroupchatTopicDlg *m_ui;

public slots:
	void accept();

signals:
	void topicAccepted(const QString &);
};

#endif // GROUPCHATTOPICDLG_H
