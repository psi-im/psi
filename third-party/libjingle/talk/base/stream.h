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

#ifndef TALK_BASE_STREAM_H__
#define TALK_BASE_STREAM_H__

#include "talk/base/sigslot.h"
#include "talk/base/basictypes.h"
#include "talk/base/scoped_ptr.h"


///////////////////////////////////////////////////////////////////////////////

// StreamInterface is a generic asynchronous stream interface, supporting read,
// write, and close operations, and asynchronous signalling of state changes.
// The interface is designed with file, memory, and socket implementations in
// mind.

// The following enumerations are declared outside of the StreamInterface
// class for brevity in use.

// The SS_OPENING state indicates that the stream will signal open or closed
// in the future.
enum StreamState { SS_CLOSED, SS_OPENING, SS_OPEN };

// Stream read/write methods return this value to indicate various success
// and failure conditions described below.
enum StreamResult { SR_ERROR, SR_SUCCESS, SR_BLOCK, SR_EOS };

// StreamEvents are used to asynchronously signal state transitionss.  The flags
// may be combined.
//  SE_OPEN: The stream has transitioned to the SS_OPEN state
//  SE_CLOSE: The stream has transitioned to the SS_CLOSED state
//  SE_READ: Data is available, so Read is likely to not return SR_BLOCK
//  SE_WRITE: Data can be written, so Write is likely to not return SR_BLOCK
enum StreamEvent { SE_OPEN = 1, SE_READ = 2, SE_WRITE = 4, SE_CLOSE = 8 };

class StreamInterface : public sigslot::has_slots<> {
 public:
  virtual ~StreamInterface() { }

  virtual StreamState GetState() const = 0;

  // Read attempts to fill buffer of size buffer_len.  Write attempts to send
  // data_len bytes stored in data.  The variables read and write are set only
  // on SR_SUCCESS (see below).  Likewise, error is only set on SR_ERROR.
  // Read and Write return a value indicating:
  //  SR_ERROR: an error occurred, which is returned in a non-null error
  //    argument.  Interpretation of the error requires knowledge of the
  //    stream's concrete type, which limits its usefulness.
  //  SR_SUCCESS: some number of bytes were successfully written, which is
  //    returned in a non-null read/write argument.
  //  SR_BLOCK: the stream is in non-blocking mode, and the operation would
  //    block, or the stream is in SS_OPENING state.
  //  SR_EOS: the end-of-stream has been reached, or the stream is in the
  //    SS_CLOSED state.
  virtual StreamResult Read(void* buffer, size_t buffer_len,
                            size_t* read, int* error) = 0;
  virtual StreamResult Write(const void* data, size_t data_len,
                             size_t* written, int* error) = 0;

  // Attempt to transition to the SS_CLOSED state.  SE_CLOSE will not be
  // signalled as a result of this call.
  virtual void Close() = 0;

  // Return the number of bytes that will be returned by Read, if known.
  virtual bool GetSize(size_t* size) const = 0;

  // Returns true if stream could be repositioned to the beginning.
  virtual bool Rewind() = 0;

  // WriteAll is a helper function which repeatedly calls Write until all the
  // data is written, or something other than SR_SUCCESS is returned.  Note that
  // unlike Write, the argument 'written' is always set, and may be non-zero
  // on results other than SR_SUCCESS.  The remaining arguments have the
  // same semantics as Write.
  StreamResult WriteAll(const void* data, size_t data_len,
                        size_t* written, int* error);

  // Similar to ReadAll.  Calls Read until buffer_len bytes have been read, or
  // until a non-SR_SUCCESS result is returned.  'read' is always set.
  StreamResult ReadAll(void* buffer, size_t buffer_len,
                       size_t* read, int* error);

  // ReadLine is a helper function which repeatedly calls Read until it hits
  // the end-of-line character, or something other than SR_SUCCESS.
  // TODO: this is too inefficient to keep here.  Break this out into a buffered
  // readline object or adapter
  StreamResult ReadLine(std::string *line);

  // Streams may signal one or more StreamEvents to indicate state changes.
  // The first argument identifies the stream on which the state change occured.
  // The second argument is a bit-wise combination of StreamEvents.
  // If SE_CLOSE is signalled, then the third argument is the associated error
  // code.  Otherwise, the value is undefined.
  // Note: Not all streams will support asynchronous event signalling.  However,
  // SS_OPENING and SR_BLOCK returned from stream member functions imply that
  // certain events will be raised in the future.
  sigslot::signal3<StreamInterface*, int, int> SignalEvent;

 protected:
  StreamInterface() { }
  
