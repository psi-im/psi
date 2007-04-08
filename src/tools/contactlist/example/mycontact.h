#ifndef MYCONTACT_H
#define MYCONTACT_H

#include <QString>
#include <QIcon>
#include <QMenu>

#include "status.h"
#include "contactlistcontact.h"

class MyContact : public ContactListContact
{
public:
	MyContact(const QString& name, const QString& jid, const QString& pic, const Status& status, ContactListGroupItem* parent = 0) : ContactListContact(parent), name_(name), jid_(jid), status_(status) {
		if (pic.isEmpty())
			picture_ = QIcon(":/pictures/unknown.png");
		else
			picture_ = QIcon(pic);

		updateParent();
	}

	virtual const QString& name() const { return name_; }
	virtual QString jid() const { return jid_; }
	virtual Status status() const { return status_; }
	virtual QIcon picture() const { return picture_; }
	virtual QIcon statusIcon() const {
		if (status_.type() == Status::Online || status_.type() == Status::FFC) {
			return QIcon(":/icons/online.png");
		}
		else if (status_.type() == Status::Offline) {
			return QIcon(":/icons/offline.png");
		}
		else {
			return QIcon(":/icons/away.png");
		}
	}
	virtual void showContextMenu(const QPoint& where) {
		QMenu menu(NULL);
		menu.addAction("Rename");
		menu.addAction("Remove");
		menu.exec(where);
	}

private:
	QString name_, jid_;
	QIcon picture_;
	Status status_;
};

#endif
