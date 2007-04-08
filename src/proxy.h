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
#include <q3groupbox.h>
#include <qdom.h>
#include "ui_proxy.h"

class HostPortEdit : public QWidget
{
	Q_OBJECT
public:
	HostPortEdit(QWidget *parent=0, const char *name=0);
	~HostPortEdit();

	QString host() const;
	int port() const;
	void setHost(const QString &);
	void setPort(int);
	void fixTabbing(QWidget *a, QWidget *b);

private:
	class Private;
	Private *d;
};

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

	QDomElement toXml(QDomDocument *) const;
	bool fromXml(const QDomElement &);
};

class ProxyEdit : public Q3GroupBox
{
	Q_OBJECT
public:
	ProxyEdit(QWidget *parent=0, const char *name=0);
	~ProxyEdit();

	void reset();
	void setType(const QString &s);
	ProxySettings proxySettings() const;
	void setProxySettings(const ProxySettings &);
	void fixTabbing(QWidget *a, QWidget *b);

private slots:
	void ck_toggled(bool);

private:
	class Private;
	Private *d;
};

class ProxyDlg : public QDialog, public Ui::Proxy
{
	Q_OBJECT
public:
	ProxyDlg(const ProxyItemList &, const QStringList &, int def, QWidget *parent=0);
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

	int currentItem() const;
	void setCurrentItem(int);
	void fixTabbing(QWidget *a, QWidget *b);

private slots:
	void pm_settingsChanged();
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

	int id; // used to keep track of 'old' position in a list
	QString name;
	QString type;
	ProxySettings settings;
};

class ProxyManager : public QObject
{
	Q_OBJECT
public:
	ProxyManager(QObject *parent=0);
	~ProxyManager();

	ProxyChooser *createProxyChooser(QWidget *parent=0);
	ProxyItemList itemList() const;
	const ProxyItem & getItem(int) const;
	int lastEdited() const;
	void setItemList(const ProxyItemList &);
	QStringList methodList() const;
	int findOldIndex(int) const;

signals:
	void settingsChanged();

public slots:
	void openDialog(int);

private slots:
	void pd_applyList(const ProxyItemList &, int cur);

private:
	class Private;
	Private *d;

	void assignIds();
};

#endif
