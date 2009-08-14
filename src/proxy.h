/*
 * proxy.h - classes for handling proxy profiles
 * Copyright (C) 2003  Justin Karneges
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef PROXYDLG_H
#define PROXYDLG_H

#include <QList>
#include <QGroupBox>

#include "ui_proxy.h"
#include "ui_proxyedit.h"

class OptionsTree;
class QDomElement;
class QDomDocument;

class ProxyItem;
class ProxyManager;
typedef QList<ProxyItem> ProxyItemList;

class ProxySettings
{
public:
	ProxySettings();

	QString host, user, pass;
	int port;
	bool useAuth;
	QString url;

	
	void toOptions(OptionsTree* o, QString base) const;
	void fromOptions(OptionsTree* o, QString base);
	QDomElement toXml(QDomDocument *) const;
	bool fromXml(const QDomElement &);
};

class ProxyEdit : public QWidget
{
	Q_OBJECT
public:
	ProxyEdit(QWidget* parent);
	~ProxyEdit();

	void reset();
	void setType(const QString &s);
	ProxySettings proxySettings() const;
	void setProxySettings(const ProxySettings &);

private:
	Ui::ProxyEdit ui_;
};

class ProxyDlg : public QDialog, public Ui::Proxy
{
	Q_OBJECT
public:
	ProxyDlg(const ProxyItemList &, const QStringList &, const QString &def, QWidget *parent=0);
	~ProxyDlg();

signals:
	void applyList(const ProxyItemList &, int cur);

private slots:
	void proxy_new();
	void proxy_remove();
	void cb_activated(int);
	void qlbx_highlighted(int);
	void qle_textChanged(const QString &);
	void doSave();

private:
	class Private;
	Private *d;

	void selectCurrent();
	QString getUniqueName() const;
	void hookEdit();
	void unhookEdit();
	void saveIntoItem(int);
};

class ProxyChooser : public QWidget
{
	Q_OBJECT
public:
	ProxyChooser(ProxyManager *, QWidget *parent=0, const char *name=0);
	~ProxyChooser();

	QString currentItem() const;
	void setCurrentItem(const QString &item);

private slots:
	void pm_settingsChanged();
	void pm_settingsChangedApply();
	void doOpen();

private:
	class Private;
	Private *d;

	void buildComboBox();
};

class ProxyItem
{
public:
	ProxyItem() {}

	QString id;
	QString name;
	QString type;
	ProxySettings settings;
};

class ProxyManager : public QObject
{
	Q_OBJECT
public:
	ProxyManager(OptionsTree *o, QObject *parent=0);
	~ProxyManager();

	ProxyChooser *createProxyChooser(QWidget *parent=0);
	ProxyItemList itemList() const;
	ProxyItem getItem(const QString &id) const;
	QString lastEdited() const;
	void migrateItemList(const ProxyItemList &);
	QStringList methodList() const;
//	int findOldIndex(int) const;

signals:
	void settingsChanged();
	void proxyRemoved(QString);

public slots:
	void openDialog(QString);

private slots:
	void pd_applyList(const ProxyItemList &, int cur);

private:
	class Private;
	Private *d;
};

#endif
