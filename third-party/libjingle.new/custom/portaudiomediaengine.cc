#include <portaudio.h>
#include <ortp/ortp.h>
#include <speex.h>

// Socket stuff
#ifndef _WIN32
#ifdef INET6
#include <netdb.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#else
#include <winsock32.h>
#endif

#include "talk/session/phone/mediaengine.h"
#include "talk/session/phone/portaudiomediaengine.h"

// Engine settings
#define ENGINE_BUFFER_SIZE 2048

// PortAudio settings
#define FRAMES_PER_BUFFER 256
#define SAMPLE_RATE 1

// Speex settings
//#define SPEEX_QUALITY 8

// ORTP settings
#define MAX_RTP_SIZE 1500 // From mediastreamer


// -----------------------------------------------------------------------------

static int portAudioCallback( void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, PaTimestamp outTime, void *channel_p )
{
	PortAudioMediaChannel* channel = (PortAudioMediaChannel*) channel_p;
	channel->readOutput((float*) outputBuffer, framesPerBuffer);
	channel->writeInput((float*) inputBuffer, framesPerBuffer);
	return 0;
}

// -----------------------------------------------------------------------------

PortAudioMediaChannel::PortAudioMediaChannel() : mute_(false), play_(false), stream_(NULL), out_buffer_(NULL), in_buffer_(NULL), speex_frame_(NULL)
{
	// Initialize buffers
	out_buffer_ = new float[ENGINE_BUFFER_SIZE];
	out_buffer_read_ = out_buffer_write_ = (float*) out_buffer_;
	out_buffer_end_ = (float*) out_buffer_ + ENGINE_BUFFER_SIZE;
	in_buffer_ = new float[ENGINE_BUFFER_SIZE];
	in_buffer_read_ = in_buffer_write_ = (float*) in_buffer_;
	in_buffer_end_ = (float*) in_buffer_ + ENGINE_BUFFER_SIZE;

	// Initialize PortAudio
	int err = Pa_OpenDefaultStream(&stream_, 1, 1, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, portAudioCallback, this );
	if (err != paNoError) 
		fprintf(stderr, "Error creating a PortAudio stream: %s\n", Pa_GetErrorText(err));

	// Initialize Speex
	speex_bits_init(&speex_bits_);
	speex_enc_state_ = speex_encoder_init(&speex_nb_mode);
	speex_dec_state_ = speex_decoder_init(&speex_nb_mode);
	speex_decoder_ctl(speex_dec_state_, SPEEX_GET_FRAME_SIZE, &speex_frame_size_);
	speex_frame_ = new float[speex_frame_size_];
	
	// int quality = SPEEX_QUALITY;
	// speex_encoder_ctl(state, SPEEX_SET_QUALITY, &quality);

	// Initialize ORTP socket
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	sockaddr.sin_port = htons(3000);
	rtp_socket_ = socket(PF_INET, SOCK_DGRAM, 0);
	fcntl(rtp_socket_, F_SETFL, 0, O_NONBLOCK);
	bind (rtp_socket_,(struct sockaddr*)&sockaddr, sizeof(sockaddr));

	// Initialize ORTP Session
	rtp_session_ = rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_max_buf_size_set(rtp_session_, MAX_RTP_SIZE);
	rtp_session_set_profile(rtp_session_, &av_profile);
	rtp_session_set_local_addr(rtp_session_, "127.0.0.1", 2000);
	rtp_session_set_remote_addr(rtp_session_, "127.0.0.1", 3000);
	rtp_session_set_scheduling_mode(rtp_session_, 0);
	rtp_session_set_blocking_mode(rtp_session_, 0);
	rtp_session_set_payload_type(rtp_session_, 110);
	rtp_session_set_jitter_compensation(rtp_session_, 250);
	rtp_session_enable_adaptive_jitter_compensation(rtp_session_, TRUE);
	rtp_timestamp_ = 0;
	//rtp_session_signal_connect(rtp_session_, "telephone-event", (RtpCallback) ortpTelephoneCallback,this);
}

PortAudioMediaChannel::~PortAudioMediaChannel()
{
	if (stream_) {
		Pa_CloseStream(stream_);
	}

	// Clean up other allocated pointers

	close(rtp_socket_);
}

void PortAudioMediaChannel::SetCodec(const char *codec)
{
	if (strcmp(codec, "speex"))
		printf("Unsupported codec: %s\n", codec);
}

