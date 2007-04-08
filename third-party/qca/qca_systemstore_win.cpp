/*
 * Copyright (C) 2004,2005  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// need to define this immediately
#define _WIN32_WINNT 0x400

#include "qca_systemstore.h"

#include <windows.h>
#include <wincrypt.h>

namespace QCA {

bool qca_have_systemstore()
{
	bool ok = false;
	HCERTSTORE hSystemStore;
	hSystemStore = CertOpenSystemStoreA(0, "ROOT");
	if(hSystemStore)
		ok = true;
	CertCloseStore(hSystemStore, 0);
	return ok;
}

CertificateCollection qca_get_systemstore(const QString &provider)
{
	CertificateCollection col;
	HCERTSTORE hSystemStore;
	hSystemStore = CertOpenSystemStoreA(0, "ROOT");
	if(!hSystemStore)
		return col;
	PCCERT_CONTEXT pc = NULL;
	while(1)
	{
		pc = CertFindCertificateInStore(
			hSystemStore,
			X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
			0,
			CERT_FIND_ANY,
			NULL,
			pc);
		if(!pc)
			break;
		int size = pc->cbCertEncoded;
		QByteArray der(size, 0);
		memcpy(der.data(), pc->pbCertEncoded, size);

		Certificate cert = Certificate::fromDER(der, 0, provider);
		if(!cert.isNull())
			col.addCertificate(cert);
	}
	CertCloseStore(hSystemStore, 0);
	return col;
}

}
