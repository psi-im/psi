#include "xmpp/base64/base64.h"

namespace XMPP {

QString Base64::encode(const QByteArray &s)
{
	int i;
	int len = s.size();
	char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
	int a, b, c;

	QByteArray p;
	p.resize((len+2)/3*4);
	int at = 0;
	for( i = 0; i < len; i += 3 ) {
		a = ((unsigned char)s[i] & 3) << 4;
		if(i + 1 < len) {
			a += (unsigned char)s[i + 1] >> 4;
			b = ((unsigned char)s[i + 1] & 0xF) << 2;
			if(i + 2 < len) {
				b += (unsigned char)s[i + 2] >> 6;
				c = (unsigned char)s[i + 2] & 0x3F;
			}
			else
				c = 64;
		}
		else {
			b = c = 64;
		}

		p[at++] = tbl[(unsigned char)s[i] >> 2];
		p[at++] = tbl[a];
		p[at++] = tbl[b];
		p[at++] = tbl[c];
	}
	return QString::fromAscii(p);
}

QByteArray Base64::decode(const QString& input)
{
	QByteArray s(QString(input).remove('\n').toUtf8());
	QByteArray p;

	// -1 specifies invalid
	// 64 specifies eof
	// everything else specifies data

	char tbl[] = {
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
		52,53,54,55,56,57,58,59,60,61,-1,-1,-1,64,-1,-1,
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
		15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
		-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
		41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	};

	// this should be a multiple of 4
	int len = s.size();

	if(len % 4) {
		return p;
	}

	p.resize(len / 4 * 3);

	int i;
	int at = 0;

	int a, b, c, d;
	c = d = 0;

	for( i = 0; i < len; i += 4 ) {
		a = tbl[(int)s[i]];
		b = tbl[(int)s[i + 1]];
		c = tbl[(int)s[i + 2]];
		d = tbl[(int)s[i + 3]];
		if((a == 64 || b == 64) || (a < 0 || b < 0 || c < 0 || d < 0)) {
			p.resize(0);
			return p;
		}
		p[at++] = ((a & 0x3F) << 2) | ((b >> 4) & 0x03);
		p[at++] = ((b & 0x0F) << 4) | ((c >> 2) & 0x0F);
		p[at++] = ((c & 0x03) << 6) | ((d >> 0) & 0x3F);
	}

	if(c & 64)
		p.resize(at - 2);
	else if(d & 64)
		p.resize(at - 1);

	return p;
}

}
