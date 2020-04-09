#ifndef PSIHTTPAUTHREQUEST_H
#define PSIHTTPAUTHREQUEST_H

#include "xmpp_httpauthrequest.h"
#include "xmpp_stanza.h"

class PsiHttpAuthRequest : public XMPP::HttpAuthRequest {
public:
    PsiHttpAuthRequest() {}
    PsiHttpAuthRequest(const XMPP::HttpAuthRequest &request, const XMPP::Stanza &stanza) :
        HttpAuthRequest(request), s(stanza)
    {
    }

    const XMPP::Stanza &stanza() const { return s; }

private:
    XMPP::Stanza s;
};

#endif // PSIHTTPAUTHREQUEST_H
