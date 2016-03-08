#include "psicapsregsitry.h"
#include "iodeviceopener.h"
#include "applicationinfo.h"

PsiCapsRegistry::PsiCapsRegistry(QObject *parent) :
	CapsRegistry(parent)
{

}

void PsiCapsRegistry::saveData(const QByteArray &data)
{
	QFile file(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/caps.xml");
	IODeviceOpener opener(&file, QIODevice::WriteOnly);
	if (!opener.isOpen()) {
		qWarning("Caps: Unable to open IO device");
		return;
	}
	file.write(data);
}

QByteArray PsiCapsRegistry::loadData()
{
	QFile file(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/caps.xml");
	IODeviceOpener opener(&file, QIODevice::ReadOnly);
	if (!opener.isOpen()) {
		qWarning("CapsRegistry: Cannot open input device");
		return QByteArray();
	}
	return file.readAll();
}
