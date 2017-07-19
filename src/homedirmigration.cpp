/*
 * homedirmigration.cpp
 * Copyright (C) 2011  Ivan Romanov <drizt@land.ru>
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

#include <QSignalMapper>
#include <QQueue>
#include <QPair>
#include <QMessageBox>
#include <QThread>
#include <QCloseEvent>
#include <QRegExp>

#include "applicationinfo.h"
#include "homedirmigration.h"
#include "ui_homedirmigration.h"
#if defined Q_OS_WIN
#include "psiapplication.h"
#endif

class Thread : public QThread
{
public:
	Thread(HomeDirMigration::Choice choice, QDir oldHomeDir, QDir configDir, QDir dataDir, QDir cacheDir, QObject *parent = NULL)
		: QThread(parent)
		, choice_(choice)
		, oldHomeDir_(oldHomeDir)
		, configDir_(configDir)
		, dataDir_(dataDir)
		, cacheDir_(cacheDir)
		, stop_(false)
	{
	}

	bool result() const
	{
		return result_;
	}

	void stop()
	{
		stop_ = true;
	}

protected:
	void run()
	{
		switch((HomeDirMigration::Choice)choice_) {
		case HomeDirMigration::Copy:
			result_ = copyFolder();
			if (!result_) {
				removeFolder(configDir_.path());
#if defined HAVE_X11
				removeFolder(dataDir_.path());
#endif
#if defined HAVE_X11 || defined Q_OS_MAC
				removeFolder(cacheDir_.path());
#endif
			}
			break;

		case HomeDirMigration::Move:
			result_ = copyFolder();
			if (result_) {
				removeFolder(oldHomeDir_.path());
			}
			else {
				removeFolder(configDir_.path());
#if defined HAVE_X11
				removeFolder(dataDir_.path());
#endif
#if defined HAVE_X11 || defined Q_OS_MAC
				removeFolder(cacheDir_.path());
#endif
			}
			break;

		case HomeDirMigration::Nothing:
			break;
		}
	}

	void removeFolder(const QString& folder)
	{
		QDir dir(folder);
		QFileInfoList infoList = dir.entryInfoList(QDir::Files | QDir::Hidden);
		QStringList files;
		foreach(QFileInfo info, infoList) {
			if(info.isFile() && info.isWritable() && info.isReadable())
				files << info.fileName();
		}

		// Remove all file in current directory
		QStringList::Iterator itFile = files.begin();
		while (itFile != files.end()) {
			QFile file(folder + "/" + *itFile);
			file.remove();
			itFile++;
		}

		// Get list of all subdirectories
		infoList = dir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
		QStringList dirs;
		foreach(QFileInfo info, infoList) {
			if(info.isDir())
				dirs << info.fileName();
		}
		QStringList::Iterator itDir = dirs.begin();
		while (itDir != dirs.end()) {
			if (*itDir != "." && *itDir != "..") {
				// and invoke remove for each
				removeFolder(folder + "/" + *itDir);
			}
			itDir++;
		}

		// Remove myself
		// Any dir always have two subdir as dotdir and dotanddotdir
		if (dir.count() == 2) {
			dir.rmdir(folder);
		}
	}

	bool copyFolder()
	{
		QQueue<QString> queue;

		queue.enqueue("");

		while (!queue.isEmpty()) {
			QString srcDirName = queue.dequeue();
			QDir srcDir(oldHomeDir_.path() + "/" + srcDirName);
			if (!srcDir.exists()) {
				continue;
			}

			QFileInfoList infoList = srcDir.entryInfoList(QDir::Files | QDir::Hidden);
			QStringList files;
			foreach(QFileInfo info, infoList) {
				if(info.isFile() && info.isReadable())
					files << info.fileName();
			}

			for (int i = 0; i < files.count(); i++) {
				if (stop_) {
					return false;
				}
				QString srcFileName;
				srcFileName = srcDirName + "/" + files[i];

				QString dstDirName;
				QRegExp settingsXp("^/psirc$|^/profiles/\\w*/accounts.xml$|^/profiles/\\w*/accounts.xml.backup$|"
								   "^/profiles/\\w*/options.xml$|^/profiles/\\w*/options.xml.backup$|^/profiles/\\w*/mucskipautojoin.txt$");

				QRegExp cacheXp("^/tmp-contentdownloader/.*$|^/tmp-sounds/.*$|^/tmp-pics/.*$|"
								"^/bob/.*|^/avatars/.*$|^/profiles/\\w*/vcard/.*$|^/caps.xml$|^/tune$");

				if (srcFileName.contains(settingsXp)) {
					dstDirName = configDir_.path() + "/" + srcDirName;
				}
				else if (srcFileName.contains(cacheXp)) {
					dstDirName = cacheDir_.path() + "/" + srcDirName;
				}
				else {
					dstDirName = dataDir_.path() + "/" + srcDirName;
				}

				QDir dstDir(dstDirName);
				if (!dstDir.exists()) {
					dstDir.mkpath(".");
				}

				QString destFileName = dstDirName + "/" + files[i];
				if (!QFile::copy(oldHomeDir_.path() + srcFileName, destFileName)) {
					qFatal("Critical error while importing old profile");
					return false;
				}
			}

			infoList = srcDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
			QStringList dirs;
			foreach(QFileInfo info, infoList)
			{
				if(info.isDir())
					dirs << info.fileName();
			}

			for (int i = 0; i < dirs.count(); i++) {
				queue.enqueue(srcDirName + "/" + dirs[i]);
			}
		}
		return true;
	}

	HomeDirMigration::Choice choice_;
	QDir oldHomeDir_;
	QDir configDir_;
	QDir dataDir_;
	QDir cacheDir_;
	bool result_;
	bool stop_;
};

