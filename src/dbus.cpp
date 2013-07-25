
#include <QString>
#include <QVector>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusAbstractAdaptor>

#include "common.h"
#include "dbus.h"

#include "psicontactlist.h"
#include "psiaccount.h"
#include "activeprofiles.h"

#define PSIDBUSIFACE "org.psi_im.Psi"



class PsiConAdapter : public QDBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.psi_im.Psi.Main")
//	Q_CLASSINFO("D-Bus Introspection", ...)

public:
	PsiConAdapter(PsiCon *psicon_);
	~PsiConAdapter();
public Q_SLOTS:
	void openURI(QString uri);
	void setStatus(QString status, QString message);
	void raise();
	void sleep();
	void wake();
/*Q_SIGNALS:
	void psi_pong();
*/
private:
	PsiCon *psicon;
};



PsiConAdapter::PsiConAdapter(PsiCon *psicon_) : QDBusAbstractAdaptor(psicon_)
{
	psicon = psicon_;
}

PsiConAdapter::~PsiConAdapter()
{}

void PsiConAdapter::openURI(QString uri)
{
	emit ActiveProfiles::instance()->openUriRequested(uri);
}

void PsiConAdapter::setStatus(QString status, QString message)
{
	emit ActiveProfiles::instance()->setStatusRequested(status, message);
}

// FIXME libguniqueapp uses activate
void PsiConAdapter::raise()
{
	emit ActiveProfiles::instance()->raiseRequested();
}

void PsiConAdapter::sleep()
{
	psicon->doSleep();
}

void PsiConAdapter::wake()
{
	psicon->doWakeup();
}


void addPsiConAdapter(PsiCon *psicon)
{
	new PsiConAdapter(psicon);
	QDBusConnection::sessionBus().registerObject("/Main", psicon);
}


#include "dbus.moc"
