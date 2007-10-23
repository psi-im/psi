#ifndef DISCOINFOQUERIER_H
#define DISCOINFOQUERIER_H

#include <QObject>

namespace XMPP {
	class Jid;
	class DiscoItem;
};

namespace Protocol {

/**
 * A DiscoInfoQuerier is an object used to query Service Discovery information.
 */
class DiscoInfoQuerier : public QObject
{
	Q_OBJECT

public:
	/**
	 * Retrieves Disco information of a jid on a specific node.
	 */
	virtual void getDiscoInfo(const XMPP::Jid& jid, const QString& node) = 0;

signals:
	/**
	 * Signals that a disco information request was succesful.
	 * 
	 * @param jid the jid on which the request was done
	 * @param node the node on which the request was done
	 * @param item the resulting disco item.
	 */
	void getDiscoInfo_success(const XMPP::Jid& jid, const QString& node, const XMPP::DiscoItem& item);

	/**
	 * Signals that a disco information request returned an error.
	 * 
	 * @param jid the jid on which the request was done
	 * @param node the node on which the request was done
	 * @param error_code the error code of the error
	 * @param error_string the error text of the error
	 */
	void getDiscoInfo_error(const XMPP::Jid& jid, const QString& node, int error_code, const QString& error_string);
};
};

#endif
