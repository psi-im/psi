/*
 * fileutil.h - common file dialogs
 * Copyright (C) 2008  Michail Pishchagin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "fileutil.h"

#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include "psioptions.h"

static QString lastUsedOpenPathOptionPath = "options.ui.last-used-open-path";
static QString lastUsedSavePathOptionPath = "options.ui.last-used-save-path";

QString FileUtil::lastUsedOpenPath()
{
	return PsiOptions::instance()->getOption(lastUsedOpenPathOptionPath).toString();
}

void FileUtil::setLastUsedOpenPath(const QString& path)
{
	QFileInfo fi(path);
	if (fi.exists()) {
		PsiOptions::instance()->setOption(lastUsedOpenPathOptionPath, path);
	}
}

QString FileUtil::lastUsedSavePath()
{
	return PsiOptions::instance()->getOption(lastUsedSavePathOptionPath).toString();
}

void FileUtil::setLastUsedSavePath(const QString& path)
{
	QFileInfo fi(path);
	if (fi.exists()) {
		PsiOptions::instance()->setOption(lastUsedSavePathOptionPath, path);
	}
}

QString FileUtil::getOpenFileName(QWidget* parent, const QString& caption, const QString& filter, QString* selectedFilter)
{
	while (1) {
		if (lastUsedOpenPath().isEmpty()) {
			setLastUsedOpenPath(QDir::homePath());
		}
		QString fileName = QFileDialog::getOpenFileName(parent, caption, lastUsedOpenPath(), filter, selectedFilter);
		if (!fileName.isEmpty()) {
			QFileInfo fi(fileName);
			if (!fi.exists()) {
				QMessageBox::information(parent, tr("Error"), tr("The file specified does not exist."));
				continue;
			}

			setLastUsedOpenPath(fi.path());
			return fileName;
		}
		break;
	}

	return QString();
}

QString FileUtil::getSaveFileName(QWidget* parent, const QString& caption, const QString& defaultFileName, const QString& filter, QString* selectedFilter)
{
	if (lastUsedSavePath().isEmpty()) {
		if (!lastUsedOpenPath().isEmpty()) {
			setLastUsedSavePath(lastUsedOpenPath());
		}
		else {
			setLastUsedSavePath(QDir::homePath());
		}
	}

	QString dir = QDir(lastUsedSavePath()).filePath(defaultFileName);
	QString fileName = QFileDialog::getSaveFileName(parent, caption, dir, filter, selectedFilter);
	if (!fileName.isEmpty()) {
		QFileInfo fi(fileName);
		if (QDir(fi.path()).exists()) {
			setLastUsedSavePath(fi.path());
			return fileName;
		}
	}

	return QString();
}

QString FileUtil::getImageFileName(QWidget* parent)
{
	// double extenstions because of QTBUG-51712
	return FileUtil::getOpenFileName(parent, tr("Choose a file"),
	                                 tr("Images (*.png *.xpm *.jpg *.jpeg *.webp *.PNG *.XPM *.JPG *.JPEG *.WEBP)"));
}

QString FileUtil::mimeToFileExt(const QString &mime)
{
	static QMap<QString, QString> mimes;
	if (!mimes.size()) {
		mimes["image/png"]     = QLatin1String("png");
		mimes["image/x-mng"]   = QLatin1String("mng");
		mimes["image/gif"]     = QLatin1String("gif");
		mimes["image/bmp"]     = QLatin1String("bmp");
		mimes["image/x-xpm"]   = QLatin1String("xpm");
		mimes["image/svg+xml"] = QLatin1String("svg");
		mimes["image/jpeg"]    = QLatin1String("jpg");
		mimes["image/webp"]    = QLatin1String("webp");

		mimes["application/octet-stream"] = "bin";
		// add more when needed
	}
	return mimes.value(mime);
}
