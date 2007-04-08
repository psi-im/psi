#ifndef PORTAUDIOMEDIAENGINE_H
#define PORTAUDIOMEDIAENGINE_H

#include <portaudio.h>
#include <speex.h>
#include <ortp/ortp.h>

#include "talk/session/phone/mediaengine.h"

class PortAudioMediaChannel : public cricket::MediaChannel 
{
public:
	PortAudioMediaChannel();
	virtual ~PortAudioMediaChannel();
	virtual void SetCodec(const char *codec);
	virtual void OnPacketReceived(const void *data, int len);

	virtual void SetPlayout(bool playout);
	virtual void SetSend(bool send);

	virtual float GetCurrentQuality();
	virtual int GetOutputLevel();

	void readOutput(float*, int);
	void writeInput(float*, int);

protected:
	void readBuffer(float*, float**, float*, float*, float*, int);
	void writeBuffer(float*, float*, float**, float*, float*, int);

private:
	bool mute_;
	bool play_;
	PortAudioStream* stream_;

	// Buffers
	float *out_buffer_, *out_buffer_read_, *out_buffer_write_, *out_buffer_end_;
	float *in_buffer_, *in_buffer_read_, *in_buffer_write_, *in_buffer_end_;

	// Speex
	SpeexBits speex_bits_;
	void *speex_enc_state_, *speex_dec_state_;
	float *speex_frame_;
	int speex_frame_size_;

	// ORTP
	int rtp_socket_;
	RtpSession* rtp_session_;
	int rtp_timestamp_;
};


class PortAudioMediaEngine : public cricket::MediaEngine 
{
public:
	PortAudioMediaEngine();
	~PortAudioMediaEngine();
	virtual bool Init();
	virtual void Terminate();

	virtual cricket::MediaChannel *CreateChannel();

	virtual int SetAudioOptions(int options);
	virtual int SetSoundDevices(int wave_in_device, int wave_out_device);

	virtual int GetInputLevel();
};

#endif
