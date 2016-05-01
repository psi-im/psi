#ifndef SOUNDACCESSINGHOST_H
#define SOUNDACCESSINGHOST_H

class QString;

class SoundAccessingHost
{
public:
	virtual ~SoundAccessingHost() {}

	virtual void playSound(const QString& fileName) = 0;
};

Q_DECLARE_INTERFACE(SoundAccessingHost, "org.psi-im.SoundAccessingHost/0.1");

#endif
