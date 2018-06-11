#ifndef ENCRYPTIONSUPPORT_H
#define ENCRYPTIONSUPPORT_H

class QDomElement;

class EncryptionSupport
{
public:
  virtual ~EncryptionSupport() = default;

  // true = handled, don't pass to next handler

  virtual bool decryptMessageElement(int account, QDomElement &message) = 0;
  virtual bool encryptMessageElement(int account, QDomElement &message) = 0;
};

Q_DECLARE_INTERFACE(EncryptionSupport, "org.psi-im.EncryptionSupport/0.1");

#endif
