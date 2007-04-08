/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PSEUDOTCP_H__
#define __PSEUDOTCP_H__

#include <list>
#include "talk/base/basictypes.h"

namespace cricket {

//////////////////////////////////////////////////////////////////////
// IPseudoTcpNotify
//////////////////////////////////////////////////////////////////////

class PseudoTcp;

class IPseudoTcpNotify {
public:
  // Notification of tcp events
  virtual void OnTcpOpen(PseudoTcp * tcp) = 0;
  virtual void OnTcpReadable(PseudoTcp * tcp) = 0;
  virtual void OnTcpWriteable(PseudoTcp * tcp) = 0;
  virtual void OnTcpClosed(PseudoTcp * tcp, uint32 nError) = 0;

  // Write the packet onto the network
  enum WriteResult { WR_SUCCESS, WR_TOO_LARGE, WR_BLOCKING, WR_FAIL };
  virtual WriteResult TcpWritePacket(PseudoTcp * tcp, const char * buffer, size_t len) = 0;
};

//////////////////////////////////////////////////////////////////////
// PseudoTcp
//////////////////////////////////////////////////////////////////////

class PseudoTcp {
public:
  PseudoTcp(IPseudoTcpNotify * notify, uint32 conv);
  virtual ~PseudoTcp();

  int Connect();
  int Recv(char * buffer, size_t len);
  int Send(const char * buffer, size_t len);
  void Close();
  int GetError();

  enum TcpState { TCP_LISTEN, TCP_SYN_SENT, TCP_SYN_RECEIVED, TCP_ESTABLISHED, TCP_CLOSED };
  TcpState State() const { return m_state; }

  // Call this when the PMTU changes.
  void NotifyMTU(uint16 mtu);

  // Call this based on timeout value returned from GetNextClock.
  // It's ok to call this too frequently.
  void NotifyClock(uint32 now);

  // Call this whenever a packet arrives.
  // Returns false if no more packets are needed.
  bool NotifyPacket(const char * buffer, size_t len);

  // Call this to determine the next time NotifyClock should be called.
  // Returns false if the socket is ready to be destroyed.
  bool GetNextClock(uint32 now, long& timeout);

protected:
	enum SendFlags { sfNone, sfDelayedAck, sfImmediateAck };
	enum TransmitResult { trFail, trWait, trOK };
  enum { BUF_SIZE = 1024 * 60 }; // Note: can't go as high as 1024 * 64, because of uint16 precision

	struct Segment {
		uint32 conv, seq, ack;
		uint8 flags;
		uint16 wnd;
		const char * data;
		uint32 len;
		uint32 tsval, tsecr;
	};

	struct SSegment {
		uint32 seq, len;
		//uint32 tstamp;
		uint8 xmit;
		bool bCtrl;

		SSegment(uint32 s, uint32 l, bool c) : seq(s), len(l), /*tstamp(0),*/ xmit(0), bCtrl(c) { }
	};
	typedef std::list<SSegment> SList;

	struct RSegment {
		uint32 seq, len;
	};

	uint32 queue(const char * data, uint32 len, bool bCtrl);

  IPseudoTcpNotify::WriteResult packet(uint32 seq, uint8 flags, const char * data, uint32 len);
	bool parse(const uint8 * buffer, uint32 size);

	void attemptSend(SendFlags sflags = sfNone);

	void closedown(uint32 err = 0);

	bool check(uint32 now, long& nTimeout);

	bool process(Segment& seg);
	TransmitResult transmit(const SList::iterator& seg, uint32 now);

	void adjustMTU();

private:
  IPseudoTcpNotify * m_notify;
  bool m_shutdown;
  int m_error;

  // TCB data
	TcpState m_state;
	uint32 m_conv;
	bool m_bReadEnable, m_bWriteEnable, m_bOutgoing;
	uint32 m_lasttraffic;

	// Incoming data
	typedef std::list<RSegment> RList;
	RList m_rlist;
	char m_rbuf[BUF_SIZE];
	uint32 m_rcv_nxt, m_rcv_wnd, m_rlen, m_lastrecv;

	// Outgoing data
	SList m_slist;
	char m_sbuf[BUF_SIZE];
	uint32 m_snd_nxt, m_snd_wnd, m_slen, m_lastsend, m_snd_una;
	// Maximum segment size, estimated protocol level, largest segment sent
	uint32 m_mss, m_msslevel, m_largest, m_mtu_advise;
	// Retransmit timer
	uint32 m_rto_base;

	// Timestamp tracking
	uint32 m_ts_recent, m_ts_lastack;

	// Round-trip calculation
	uint32 m_rx_rttvar, m_rx_srtt, m_rx_rto;

	// Congestion avoidance, Fast retransmit/recovery, Delayed ACKs
	uint32 m_ssthresh, m_cwnd;
	uint8 m_dup_acks;
	uint32 m_recover;
	uint32 m_t_ack;
};

//////////////////////////////////////////////////////////////////////

} // namespace cricket

#endif // __PSEUDOTCP_H__
