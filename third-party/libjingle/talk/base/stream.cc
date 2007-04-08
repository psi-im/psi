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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string>
#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/stream.h"

#ifdef WIN32
#define fileno _fileno
#endif

///////////////////////////////////////////////////////////////////////////////

StreamResult StreamInterface::WriteAll(const void* data, size_t data_len,
                                       size_t* written, int* error) {
  StreamResult result = SR_SUCCESS;
  size_t total_written = 0, current_written;
  while (total_written < data_len) {
    result = Write(static_cast<const char*>(data) + total_written,
                   data_len - total_written, &current_written, error);
    if (result != SR_SUCCESS)
      break;
    total_written += current_written;
  }
  if (written)
    *written = total_written;
  return result;
}

StreamResult StreamInterface::ReadAll(void* buffer, size_t buffer_len,
                                      size_t* read, int* error) {
  StreamResult result = SR_SUCCESS;
  size_t total_read = 0, current_read;
  while (total_read < buffer_len) {
    result = Read(static_cast<char*>(buffer) + total_read,
                  buffer_len - total_read, &current_read, error);
    if (result != SR_SUCCESS)
      break;
    total_read += current_read;
  }
  if (read)
    *read = total_read;
  return result;
}

StreamResult StreamInterface::ReadLine(std::string* line) {
  StreamResult result = SR_SUCCESS;
  while (true) {
    char ch;
    result = Read(&ch, sizeof(ch), NULL, NULL);
    if (result != SR_SUCCESS) {
      break;
    }
    if (ch == '\n') {
      break;
    }
    line->push_back(ch);
  }
  if (!line->empty()) {   // give back the line we've collected so far with
    result = SR_SUCCESS;  // a success code.  Otherwise return the last code
  }
  return result;
}

///////////////////////////////////////////////////////////////////////////////

FileStream::FileStream() : file_(NULL) {
}

FileStream::~FileStream() {
  FileStream::Close();
}

bool FileStream::Open(const char* filename, const char* mode) {
  Close();
  file_ = fopen(filename, mode);
  return (file_ != NULL);
}

bool FileStream::DisableBuffering() {
  if (!file_)
    return false;
  return (setvbuf(file_, NULL, _IONBF, 0) == 0);
}

StreamState FileStream::GetState() const {
  return (file_ == NULL) ? SS_CLOSED : SS_OPEN;
}

StreamResult FileStream::Read(void* buffer, size_t buffer_len,
                              size_t* read, int* error) {
  if (!file_)
    return SR_EOS;
  size_t result = fread(buffer, 1, buffer_len, file_);
  if ((result == 0) && (buffer_len > 0)) {
    if (feof(file_))
      return SR_EOS;
    if (error)
      *error = errno;
    return SR_ERROR;
  }
  if (read)
    *read = result;
  return SR_SUCCESS;
}

StreamResult FileStream::Write(const void* data, size_t data_len,
                               size_t* written, int* error) {
  if (!file_)
    return SR_EOS;
  size_t result = fwrite(data, 1, data_len, file_);
  if ((result == 0) && (data_len > 0)) {
    if (error)
      *error = errno;
    return SR_ERROR;
  }
  if (written)
    *written = result;
  return SR_SUCCESS;
}

void FileStream::Close() {
  if (file_) {
    fclose(file_);
    file_ = NULL;
  }
}

bool FileStream::SetPosition(size_t position) {
  if (!file_)
    return false;
  return (fseek(file_, position, SEEK_SET) == 0);
}

bool FileStream::GetPosition(size_t * position) const {
  ASSERT(position != NULL);
  if (!file_ || !position)
    return false;
  long result = ftell(file_);
  if (result < 0)
    return false;
  *position = result;
  return true;
}

bool FileStream::GetSize(size_t * size) const {
  ASSERT(size != NULL);
  if (!file_ || !size)
    return false;
  struct stat file_stats;
  if (fstat(fileno(file_), &file_stats) != 0)
    return false;
  *size = file_stats.st_size;
  return true;
}

///////////////////////////////////////////////////////////////////////////////


MemoryStream::MemoryStream()
  : allocated_length_(0), buffer_(NULL), data_length_(0), seek_position_(0) {
}