HomeDirMigration::HomeDirMigration(QWidget *parent)
	: QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint |
			  Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint)
	, ui(new Ui::HomeDirMigration)
	, configDir_(ApplicationInfo::homeDir(ApplicationInfo::ConfigLocation))
	, dataDir_(ApplicationInfo::homeDir(ApplicationInfo::DataLocation))
	, cacheDir_(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation))
	, choice_(Copy)
	, thread_(NULL)
{
}

bool HomeDirMigration::checkOldHomeDir()
{
#if defined HAVE_X11 || defined Q_OS_MAC
	QString base = QDir::homePath();

	if (!oldHomeDir_.exists()) {
		oldHomeDir_.setPath(base + "/.psi");
	}
#elif defined Q_OS_WIN
	QString base = QFileInfo(QCoreApplication::applicationFilePath()).fileName()
			.toLower().indexOf("portable") == -1?
				"" : QCoreApplication::applicationDirPath();
	if (base.isEmpty()) {
		// Windows 9x
		if(QDir::homePath() == QDir::rootPath()) {
			base = ".";
		}
		// Windows NT/2K/XP variant
		else {
			base = QDir::homePath();
		}
	}
	// no trailing slash
	base = QDir::cleanPath(base);

	oldHomeDir_.setPath(base + "/PsiData");
#endif
	return oldHomeDir_.exists();
}

QString HomeDirMigration::oldHomeDir() const
{
	return oldHomeDir_.path();
}

void HomeDirMigration::threadFinish()
{
	ui->busy->stop();
	QDialog::accept();
}

int HomeDirMigration::exec()
{
	ui->setupUi(this);

	adjustSize();
	setFixedSize(size());
	QSignalMapper *mapper = new QSignalMapper(this);

	// Mapping to copy radio button
	connect(ui->rbCopy, SIGNAL(clicked()), mapper, SLOT(map()));
	mapper->setMapping(ui->rbCopy, Copy);

	// Mapping to move radio button
	connect(ui->rbMove, SIGNAL(clicked()), mapper, SLOT(map()));
	mapper->setMapping(ui->rbMove, Move);

	// Mapping to nothing radio button
	connect(ui->rbNothing, SIGNAL(clicked()), mapper, SLOT(map()));
	mapper->setMapping(ui->rbNothing, Nothing);

	connect(mapper, SIGNAL(mapped(int)), SLOT(setChoice(int)));

	return QDialog::exec();
}

void HomeDirMigration::closeEvent(QCloseEvent *event)
{
	event->ignore();
	if (!thread_) {
		exit(0);
	}
	else {
		thread_->stop();
		thread_->wait();
		exit(0);
	}
}

void HomeDirMigration::accept()
{
	if (choice_ == Copy || choice_ == Move) {
		thread_ = new Thread(choice_, oldHomeDir_, configDir_, dataDir_, cacheDir_, this);
		connect(thread_, SIGNAL(finished()), SLOT(threadFinish()));
		thread_->start();
		ui->busy->start();
		setEnabled(false);
	}
	else {
		QDialog::accept();
	}
}

void HomeDirMigration::setChoice(int choice)
{
	choice_ = (Choice)choice;
}

HomeDirMigration::~HomeDirMigration()
{
	delete ui;
}
