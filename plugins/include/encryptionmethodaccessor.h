/*
 * encryptionmethodaccessor.h - accessor for plugin encryption method host
 * Copyright (C) 2026 Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef ENCRYPTIONMETHODACCESSOR_H
#define ENCRYPTIONMETHODACCESSOR_H

#include <QtPlugin>

class EncryptionMethodAccessingHost;

class EncryptionMethodAccessor {
public:
    virtual ~EncryptionMethodAccessor()                                                = default;
    virtual void setEncryptionMethodAccessingHost(EncryptionMethodAccessingHost *host) = 0;
};

Q_DECLARE_INTERFACE(EncryptionMethodAccessor, "org.psi-im.EncryptionMethodAccessor/0.1")

#endif // ENCRYPTIONMETHODACCESSOR_H
