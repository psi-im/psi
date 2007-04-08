/*
 * qca_plugin.h - Qt Cryptographic Architecture
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2004,2005  Brad Hards <bradh@frogmouth.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef QCA_PLUGIN_H
#define QCA_PLUGIN_H

// NOTE: this API is private to QCA

#include "qca_core.h"

namespace QCA
{
	class ProviderItem;

	class ProviderManager
	{
	public:
		ProviderManager();
		~ProviderManager();

		void scan();
		bool add(Provider *p, int priority);
		void unload(const QString &name);
		void unloadAll();
		void setDefault(Provider *p);
		Provider *find(Provider *p) const;
		Provider *find(const QString &name) const;
		Provider *findFor(const QString &name, const QString &type) const;
		void changePriority(const QString &name, int priority);
		int getPriority(const QString &name);
		QStringList allFeatures() const;
		const ProviderList & providers() const;

		static void mergeFeatures(QStringList *a, const QStringList &b);

		QString diagnosticText() const;
		void appendDiagnosticText(const QString &str);
		void clearDiagnosticText();

	private:
		QString dtext;
		QList<ProviderItem*> providerItemList;
		ProviderList providerList;
		Provider *def;
		bool scanned_static;
		void addItem(ProviderItem *i, int priority);
		bool haveAlready(const QString &name) const;
	};
}

#endif
