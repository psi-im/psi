#ifndef IRISPROTOCOL_DISCOINFOQUERIER_H
#define IRISPROTOCOL_DISCOINFOQUERIER_H

#include <QObject>
#include <QPointer>

#include "xmpp_client.h"
#include "protocol/discoinfoquerier.h"

namespace XMPP {
	class Jid;
}

namespace IrisProtocol {

class DiscoInfoQuerier : public Protocol::DiscoInfoQuerier
{
	Q_OBJECT
public:
	DiscoInfoQuerier(XMPP::Client* client);

	void getDiscoInfo(const XMPP::Jid& jid, const QString& node);

private slots:
	void discoFinished();

private:
	QPointer<XMPP::Client> client_;
};

} // namespace

#endif

