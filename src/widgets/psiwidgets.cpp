/*
 * psiwidgets.cpp - plugin for loading Psi's custom widgets into Qt Designer
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

#include "psiwidgets.h"

#include "busywidget.h"
#include "fancylabel.h"
#include "iconwidget.h"
#include "iconlabel.h"
#include "iconbutton.h"
#include "iconsetselect.h"
#include "iconsetdisplay.h"
#include "icontoolbutton.h"
#include "urllabel.h"
#include "psitextview.h"

static const char *psiwidget_data[] = {
"16 16 5 1",
". c None",
"# c #000000",
"c c #57acda",
"b c #72bde6",
"a c #cde9f8",
".###..####..###.",
"#aaa#.#aa#.#aaa#",
"#abbb##ac##abcc#",
".##ab##ac##ac##.",
"..#abc#ac#abc#..",
"..#abc#ac#abc#..",
"..#abc#ac#abc#..",
"..#abc#ac#abc#..",
"..#abbbbbbbbc#..",
"...#abbbbbcc#...",
"....##cccc##....",
"......#ac#......",
"......#ac#......",
"......#ac#......",
"......#ac#......",
"......####......"};

//----------------------------------------------------------------------------
// PsiWidgetPlugin - base plugin class to avoid code duplication
//----------------------------------------------------------------------------

PsiWidgetPlugin::PsiWidgetPlugin( QObject *parent )
	: QObject( parent )
{
	initialized = false;
}

QWidget *PsiWidgetPlugin::createWidget(QWidget */* parent */)
{
	return 0;
}

QString PsiWidgetPlugin::name() const
{
	return "Psi Plugin";
}

QString PsiWidgetPlugin::group() const
{
	return "Psi";
}

QString PsiWidgetPlugin::toolTip() const
{
	return name();
}

QString PsiWidgetPlugin::whatsThis() const
{
	return "Psi Widget";
}

QString PsiWidgetPlugin::includeFile() const
{
	return "psiwidget.h";
}

QString PsiWidgetPlugin::codeTemplate() const
{
	return QString();
}

QString PsiWidgetPlugin::domXml() const
{
	return QString();
}

QIcon PsiWidgetPlugin::icon() const
{
	return QIconSet( QPixmap( (const char **)psiwidget_data ) );
}

bool PsiWidgetPlugin::isContainer() const
{
	return false;
}

void PsiWidgetPlugin::initialize( QDesignerFormEditorInterface * )
{
	if ( initialized )
		return;

	initialized = true;
}

bool PsiWidgetPlugin::isInitialized() const
{
	return initialized;
}

//----------------------------------------------------------------------------
// BusyWidgetPlugin
//----------------------------------------------------------------------------

class BusyWidgetPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	BusyWidgetPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new BusyWidget( parent );
	}

	QString name() const
	{
		return "BusyWidget";
	}

	QString domXml() const
	{
		return "<widget class=\"BusyWidget\" name=\"busyWidget\">\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "Widget for indicating that program is doing something.";
	}

	QString includeFile() const
	{
		return "busywidget.h";
	}
};


//----------------------------------------------------------------------------
// IconLabelPlugin
//----------------------------------------------------------------------------

class IconLabelPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	IconLabelPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new IconLabel( parent );
	}

	QString name() const
	{
		return "IconLabel";
	}

	QString domXml() const
	{
		return "<widget class=\"IconLabel\" name=\"iconLabel\">\n"
		       " <property name=\"geometry\">\n"
		       "  <rect>\n"
		       "   <x>0</x>\n"
		       "   <y>0</y>\n"
		       "   <width>100</width>\n"
		       "   <height>100</height>\n"
		       "  </rect>\n"
		       " </property>\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "Label that can contain animated PsiIcon.";
	}

	QString includeFile() const
	{
		return "iconlabel.h";
	}
};

//----------------------------------------------------------------------------
// FancyLabelPlugin
//----------------------------------------------------------------------------

class FancyLabelPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	FancyLabelPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new FancyLabel( parent );
	}

	QString name() const
	{
		return "FancyLabel";
	}

	QString domXml() const
	{
		return "<widget class=\"FancyLabel\" name=\"fancyLabel\">\n"
		       " <property name=\"geometry\">\n"
		       "  <rect>\n"
		       "   <x>0</x>\n"
		       "   <y>0</y>\n"
		       "   <width>100</width>\n"
		       "   <height>100</height>\n"
		       "  </rect>\n"
		       " </property>\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "Just a Fancy Label. Use it for decoration of dialogs. ;-)";
	}

	QString includeFile() const
	{
		return "fancylabel.h";
	}
};

//----------------------------------------------------------------------------
// IconsetSelectPlugin
//----------------------------------------------------------------------------

class IconsetSelectPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	IconsetSelectPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new IconsetSelect( parent );
	}

	QString name() const
	{
		return "IconsetSelect";
	}

	QString domXml() const
	{
		return "<widget class=\"IconsetSelect\" name=\"iconsetSelect\">\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "Widget for Iconset selection.";
	}

	QString includeFile() const
	{
		return "iconsetselect.h";
	}
};

//----------------------------------------------------------------------------
// IconsetDisplayPlugin
//----------------------------------------------------------------------------

class IconsetDisplayPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	IconsetDisplayPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new IconsetDisplay( parent );
	}

	QString name() const
	{
		return "IconsetDisplay";
	}

	QString domXml() const
	{
		return "<widget class=\"IconsetDisplay\" name=\"iconsetDisplay\">\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "Displays all icons in Iconset.";
	}

	QString includeFile() const
	{
		return "iconsetdisplay.h";
	}
};

//----------------------------------------------------------------------------
// IconButtonPlugin
//----------------------------------------------------------------------------

class IconButtonPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	IconButtonPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new IconButton( parent );
	}

	QString name() const
	{
		return "IconButton";
	}

	QString domXml() const
	{
		return "<widget class=\"IconButton\" name=\"iconButton\">\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "PushButton that can contain animated PsiIcon.";
	}

	QString includeFile() const
	{
		return "iconbutton.h";
	}
};

//----------------------------------------------------------------------------
// IconToolButtonPlugin
//----------------------------------------------------------------------------

class IconToolButtonPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	IconToolButtonPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new IconToolButton( parent );
	}

	QString name() const
	{
		return "IconToolButton";
	}

	QString domXml() const
	{
		return "<widget class=\"IconToolButton\" name=\"iconToolButton\">\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "ToolButton that can contain animated PsiIcon.";
	}

	QString includeFile() const
	{
		return "icontoolbutton.h";
	}
};

//----------------------------------------------------------------------------
// PsiTextViewPlugin
//----------------------------------------------------------------------------

class PsiTextViewPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	PsiTextViewPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new PsiTextView( parent );
	}

	QString name() const
	{
		return "PsiTextView";
	}

	QString domXml() const
	{
		return "<widget class=\"PsiTextView\" name=\"PsiTextView\">\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "Widget for displaying rich-text data, with inline Icons.";
	}

	QString includeFile() const
	{
		return "psitextview.h";
	}
};


//----------------------------------------------------------------------------
// URLLabelPlugin
//----------------------------------------------------------------------------

class URLLabelPlugin : public PsiWidgetPlugin
{
	Q_OBJECT
public:
	URLLabelPlugin( QObject *parent = 0 )
		: PsiWidgetPlugin( parent )
	{
		// nothing to do
	}

	QWidget *createWidget(QWidget *parent)
	{
		return new URLLabel( parent );
	}

	QString name() const
	{
		return "URLLabel";
	}

	QString domXml() const
	{
		return "<widget class=\"URLLabel\" name=\"URLLabel\">\n"
		       " <property name=\"url\">\n"
		       "  <string>http://host</string>\n"
		       " </property>\n"
		       " <property name=\"title\">\n"
		       "  <string>The Title</string>\n"
		       " </property>\n"
		       "</widget>\n";
	}

	QString whatsThis() const
	{
		return "Widget for displaying clickable URLs.";
	}

	QString includeFile() const
	{
		return "urllabel.h";
	}
};

//----------------------------------------------------------------------------
// AllPsiWidgetsPlugin
//----------------------------------------------------------------------------

class AllPsiWidgetsPlugin : public QObject, public QDesignerCustomWidgetCollectionInterface
{
	Q_OBJECT
	Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)
public:
	AllPsiWidgetsPlugin(QObject *parent = 0)
		: QObject( parent )
	{
		plugins.append( new BusyWidgetPlugin( this ) );
		plugins.append( new IconLabelPlugin( this ) );
		plugins.append( new FancyLabelPlugin( this ) );
		plugins.append( new IconsetSelectPlugin( this ) );
		plugins.append( new IconsetDisplayPlugin( this ) );
		plugins.append( new IconButtonPlugin( this ) );
		plugins.append( new IconToolButtonPlugin( this ) );
		plugins.append( new PsiTextViewPlugin( this ) );
		plugins.append( new URLLabelPlugin( this ) );
	}

	virtual QList<QDesignerCustomWidgetInterface*> customWidgets() const
	{
		return plugins;
	}

private:
	QList<QDesignerCustomWidgetInterface*> plugins;
};

Q_EXPORT_PLUGIN( AllPsiWidgetsPlugin );

#include "psiwidgets.moc"