 private:
  DISALLOW_EVIL_CONSTRUCTORS(StreamInterface);
};

///////////////////////////////////////////////////////////////////////////////

// StreamAdapterInterface is a convenient base-class for adapting a stream.
// By default, all operations are pass-through.  Override the methods that you
// require adaptation.  Note that the adapter will delete the adapted stream.

class StreamAdapterInterface : public StreamInterface {
 public:
  explicit StreamAdapterInterface(StreamInterface* stream) : stream_(stream) {
    stream_->SignalEvent.connect(this, &StreamAdapterInterface::OnEvent);
  }

  virtual StreamState GetState() const {
    return stream_->GetState();
  }
  virtual StreamResult Read(void* buffer, size_t buffer_len,
                            size_t* read, int* error) {
    return stream_->Read(buffer, buffer_len, read, error);
  }
  virtual StreamResult Write(const void* data, size_t data_len,
                             size_t* written, int* error) {
    return stream_->Write(data, data_len, written, error);
  }
  virtual void Close() {
    stream_->Close();
  }
  virtual bool GetSize(size_t* size) const {
    return stream_->GetSize(size);
  }
  virtual bool Rewind() {
    return stream_->Rewind();
  }

 protected:
  // Note that the adapter presents itself as the origin of the stream events,
  // since users of the adapter may not recognize the adapted object.
  virtual void OnEvent(StreamInterface* stream, int events, int err) {
    SignalEvent(this, events, err);
  }

 private:
  scoped_ptr<StreamInterface> stream_;
  DISALLOW_EVIL_CONSTRUCTORS(StreamAdapterInterface);
};

///////////////////////////////////////////////////////////////////////////////

// FileStream is a simple implementation of a StreamInterface, which does not
// support asynchronous notification.

class FileStream : public StreamInterface {
 public:
  FileStream();
  virtual ~FileStream();

  // The semantics of filename and mode are the same as stdio's fopen
  virtual bool Open(const char* filename, const char* mode);

  // By default, reads and writes are buffered for efficiency.  Disabling
  // buffering causes writes to block until the bytes on disk are updated.
  virtual bool DisableBuffering();

  virtual StreamState GetState() const;
  virtual StreamResult Read(void* buffer, size_t buffer_len,
                            size_t* read, int* error);
  virtual StreamResult Write(const void* data, size_t data_len,
                             size_t* written, int* error);
  virtual void Close();
  virtual bool GetSize(size_t* size) const;
  virtual bool Rewind() { return SetPosition(0); }

  bool SetPosition(size_t position);
  bool GetPosition(size_t* position) const;

 private:
  FILE* file_;
  DISALLOW_EVIL_CONSTRUCTORS(FileStream);
};

///////////////////////////////////////////////////////////////////////////////

// MemoryStream is a simple implementation of a StreamInterface over in-memory
// data.  It does not support asynchronous notification.

class MemoryStream : public StreamInterface {
 public:
  enum { SIZE_UNKNOWN = static_cast<size_t>(-1) };

  MemoryStream();
  // Pre-populate stream with the provided data.
  MemoryStream(const char* data, size_t length = SIZE_UNKNOWN);
  virtual ~MemoryStream();

  virtual StreamState GetState() const;
  virtual StreamResult Read(void *buffer, size_t bytes, size_t *bytes_read, int *error);
  virtual StreamResult Write(const void *buffer, size_t bytes, size_t *bytes_written, int *error);
  virtual void Close();
  virtual bool GetSize(size_t* size) const;
  virtual bool Rewind() { return SetPosition(0); }

  char* GetBuffer() { return buffer_; }
  const char* GetBuffer() const { return buffer_; }
  bool SetPosition(size_t position);
  bool GetPosition(size_t* position) const;

 private:
  size_t   allocated_length_;
  char*    buffer_;
  size_t   data_length_;
  size_t   seek_position_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(MemoryStream);
};

///////////////////////////////////////////////////////////////////////////////

// Flow attempts to move bytes from source to sink via buffer of size
// buffer_len.  The function returns SR_SUCCESS when source reaches
// end-of-stream (returns SR_EOS), and all the data has been written successful
// to sink.  Alternately, if source returns SR_BLOCK or SR_ERROR, or if sink
// returns SR_BLOCK, SR_ERROR, or SR_EOS, then the function immediately returns
// with the unexpected StreamResult value.

StreamResult Flow(StreamInterface* source,
                  char* buffer, size_t buffer_len,
                  StreamInterface* sink);

///////////////////////////////////////////////////////////////////////////////

#endif  // TALK_BASE_STREAM_H__
