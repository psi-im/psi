
#include <QString>
#include <QVector>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusAbstractAdaptor>

#include "common.h"
#include "dbus.h"

#include "psicontactlist.h"
#include "psiaccount.h"

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
//	void openURI(QString uri);
	void raise();
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

/*void PsiConAdapter::openURI(QString uri)
{
	psicon->doOpenUri(uri);
}*/

// FIXME libguniqueapp uses activate
void PsiConAdapter::raise()
{
	psicon->raiseMainwin();
}




void addPsiConAdapter(PsiCon *psicon)
{
	new PsiConAdapter(psicon);
	QDBusConnection::sessionBus().registerObject("/Main", psicon);
}


#include "dbus.moc"
