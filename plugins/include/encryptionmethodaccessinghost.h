/*
 * encryptionmethodaccessinghost.h - host API for plugin encryption methods
 * Copyright (C) 2026 Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef ENCRYPTIONMETHODACCESSINGHOST_H
#define ENCRYPTIONMETHODACCESSINGHOST_H

#include <QtPlugin>

class EncryptionMethodProvider;

class EncryptionMethodAccessingHost {
public:
    virtual ~EncryptionMethodAccessingHost() = default;

    // The provider remains owned by the plugin. Register it from enable(),
    // unregister it before destroying the provider, and do not use the host
    // after disable() has returned.
    virtual bool registerEncryptionMethod(EncryptionMethodProvider *method)     = 0;
    virtual void unregisterEncryptionMethod(EncryptionMethodProvider *method)   = 0;
    virtual void encryptionMethodStateChanged(EncryptionMethodProvider *method) = 0;
};

Q_DECLARE_INTERFACE(EncryptionMethodAccessingHost, "org.psi-im.EncryptionMethodAccessingHost/0.1")

#endif // ENCRYPTIONMETHODACCESSINGHOST_H
