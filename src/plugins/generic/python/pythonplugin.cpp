/*
 * pythonscriptingplugin.h - Psi plugin providing Python scripting
 * Copyright (C) 2006  Kevin Smith
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
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

#include <Python.h>

#include <QtCore>
#include <QDebug>

#include "psiplugin.h"

extern int Py_VerboseFlag;

/**
 * Function to obtain all the directories in which plugins can be stored
 * \return List of plugin directories
 */ 
static QStringList scriptDirs()
{
	QStringList l;
	l += "/Users/kismith/.psi/python";
	return l;
}

class PythonPlugin : public PsiPlugin
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin)

public:
	PythonPlugin();
	~PythonPlugin();
	virtual QString name() const; 
	virtual QString shortName() const;
	virtual QString version() const;
	virtual bool processEvent( const QString& account, QDomNode &event );
	
private:
	QStringList scriptObjects_;
	void loadScripts();
	QString loadScript( const QString& fileName);
	PyObject* main_module_;
	PyObject* main_dict_;

};

Q_EXPORT_PLUGIN(PythonPlugin);

PythonPlugin::PythonPlugin()
{
	Py_Initialize();
	main_module_ = PyImport_AddModule("__main__");
	main_dict_ = PyModule_GetDict(main_module_);
	QString command="print \"Python running\"";
	PyRun_SimpleString(qPrintable(command));
	loadScripts();
	
}

PythonPlugin::~PythonPlugin()
{
	Py_Finalize();
}

QString PythonPlugin::name() const
{
	return "Python Scripting Plugin";
}

QString PythonPlugin::shortName() const
{
	return "python";
}

QString PythonPlugin::version() const
{
	return "0.0";
}

bool PythonPlugin::processEvent( const QString& account, QDomNode &event )
{
	foreach (QString script, scriptObjects_)
	{
		qDebug() << (qPrintable(QString("running it on script %1").arg(script)));
		QString scriptCall=QString("%1.processEvent(\"\"\"%2\"\"\")").arg(script).arg(toString(event));
		QString command=QString("%1").arg(scriptCall);
		qDebug() << (qPrintable(QString("Running python command:\n%1").arg(command)));
		PyRun_SimpleString(qPrintable(command));
	}
	return true;
}

void PythonPlugin::loadScripts()
{
	foreach(QString d, scriptDirs()) {
		QDir dir(d);
		foreach(QString file, dir.entryList()) {
		  	file=dir.absoluteFilePath(file);
			if (file.contains(".py"))
				loadScript(file);
		}
	}
}

QString PythonPlugin::loadScript(const QString& fileName)
{
	FILE *file;
	if ((file = fopen(qPrintable(fileName),"r")) == NULL )
		return "";
	qDebug() << (qPrintable(QString("Found script file %1").arg(fileName)));
	PyObject* pyName = PyRun_File(file, qPrintable(fileName), Py_file_input, main_dict_, main_dict_);
	qDebug("well, we got this far");
	fclose(file);
	pyName = PyDict_GetItemString( main_dict_, "name" );
	if (pyName == NULL || !PyString_Check(pyName))
	{
		qWarning(qPrintable(QString("Tried to load %1 but it didn't return a string for its name").arg(fileName)));
		return "";
	}
	QString name( PyString_AsString( pyName ) );
	scriptObjects_.append(name);
	qDebug() << (qPrintable(QString("Found script %1 in the file").arg(name)));
	return name;
}

#include "pythonplugin.moc"
