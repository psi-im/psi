/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CRICKET_XMPP_XMPPLIB_BUZZ_CONSTANTS_H_
#define _CRICKET_XMPP_XMPPLIB_BUZZ_CONSTANTS_H_

#include <string>
#include "talk/xmllite/qname.h"
#include "talk/xmpp/jid.h"


#define NS_CLIENT Constants::ns_client()
#define NS_SERVER Constants::ns_server()
#define NS_STREAM Constants::ns_stream()
#define NS_XSTREAM Constants::ns_xstream()
#define NS_TLS Constants::ns_tls()
#define NS_SASL Constants::ns_sasl()
#define NS_BIND Constants::ns_bind()
#define NS_DIALBACK Constants::ns_dialback()
#define NS_SESSION Constants::ns_session()
#define NS_STANZA Constants::ns_stanza()
#define NS_PRIVACY Constants::ns_privacy()
#define NS_ROSTER Constants::ns_roster()
#define NS_VCARD Constants::ns_vcard()
#define NS_VCARD_UPDATE Constants::ns_vcard_update()
#define STR_CLIENT Constants::str_client()
#define STR_SERVER Constants::str_server()
#define STR_STREAM Constants::str_stream()


namespace buzz {

extern const Jid JID_EMPTY;

class Constants {
 public:
  static const std::string & ns_client();
  static const std::string & ns_server();
  static const std::string & ns_stream();
  static const std::string & ns_xstream();
  static const std::string & ns_tls();
  static const std::string & ns_sasl();
  static const std::string & ns_bind();
  static const std::string & ns_dialback();
  static const std::string & ns_session();
  static const std::string & ns_stanza();
  static const std::string & ns_privacy();
  static const std::string & ns_roster();
  static const std::string & ns_vcard();
  static const std::string & ns_vcard_update();

