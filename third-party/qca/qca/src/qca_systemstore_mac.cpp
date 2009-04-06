/*
 * Copyright (C) 2004,2005  Justin Karneges <justin@affinix.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "qca_systemstore.h"

#include <Carbon/Carbon.h>
#include <Security/SecTrust.h>
#include <Security/SecCertificate.h>

namespace QCA {

bool qca_have_systemstore()
{
	return true;
}

CertificateCollection qca_get_systemstore(const QString &provider)
{
	CertificateCollection col;
	CFArrayRef anchors;
	if(SecTrustCopyAnchorCertificates(&anchors) != 0)
		return col;
	for(int n = 0; n < CFArrayGetCount(anchors); ++n)
	{
		SecCertificateRef cr = (SecCertificateRef)CFArrayGetValueAtIndex(anchors, n);
		CSSM_DATA cssm;
		SecCertificateGetData(cr, &cssm);
		QByteArray der(cssm.Length, 0);
		memcpy(der.data(), cssm.Data, cssm.Length);

		Certificate cert = Certificate::fromDER(der, 0, provider);
		if(!cert.isNull())
			col.addCertificate(cert);
	}
	CFRelease(anchors);
	return col;
}

}
