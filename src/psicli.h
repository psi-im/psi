#ifndef PSICLI_H
#define PSICLI_H

#include "simplecli/simplecli.h"
#include "applicationinfo.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>

class PsiCli : public SimpleCli
{
	Q_OBJECT
public:
	PsiCli() {
		defineParam("profile", tr("PROFILE", "translate in UPPER_CASE with no spaces"),
			    tr("Activate program instance running specified profile. "
			       "Otherwise, open new instance using this profile "
			       "(unless used together with --remote).",
					"do not translate --remote"));

		defineSwitch("remote",
			     tr("Force remote-control mode. "
				"If there is no running instance, "
				"or --profile was specified but there is no instance using it, "
				"exit without doing anything. "
				"Cannot be used with --choose-profile.",
					"do not translate --profile, --choose-profile"));

		defineSwitch("choose-profile",
			     tr("Display Choose Profile dialog on startup. "
				"Cannot be used together with --remote.",
					"do not translate --remote"));

		defineParam("uri", tr("URI", "translate in UPPER_CASE with no spaces"),
			    tr("Open XMPP URI. (e.g. xmpp:someone@example.org?chat) "
			       "For security reasons, this must be the last option."));

		defineParam("status", tr("STATUS", "translate in UPPER_CASE with no spaces"),
			    tr("Set status. STATUS must be one of `online', `chat', `away', `xa', `dnd', `offline'.",
					"do not translate `online', `chat', etc; STATUS is the same as in previous string"));

		defineParam("status-message", tr("MSG", "translate in UPPER_CASE with no spaces"),
			    tr("Set status message. Must be used together with --status.",
					"do not translate --status"));

		defineSwitch("help", tr("Show this help message and exit."));
		defineAlias("h", "help");
		defineAlias("?", "help");

		defineSwitch("version", tr("Show version information and exit."));
		defineAlias("v", "version");
	}

	void showHelp(int textWidth = 78) {
		QString u1 = tr("Usage:") + " " + QFileInfo(QApplication::applicationFilePath()).fileName();
		QString u2 = "%1 [--profile=%2] [--remote|--choose-profile] [--status=%3\t[--status-message=%4]] [--uri=%5]";
		QString output = wrap(u2.arg(u1, tr("PROFILE"), tr("STATUS"), tr("MSG"), tr("URI")),
				      textWidth, u1.length() + 1, 0).replace('\t', ' '); // non-breakable space ;)

		output += '\n';
		output += tr("Psi - The Cross-Platform Jabber/XMPP Client For Power Users");
		output += "\n\n";
		output += tr("Options:");
		output += '\n';
		output += optionsHelp(textWidth);
		output += '\n';
		output += tr("Go to <http://psi-im.org/> for more information about Psi.");
		show(output);
	}

	void showVersion() {
		show(QString("%1 %2\nQt %3\n")
			.arg(ApplicationInfo::name()).arg(ApplicationInfo::version())
			.arg(qVersion())
			+QString(tr("Compiled with Qt %1", "%1 will contain Qt version number"))
			.arg(QT_VERSION_STR));
	}

	void show(const QString& text) {
#ifdef Q_WS_WIN
		QMessageBox::information(0, ApplicationInfo::name(), text);
#else
		puts(qPrintable(text));
#endif
	}

	virtual ~PsiCli() {}
};

#endif