  static const std::string & str_client();
  static const std::string & str_server();
  static const std::string & str_stream();
};

extern const std::string STR_GET;
extern const std::string STR_SET;
extern const std::string STR_RESULT;
extern const std::string STR_ERROR;


extern const std::string STR_FROM;
extern const std::string STR_TO;
extern const std::string STR_BOTH;
extern const std::string STR_REMOVE;

extern const std::string STR_MESSAGE;
extern const std::string STR_BODY;
extern const std::string STR_PRESENCE;
extern const std::string STR_STATUS;
extern const std::string STR_SHOW;
extern const std::string STR_PRIOIRTY;
extern const std::string STR_IQ;

extern const std::string STR_TYPE;
extern const std::string STR_NAME;
extern const std::string STR_ID;
extern const std::string STR_JID;
extern const std::string STR_SUBSCRIPTION;
extern const std::string STR_ASK;
extern const std::string STR_X;
extern const std::string STR_GOOGLE_COM;
extern const std::string STR_GMAIL_COM;
extern const std::string STR_GOOGLEMAIL_COM;
extern const std::string STR_DEFAULT_DOMAIN;

extern const std::string STR_UNAVAILABLE;
extern const std::string STR_INVISIBLE;

extern const QName QN_STREAM_STREAM;
extern const QName QN_STREAM_FEATURES;
extern const QName QN_STREAM_ERROR;

extern const QName QN_XSTREAM_BAD_FORMAT;
extern const QName QN_XSTREAM_BAD_NAMESPACE_PREFIX;
extern const QName QN_XSTREAM_CONFLICT;
extern const QName QN_XSTREAM_CONNECTION_TIMEOUT;
extern const QName QN_XSTREAM_HOST_GONE;
extern const QName QN_XSTREAM_HOST_UNKNOWN;
extern const QName QN_XSTREAM_IMPROPER_ADDRESSIING;
extern const QName QN_XSTREAM_INTERNAL_SERVER_ERROR;
extern const QName QN_XSTREAM_INVALID_FROM;
extern const QName QN_XSTREAM_INVALID_ID;
extern const QName QN_XSTREAM_INVALID_NAMESPACE;
extern const QName QN_XSTREAM_INVALID_XML;
extern const QName QN_XSTREAM_NOT_AUTHORIZED;
extern const QName QN_XSTREAM_POLICY_VIOLATION;
extern const QName QN_XSTREAM_REMOTE_CONNECTION_FAILED;
extern const QName QN_XSTREAM_RESOURCE_CONSTRAINT;
extern const QName QN_XSTREAM_RESTRICTED_XML;
extern const QName QN_XSTREAM_SEE_OTHER_HOST;
extern const QName QN_XSTREAM_SYSTEM_SHUTDOWN;
extern const QName QN_XSTREAM_UNDEFINED_CONDITION;
extern const QName QN_XSTREAM_UNSUPPORTED_ENCODING;
extern const QName QN_XSTREAM_UNSUPPORTED_STANZA_TYPE;
extern const QName QN_XSTREAM_UNSUPPORTED_VERSION;
extern const QName QN_XSTREAM_XML_NOT_WELL_FORMED;
extern const QName QN_XSTREAM_TEXT;

extern const QName QN_TLS_STARTTLS;
extern const QName QN_TLS_REQUIRED;
extern const QName QN_TLS_PROCEED;
extern const QName QN_TLS_FAILURE;

extern const QName QN_SASL_MECHANISMS;
extern const QName QN_SASL_MECHANISM;
extern const QName QN_SASL_AUTH;
extern const QName QN_SASL_CHALLENGE;
extern const QName QN_SASL_RESPONSE;
extern const QName QN_SASL_ABORT;
extern const QName QN_SASL_SUCCESS;
extern const QName QN_SASL_FAILURE;
extern const QName QN_SASL_ABORTED;
extern const QName QN_SASL_INCORRECT_ENCODING;
extern const QName QN_SASL_INVALID_AUTHZID;
extern const QName QN_SASL_INVALID_MECHANISM;
extern const QName QN_SASL_MECHANISM_TOO_WEAK;
extern const QName QN_SASL_NOT_AUTHORIZED;
extern const QName QN_SASL_TEMPORARY_AUTH_FAILURE;

extern const QName QN_DIALBACK_RESULT;
extern const QName QN_DIALBACK_VERIFY;

extern const QName QN_STANZA_BAD_REQUEST;
extern const QName QN_STANZA_CONFLICT;
extern const QName QN_STANZA_FEATURE_NOT_IMPLEMENTED;
extern const QName QN_STANZA_FORBIDDEN;
extern const QName QN_STANZA_GONE;
extern const QName QN_STANZA_INTERNAL_SERVER_ERROR;
extern const QName QN_STANZA_ITEM_NOT_FOUND;
extern const QName QN_STANZA_JID_MALFORMED;
extern const QName QN_STANZA_NOT_ACCEPTABLE;
extern const QName QN_STANZA_NOT_ALLOWED;
extern const QName QN_STANZA_PAYMENT_REQUIRED;
extern const QName QN_STANZA_RECIPIENT_UNAVAILABLE;
extern const QName QN_STANZA_REDIRECT;
extern const QName QN_STANZA_REGISTRATION_REQUIRED;
extern const QName QN_STANZA_REMOTE_SERVER_NOT_FOUND;
extern const QName QN_STANZA_REMOTE_SERVER_TIMEOUT;
extern const QName QN_STANZA_RESOURCE_CONSTRAINT;
extern const QName QN_STANZA_SERVICE_UNAVAILABLE;
extern const QName QN_STANZA_SUBSCRIPTION_REQUIRED;
extern const QName QN_STANZA_UNDEFINED_CONDITION;
extern const QName QN_STANZA_UNEXPECTED_REQUEST;
extern const QName QN_STANZA_TEXT;

extern const QName QN_BIND_BIND;
extern const QName QN_BIND_RESOURCE;
extern const QName QN_BIND_JID;

extern const QName QN_MESSAGE;
extern const QName QN_BODY;
extern const QName QN_SUBJECT;
extern const QName QN_THREAD;
extern const QName QN_PRESENCE;
extern const QName QN_SHOW;
extern const QName QN_STATUS;
extern const QName QN_LANG;
extern const QName QN_PRIORITY;
extern const QName QN_IQ;
extern const QName QN_ERROR;

extern const QName QN_SERVER_MESSAGE;
extern const QName QN_SERVER_BODY;
extern const QName QN_SERVER_SUBJECT;
extern const QName QN_SERVER_THREAD;
extern const QName QN_SERVER_PRESENCE;
extern const QName QN_SERVER_SHOW;
extern const QName QN_SERVER_STATUS;
extern const QName QN_SERVER_LANG;
extern const QName QN_SERVER_PRIORITY;
extern const QName QN_SERVER_IQ;
extern const QName QN_SERVER_ERROR;

extern const QName QN_SESSION_SESSION;

extern const QName QN_PRIVACY_QUERY;
extern const QName QN_PRIVACY_ACTIVE;
extern const QName QN_PRIVACY_DEFAULT;
extern const QName QN_PRIVACY_LIST;
extern const QName QN_PRIVACY_ITEM;
extern const QName QN_PRIVACY_IQ;
extern const QName QN_PRIVACY_MESSAGE;
extern const QName QN_PRIVACY_PRESENCE_IN;
extern const QName QN_PRIVACY_PRESENCE_OUT;

extern const QName QN_ROSTER_QUERY;
extern const QName QN_ROSTER_ITEM;
extern const QName QN_ROSTER_GROUP;

extern const QName QN_VCARD_QUERY;
extern const QName QN_VCARD_FN;
extern const QName QN_VCARD_PHOTO;
extern const QName QN_VCARD_PHOTO_BINVAL;

extern const QName QN_XML_LANG;

extern const QName QN_ENCODING;
extern const QName QN_VERSION;
extern const QName QN_TO;
extern const QName QN_FROM;
extern const QName QN_TYPE;
extern const QName QN_ID;
extern const QName QN_CODE;
extern const QName QN_NAME;
extern const QName QN_VALUE;
extern const QName QN_ACTION;
extern const QName QN_ORDER;
extern const QName QN_MECHANISM;
extern const QName QN_ASK;
extern const QName QN_JID;
extern const QName QN_SUBSCRIPTION;


extern const QName QN_XMLNS_CLIENT;
extern const QName QN_XMLNS_SERVER;
extern const QName QN_XMLNS_STREAM;

// Presence
extern const std::string STR_SHOW_AWAY;
extern const std::string STR_SHOW_CHAT;
extern const std::string STR_SHOW_DND;
extern const std::string STR_SHOW_XA;
extern const std::string STR_SHOW_OFFLINE;

// Subscription
extern const std::string STR_SUBSCRIBE;
extern const std::string STR_SUBSCRIBED;
extern const std::string STR_UNSUBSCRIBE;
extern const std::string STR_UNSUBSCRIBED;


// JEP 0030
extern const QName QN_NODE;
extern const QName QN_CATEGORY;
extern const QName QN_VAR;
extern const std::string NS_DISCO_INFO;
extern const std::string NS_DISCO_ITEMS;

extern const QName QN_DISCO_INFO_QUERY;
extern const QName QN_DISCO_IDENTITY;
extern const QName QN_DISCO_FEATURE;

extern const QName QN_DISCO_ITEMS_QUERY;
extern const QName QN_DISCO_ITEM;


// JEP 0115
extern const std::string NS_CAPS;
extern const QName QN_CAPS_C;
extern const QName QN_VER;
extern const QName QN_EXT;


// Avatar - JEP 0153
extern const std::string kNSVCard;
extern const QName kQnVCardX;
extern const QName kQnVCardPhoto;


// JEP 0091 Delayed Delivery
extern const std::string kNSDelay;
extern const QName kQnDelayX;
extern const QName kQnStamp;

}

#endif // _CRICKET_XMPP_XMPPLIB_BUZZ_CONSTANTS_H_