void PortAudioMediaChannel::OnPacketReceived(const void *data, int len)
{
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	struct hostent *host = gethostbyname("localhost");
	memcpy(&sockaddr.sin_addr.s_addr, host->h_addr, host->h_length);
	sockaddr.sin_port = htons(2000);

	char buf[2048];
	memcpy(buf, data, len);

	// Pass packet on to ORTP
	if (play_) {
		sendto(rtp_socket_, buf, len, 0, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
	}
}

void PortAudioMediaChannel::SetPlayout(bool playout)
{
	if (!stream_) 
		return;

	if (play_ && !playout) {
		int err = Pa_StopStream(stream_);
		if (err != paNoError) {
			fprintf(stderr, "Error stopping PortAudio stream: %s\n", Pa_GetErrorText(err));
			return;
		}
		play_ = false;
	}
	else if (!play_ && playout) {
		int err = Pa_StartStream(stream_);
		if (err != paNoError) {
			fprintf(stderr, "Error starting PortAudio stream: %s\n", Pa_GetErrorText(err));
			return;
		}
		play_ = true;
	}
}

void PortAudioMediaChannel::SetSend(bool send)
{
	mute_ = !send;
}


float PortAudioMediaChannel::GetCurrentQuality()
{
	return 0;
}

int PortAudioMediaChannel::GetOutputLevel()
{
	return 0;
}

void PortAudioMediaChannel::readOutput(float* buf, int len)
{
	//readBuffer(out_buffer_, &out_buffer_read_, out_buffer_write_, out_buffer_end_, buf, len);

	// Receive a packet (if there is one)
	mblk_t *mp;
	mp = rtp_session_recvm_with_ts(rtp_session_,rtp_timestamp_);
	while (mp != NULL) {
		gint in_len = mp->b_cont->b_wptr-mp->b_cont->b_rptr;

		// Decode speex stream
		speex_bits_read_from(&speex_bits_,mp->b_cont->b_rptr, in_len);
		speex_decode(speex_dec_state_, &speex_bits_, speex_frame_);
		writeBuffer(out_buffer_, out_buffer_read_, &out_buffer_write_, out_buffer_end_, speex_frame_, speex_frame_size_);
		rtp_timestamp_++;
		mp = rtp_session_recvm_with_ts(rtp_session_,rtp_timestamp_);
	}

	// Read output
	readBuffer(out_buffer_, &out_buffer_read_, out_buffer_write_, out_buffer_end_, buf, len);
}

void PortAudioMediaChannel::writeInput(float* buf, int len)
{
	//writeBuffer(in_buffer_, in_buffer_read_, &in_buffer_write_, in_buffer_end_, buf, len);
}
	

void PortAudioMediaChannel::readBuffer(float* buffer, float** buffer_read_p, float*buffer_write, float* buffer_end, float* target_buffer, int target_len)
{
	float *end, *tmp, *buffer_read = *buffer_read_p;
	int remaining;
	
	// First phase
	tmp = buffer_read + target_len;
	if (buffer_write < buffer_read && tmp > buffer_end) {
		end = buffer_end;
		remaining = tmp - buffer_end;
	}
	else {
		end = (tmp > buffer_write ? buffer_write : tmp);
		remaining = 0;
	}

	while (buffer_read < end) {
		*target_buffer++ = *buffer_read++;
	}

	// Second phase
	if (remaining > 0) {
		buffer_read = buffer;
		tmp = buffer_read + remaining;
		end = (tmp > buffer_write ? buffer_write : tmp);
		while (buffer_read < end) {
			*target_buffer++ = *buffer_read++;
		}
	}

	// Finish up
	*buffer_read_p = buffer_read;
}

void PortAudioMediaChannel::writeBuffer(float* buffer, float* buffer_read, float**buffer_write_p, float* buffer_end, float* source_buffer, int source_len)
{
	float *end, *tmp, *buffer_write = *buffer_write_p;
	int remaining;

	// First phase
	tmp = buffer_write + source_len;
	if (buffer_write > buffer_read) {
		if (tmp > buffer_end) {
			end = buffer_end;
			remaining = tmp - buffer_end;
		}
		else {
			end = tmp;
			remaining = 0;
		}
	}
	else {
		if (tmp > buffer_read) {
			printf("Warning: Dropping frame(s)\n");
			end = buffer_read;
			remaining = 0;
		}
		else {
			end = tmp;
			remaining = 0;
		}
	}
	
	while (buffer_write < end) {
		*buffer_write++ = *source_buffer++;
	}

	// Second phase
	if (remaining > 0) {
		buffer_write = buffer;
		tmp = buffer_write + remaining;
		if (tmp > buffer_read) {
			printf("Warning: Dropping frame(s)\n");
			end = buffer_read;
		}
		else {
			end = tmp;
		}
		while (buffer_write < end) {
			*buffer_write++ = *source_buffer++;
		}
	}

	// Finish up
	*buffer_write_p = buffer_write;
}

// -----------------------------------------------------------------------------

PortAudioMediaEngine::PortAudioMediaEngine()
{
}

PortAudioMediaEngine::~PortAudioMediaEngine()
{
	Pa_Terminate();
}

bool PortAudioMediaEngine::Init()
{
	ortp_init();

	int err = Pa_Initialize();
	if (err != paNoError) {
		fprintf(stderr,"Error initializing PortAudio: %s\n",Pa_GetErrorText(err));
		return false;
	}

	// Speex
	rtp_profile_set_payload(&av_profile, 110, &speex_wb);
	codecs_.push_back(Codec(110, "speex", 8));
	
	return true;
}

void PortAudioMediaEngine::Terminate()
{
}


cricket::MediaChannel* PortAudioMediaEngine::CreateChannel()
{
	return new PortAudioMediaChannel();
}

int PortAudioMediaEngine::SetAudioOptions(int options)
{
}

int PortAudioMediaEngine::SetSoundDevices(int wave_in_device, int wave_out_device)
{
}

int PortAudioMediaEngine::GetInputLevel()
{
}