MemoryStream::MemoryStream(const char* data, size_t length)
  : allocated_length_(length == SIZE_UNKNOWN ? strlen(data) : length),
    buffer_(static_cast<char*>(malloc(allocated_length_))), 
    data_length_(allocated_length_),
    seek_position_(0) {
  memcpy(buffer_, data, allocated_length_);
}

MemoryStream::~MemoryStream() {
  free(buffer_);
}

StreamState MemoryStream::GetState() const {
  return SS_OPEN;
}

StreamResult MemoryStream::Read(void *buffer, size_t bytes,
    size_t *bytes_read, int *error) {
  StreamResult sr = SR_ERROR;
  int error_value = 0;
  size_t bytes_read_value = 0;

  if (seek_position_ < data_length_) {
    size_t remaining_length = data_length_ - seek_position_;
    if (bytes > remaining_length) {
      // Read partial buffer
      bytes = remaining_length;
    }
    // Read entire buffer
    memcpy(buffer, &buffer_[seek_position_], bytes);
    bytes_read_value = bytes;
    seek_position_ += bytes;
    sr = SR_SUCCESS;
  } else {
    // At end of stream
    error_value = EOF;
    sr = SR_EOS;
  }

  if (bytes_read) {
    *bytes_read = bytes_read_value;
  }
  if (error) {
    *error = error_value;
  }

  return sr;
}

StreamResult MemoryStream::Write(const void *buffer,
    size_t bytes, size_t *bytes_written, int *error) {
  StreamResult sr = SR_SUCCESS;
  int error_value = 0;
  size_t bytes_written_value = 0;

  size_t new_position = seek_position_ + bytes;
  if (new_position > allocated_length_) {
    // Need to reallocate
    // round up to next 0x100 bytes
    size_t new_allocated_length = cricket::_max((new_position | 0xFF) + 1, allocated_length_ * 2);
    char *new_buffer = (char *)realloc(buffer_, new_allocated_length);
    if (new_buffer) {
      buffer_ = new_buffer;
      allocated_length_ = new_allocated_length;
    } else {
      error_value = ENOMEM;
      sr = SR_ERROR;
    }
  }

  if (sr == SR_SUCCESS) {
    bytes_written_value = bytes;
    memcpy(&buffer_[seek_position_], buffer, bytes);
    seek_position_ = new_position;
    if (data_length_ < seek_position_) {
      data_length_ = seek_position_;
    }
  }

  if (bytes_written) {
    *bytes_written = bytes_written_value;
  }
  if (error) {
    *error = error_value;
  }

  return sr;
}

void MemoryStream::Close() {
  // nothing to do
}

bool MemoryStream::SetPosition(size_t position) {
  if (position <= data_length_) {
    seek_position_ = position;
    return true;
  }
  return false;
}

bool MemoryStream::GetPosition(size_t *position) const {
  if (!position) {
    return false;
  }
  *position = seek_position_;
  return true;
}

bool MemoryStream::GetSize(size_t *size) const {
  if (!size) {
    return false;
  }
  *size = data_length_;
  return true;
}

///////////////////////////////////////////////////////////////////////////////

StreamResult Flow(StreamInterface* source,
                  char* buffer, size_t buffer_len,
                  StreamInterface* sink) {
  ASSERT(buffer_len > 0);

  StreamResult result;
  size_t count, read_pos, write_pos;

  bool end_of_stream = false;
  do {
    // Read until buffer is full, end of stream, or error
    read_pos = 0;
    do {
      result = source->Read(buffer + read_pos, buffer_len - read_pos,
                            &count, NULL);
      if (result == SR_EOS) {
        end_of_stream = true;
      } else if (result != SR_SUCCESS) {
        return result;
      } else {
        read_pos += count;
      }
    } while (!end_of_stream && (read_pos < buffer_len));

    // Write until buffer is empty, or error (including end of stream)
    write_pos = 0;
    do {
      result = sink->Write(buffer + write_pos, read_pos - write_pos,
                           &count, NULL);
      if (result != SR_SUCCESS)
        return result;

      write_pos += count;
    } while (write_pos < read_pos);
  } while (!end_of_stream);

  return SR_SUCCESS;
}
