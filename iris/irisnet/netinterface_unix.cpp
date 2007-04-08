/*
 * Copyright (C) 2006  Justin Karneges
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

// this code assumes the following ioctls work:
//   SIOCGIFCONF  - get list of devices
//   SIOCGIFFLAGS - get flags about a device

// gateway detection currently only works on linux

#include "irisnetplugin.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <errno.h>

// for solaris
#ifndef SIOCGIFCONF
# include<sys/sockio.h>
#endif

class UnixIface
{
public:
	QString name;
	bool loopback;
	QHostAddress address;
};

class UnixGateway
{
public:
	QString ifaceName;
	QHostAddress address;
};

static QList<UnixIface> get_sioc_ifaces()
{
	QList<UnixIface> out;

	int tmpsock = socket(AF_INET, SOCK_DGRAM, 0);
	if(tmpsock < 0)
		return out;

	struct ifconf ifc;
	int lastlen = 0;
	QByteArray buf(100 * sizeof(struct ifreq), 0); // guess
	while(1)
	{
		ifc.ifc_len = buf.size();
		ifc.ifc_buf = buf.data();
		if(ioctl(tmpsock, SIOCGIFCONF, &ifc) < 0)
		{
			if(errno != EINVAL || lastlen != 0)
				return out;
		}
		else
		{
			// if it didn't grow since last time, then
			//   there's no overflow
			if(ifc.ifc_len == lastlen)
				break;
			lastlen = ifc.ifc_len;
		}
		buf.resize(buf.size() + 10 * sizeof(struct ifreq));
	}
	buf.resize(lastlen);

	int itemsize;
	for(int at = 0; at < buf.size(); at += itemsize)
	{
		struct ifreq *ifr = (struct ifreq *)(buf.data() + at);

		int sockaddr_len;
		if(((struct sockaddr *)&ifr->ifr_addr)->sa_family == AF_INET)
			sockaddr_len = sizeof(struct sockaddr_in);
		else if(((struct sockaddr *)&ifr->ifr_addr)->sa_family == AF_INET6)
			sockaddr_len = sizeof(struct sockaddr_in6);
		else
			sockaddr_len = sizeof(struct sockaddr);

		// set this asap so the next iteration is possible
		itemsize = sizeof(ifr->ifr_name) + sockaddr_len;

		// skip if the family is 0 (sometimes you get empty entries)
		if(ifr->ifr_addr.sa_family == 0)
			continue;

		// make a copy of this item to do additional ioctls on
		struct ifreq ifrcopy = *ifr;

		// grab the flags
		if(ioctl(tmpsock, SIOCGIFFLAGS, &ifrcopy) < 0)
			continue;

		// device must be up and not loopback
		if(!(ifrcopy.ifr_flags & IFF_UP))
			continue;

		UnixIface i;
		i.name = QString::fromLatin1(ifr->ifr_name);
		i.loopback = (ifrcopy.ifr_flags & IFF_LOOPBACK) ? true : false;
		i.address.setAddress(&ifr->ifr_addr);
		out += i;
	}

	// don't need this anymore
	close(tmpsock);

	return out;
}

static QStringList read_proc_as_lines(const char *procfile)
{
	QStringList out;

	FILE *f = fopen(procfile, "r");
	if(!f)
		return out;

	QByteArray buf;
	while(!feof(f))
	{
		// max read on a proc is 4K
		QByteArray block(4096, 0);
		int ret = fread(block.data(), 1, block.size(), f);
		if(ret <= 0)
			break;
		block.resize(ret);
		buf += block;
	}
	fclose(f);

	QString str = QString::fromLocal8Bit(buf);
	out = str.split('\n', QString::SkipEmptyParts);
	return out;
}

static QHostAddress linux_ipv6_to_qaddr(const QString &in)
{
	QHostAddress out;
	if(in.length() != 32)
		return out;
	quint8 raw[16];
	for(int n = 0; n < 16; ++n)
	{
		bool ok;
		int x = in.mid(n * 2, 2).toInt(&ok, 16);
		if(!ok)
			return out;
		raw[n] = (quint8)x;
	}
	out.setAddress(raw);
	return out;
}

static QHostAddress linux_ipv4_to_qaddr(const QString &in)
{
	QHostAddress out;
	if(in.length() != 8)
		return out;
	quint32 raw;
	unsigned char *rawp = (unsigned char *)&raw;
	for(int n = 0; n < 4; ++n)
	{
		bool ok;
		int x = in.mid(n * 2, 2).toInt(&ok, 16);
		if(!ok)
			return out;
		rawp[n] = (unsigned char )x;
	}
	out.setAddress(raw);
	return out;
}

static QList<UnixIface> get_linux_ipv6_ifaces()
{
	QList<UnixIface> out;

	QStringList lines = read_proc_as_lines("/proc/net/if_inet6");
	for(int n = 0; n < lines.count(); ++n)
	{
		const QString &line = lines[n];
		QStringList parts = line.simplified().split(' ', QString::SkipEmptyParts);
		if(parts.count() < 6)
			continue;

		QString name = parts[5];
		if(name.isEmpty())
			continue;
		QHostAddress addr = linux_ipv6_to_qaddr(parts[0]);
		if(addr.isNull())
			continue;

		QString scopestr = parts[3];
		bool ok;
		unsigned int scope = parts[3].toInt(&ok, 16);
		if(!ok)
			continue;

		// IPV6_ADDR_LOOPBACK    0x0010U
		// IPV6_ADDR_SCOPE_MASK  0x00f0U
		bool loopback = false;
		if((scope & 0x00f0U) == 0x0010U)
			loopback = true;

		UnixIface i;
		i.name = name;
		i.loopback = loopback;
		i.address = addr;
		out += i;
	}

	return out;
}

static QList<UnixGateway> get_linux_gateways()
{
	QList<UnixGateway> out;

	QStringList lines = read_proc_as_lines("/proc/net/route");
	// skip the first line, so we start at 1
	for(int n = 1; n < lines.count(); ++n)
	{
		const QString &line = lines[n];
		QStringList parts = line.simplified().split(' ', QString::SkipEmptyParts);
		if(parts.count() < 10) // net-tools does 10, but why not 11?
			continue;

		QHostAddress addr = linux_ipv4_to_qaddr(parts[2]);
		if(addr.isNull())
			continue;

		int iflags = parts[3].toInt(0, 16);
		if(!(iflags & RTF_UP))
			continue;

		if(!(iflags & RTF_GATEWAY))
			continue;

		UnixGateway g;
		g.ifaceName = parts[0];
		g.address = addr;
		out += g;
	}

	lines = read_proc_as_lines("/proc/net/ipv6_route");
	for(int n = 0; n < lines.count(); ++n)
	{
		const QString &line = lines[n];
		QStringList parts = line.simplified().split(' ', QString::SkipEmptyParts);
		if(parts.count() < 10)
			continue;

		QHostAddress addr = linux_ipv6_to_qaddr(parts[4]);
		if(addr.isNull())
			continue;

		int iflags = parts[8].toInt(0, 16);
		if(!(iflags & RTF_UP))
			continue;

		if(!(iflags & RTF_GATEWAY))
			continue;

		UnixGateway g;
		g.ifaceName = parts[9];
		g.address = addr;
		out += g;
	}

	return out;
}

static QList<UnixIface> get_unix_ifaces()
{
	QList<UnixIface> out = get_sioc_ifaces();
#ifdef Q_OS_LINUX
	out += get_linux_ipv6_ifaces();
#endif
	return out;
}

static QList<UnixGateway> get_unix_gateways()
{
	// support other platforms here
	QList<UnixGateway> out;
#ifdef Q_OS_LINUX
	out = get_linux_gateways();
#endif
	return out;
}

namespace XMPP {

class UnixNet : public NetInterfaceProvider
{
	Q_OBJECT
	Q_INTERFACES(XMPP::NetInterfaceProvider);
public:
	QList<Info> info;
	QTimer t;

	UnixNet() : t(this)
	{
		connect(&t, SIGNAL(timeout()), SLOT(check()));
	}

	void start()
	{
		t.start(5000);
		poll();
	}

	QList<Info> interfaces() const
	{
		return info;
	}

	void poll()
	{
		QList<Info> ifaces;

		QList<UnixIface> list = get_unix_ifaces();
		for(int n = 0; n < list.count(); ++n)
		{
			// see if we have it already
			int lookup = -1;
			for(int k = 0; k < ifaces.count(); ++k)
			{
				if(ifaces[k].id == list[n].name)
				{
					lookup = k;
					break;
				}
			}

			// don't have it?  make it
			if(lookup == -1)
			{
				Info i;
				i.id = list[n].name;
				i.name = list[n].name;
				i.isLoopback = list[n].loopback;
				i.addresses += list[n].address;
				ifaces += i;
			}
			// otherwise, tack on the address
			else
				ifaces[lookup].addresses += list[n].address;
		}

		QList<UnixGateway> glist = get_unix_gateways();
		for(int n = 0; n < glist.count(); ++n)
		{
			// look up the interface
			int lookup = -1;
			for(int k = 0; k < ifaces.count(); ++k)
			{
				if(ifaces[k].id == glist[n].ifaceName)
				{
					lookup = k;
					break;
				}
			}

			if(lookup == -1)
				break;

			ifaces[lookup].gateway = glist[n].address;
		}

		info = ifaces;
	}

signals:
	void updated();

public slots:
	void check()
	{
		poll();
		emit updated();
	}
};

class UnixNetProvider : public IrisNetProvider
{
	Q_OBJECT
	Q_INTERFACES(XMPP::IrisNetProvider);
public:
	virtual NetInterfaceProvider *createNetInterfaceProvider()
	{
		return new UnixNet;
	}
};

IrisNetProvider *irisnet_createUnixNetProvider()
{
	return new UnixNetProvider;
}

}

#include "netinterface_unix.moc"
