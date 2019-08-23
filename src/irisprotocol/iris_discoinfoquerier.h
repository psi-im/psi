#ifndef IRIS_DISCOINFOQUERIER_H
#define IRIS_DISCOINFOQUERIER_H

#include "protocol/discoinfoquerier.h"
#include "xmpp_client.h"

#include <QObject>
#include <QPointer>

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
}; // namespace IrisProtocol

#endif // IRIS_DISCOINFOQUERIER_H
