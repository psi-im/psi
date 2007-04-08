/*
 * psiwidgets.h - plugin for loading Psi's custom widgets into Qt Designer
 * Copyright (C) 2003-2005  Michail Pishchagin
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

#ifndef PSIWIDGETSPLUGIN_H
#define PSIWIDGETSPLUGIN_H

#include <QtDesigner/QDesignerCustomWidgetInterface>

#include <QtCore/qplugin.h>
#include <QtGui/QIcon>

class PsiWidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
	Q_OBJECT
	Q_INTERFACES( QDesignerCustomWidgetInterface )
public:
	PsiWidgetPlugin(QObject *parent = 0);

	virtual QWidget *createWidget(QWidget *parent);
	virtual QString name() const;
	virtual QString group() const;
	virtual QString toolTip() const;
	virtual QString whatsThis() const;
	virtual QString includeFile() const;

	virtual QString codeTemplate() const;
	virtual QString domXml() const;
	virtual QIcon icon() const;
	virtual bool isContainer() const;

	void initialize( QDesignerFormEditorInterface * );
	bool isInitialized() const;

private:
	bool initialized;
};

#endif
