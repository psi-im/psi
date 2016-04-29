#ifndef SOUNDACCESSOR_H
#define SOUNDACCESSOR_H

class SoundAccessingHost;

class SoundAccessor
{
public:
	virtual ~SoundAccessor() {}

	virtual void setSoundAccessingHost(SoundAccessingHost* host) = 0;
};

Q_DECLARE_INTERFACE(SoundAccessor, "org.psi-im.SoundAccessor/0.1");

#endif
