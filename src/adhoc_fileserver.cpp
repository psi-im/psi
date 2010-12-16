/*
 * adhoc_fileserver.cpp - Implementation of a personal ad-hoc fileserver
 * Copyright (C) 2005  Remko Troncon
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

#include <QDir>
#include <QFileInfo>

#include "psiaccount.h"
#include "adhoc_fileserver.h"
#include "xmpp_xdata.h"

using namespace XMPP;

bool AHFileServer::isAllowed(const Jid& j) const
{
	return manager()->account()->jid().compare(j,false);
}

AHCommand AHFileServer::execute(const AHCommand& c, const Jid& requester)
{
	// Extract the file
	QString file;
	if (c.hasData()) {
		QString fileName, curDir;
		XData::FieldList fl = c.data().fields();
		for (unsigned int i=0; i < fl.count(); i++) {
			if (fl[i].var() == "file" && !(fl[i].value().isEmpty())) {
				file = fl[i].value().first();
			}
		}
	}
	else {
		file = QDir::currentDirPath();
	}

	if (QFileInfo(file).isDir()) {
		// Return a form with a filelist
		XData form;
		form.setTitle(QObject::tr("Choose file"));
		form.setInstructions(QObject::tr("Choose a file"));
		form.setType(XData::Data_Form);
		XData::FieldList fields;

		XData::Field files_field;
		files_field.setType(XData::Field::Field_ListSingle);
		files_field.setVar("file");
		files_field.setLabel(QObject::tr("File"));
		files_field.setRequired(true);

		XData::Field::OptionList file_options;
     	QDir d(file);
		//d.setFilter(QDir::Files|QDir::Hidden|QDir::NoSymLinks);
		QStringList l = d.entryList();
		for (QStringList::Iterator it = l.begin(); it != l.end(); ++it ) {
			XData::Field::Option file_option;
			QFileInfo fi(QDir(file).filePath(*it));
			file_option.label = *it + (fi.isDir() ? QString(" [DIR]") : QString(" (%1 bytes)").arg(QString::number(fi.size())));
			file_option.value = QDir(file).absFilePath(*it);
			file_options += file_option;
		}
		files_field.setOptions(file_options);
		fields += files_field;
		
		form.setFields(fields);

		return AHCommand::formReply(c, form);
	}
	else {
		QStringList l(file);
		manager()->account()->sendFiles(requester,l,true);
		return AHCommand::completedReply(c);
	}
}
