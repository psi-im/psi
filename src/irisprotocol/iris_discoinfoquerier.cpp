#include "irisprotocol/iris_discoinfoquerier.h"
#include "xmpp_discoinfotask.h"

using namespace XMPP;

namespace IrisProtocol {

DiscoInfoQuerier::DiscoInfoQuerier(XMPP::Client* client) : client_(client)
{
}

void DiscoInfoQuerier::getDiscoInfo(const XMPP::Jid& jid, const QString& node)
{
	JT_DiscoInfo* disco = new JT_DiscoInfo(client_->rootTask());
	connect(disco, SIGNAL(finished()), SLOT(discoFinished()));
	disco->get(jid, node);
	disco->go(true);
}

void DiscoInfoQuerier::discoFinished()
{
	JT_DiscoInfo *disco = (JT_DiscoInfo*)sender();
	Q_ASSERT(disco);
	if (disco->success()) {
		emit getDiscoInfo_success(disco->jid(), disco->node(), disco->item());
	}
	else {
		emit getDiscoInfo_error(disco->jid(), disco->node(), disco->statusCode(), disco->statusString());
	}
}

} // namespace

