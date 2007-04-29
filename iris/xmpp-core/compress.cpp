#include <QtCore> // for qWarning()
#include <QObject>
#include <QIODevice>
#include <zlib.h>

#include "compress.h"

#define CHUNK_SIZE 1024

Compressor::Compressor(QIODevice* device, int compression) : device_(device)
{
	zlib_stream_ = (z_stream*) malloc(sizeof(z_stream));
	zlib_stream_->zalloc = Z_NULL;
	zlib_stream_->zfree = Z_NULL;
	zlib_stream_->opaque = Z_NULL;
	int result = deflateInit(zlib_stream_, compression);
	Q_ASSERT(result == Z_OK);
	Q_UNUSED(result);
	connect(device, SIGNAL(aboutToClose()), this, SLOT(flush()));
	flushed_ = false;
}

Compressor::~Compressor()
{
	flush();
}

void Compressor::flush()
{
	if (flushed_)
		return;
	
	// Flush
	write(QByteArray(),true);
	int result = deflateEnd(zlib_stream_);
	if (result != Z_OK) 
		qWarning(QString("compressor.c: deflateEnd failed (%1)").arg(result).toAscii());
	
	flushed_ = true;
}

int Compressor::write(const QByteArray& input)
{
	return write(input,false);
}

int Compressor::write(const QByteArray& input, bool flush)
{
	int result;
	zlib_stream_->avail_in = input.size();
	zlib_stream_->next_in = (Bytef*) input.data();
	QByteArray output;

	// Write the data
	int output_position = 0;
	do {
		output.resize(output_position + CHUNK_SIZE);
		zlib_stream_->avail_out = CHUNK_SIZE;
		zlib_stream_->next_out = (Bytef*) (output.data() + output_position);
		result = deflate(zlib_stream_,(flush ? Z_FINISH : Z_NO_FLUSH));
		if (result == Z_STREAM_ERROR) {
			qWarning(QString("compressor.cpp: Error ('%1')").arg(zlib_stream_->msg).toAscii());
			return result;
		}
		output_position += CHUNK_SIZE;
	}
	while (zlib_stream_->avail_out == 0);
	if (zlib_stream_->avail_in != 0) {
		qWarning("Compressor: avail_in != 0");
	}
	output_position -= zlib_stream_->avail_out;

	// Flush the data
	if (!flush) {
		do {
			output.resize(output_position + CHUNK_SIZE);
			zlib_stream_->avail_out = CHUNK_SIZE;
			zlib_stream_->next_out = (Bytef*) (output.data() + output_position);
			result = deflate(zlib_stream_,Z_SYNC_FLUSH);
			if (result == Z_STREAM_ERROR) {
				qWarning(QString("compressor.cpp: Error ('%1')").arg(zlib_stream_->msg).toAscii());
				return result;
			}
			output_position += CHUNK_SIZE;
		}
		while (zlib_stream_->avail_out == 0);
		output_position -= zlib_stream_->avail_out;
	}
	output.resize(output_position);

	// Write the compressed data
	device_->write(output);
	return 0;
}

// -----------------------------------------------------------------------------

Decompressor::Decompressor(QIODevice* device) : device_(device)
{
	zlib_stream_ = (z_stream*) malloc(sizeof(z_stream));
	zlib_stream_->zalloc = Z_NULL;
	zlib_stream_->zfree = Z_NULL;
	zlib_stream_->opaque = Z_NULL;
	int result = inflateInit(zlib_stream_);
	Q_ASSERT(result == Z_OK);
	Q_UNUSED(result);
	connect(device, SIGNAL(aboutToClose()), this, SLOT(flush()));
	flushed_ = false;
}

Decompressor::~Decompressor()
{
	flush();
}

void Decompressor::flush()
{
	if (flushed_)
		return;
	
	// Flush
	write(QByteArray(),true);
	int result = inflateEnd(zlib_stream_);
	if (result != Z_OK) 
		qWarning(QString("compressor.c: inflateEnd failed (%1)").arg(result).toAscii());
	
	flushed_ = true;
}

int Decompressor::write(const QByteArray& input)
{
	return write(input,false);
}

int Decompressor::write(const QByteArray& input, bool flush)
{
	int result;
	zlib_stream_->avail_in = input.size();
	zlib_stream_->next_in = (Bytef*) input.data();
	QByteArray output;

	// Write the data
	int output_position = 0;
	do {
		output.resize(output_position + CHUNK_SIZE);
		zlib_stream_->avail_out = CHUNK_SIZE;
		zlib_stream_->next_out = (Bytef*) (output.data() + output_position);
		result = inflate(zlib_stream_,(flush ? Z_FINISH : Z_NO_FLUSH));
		if (result == Z_STREAM_ERROR) {
			qWarning(QString("compressor.cpp: Error ('%1')").arg(zlib_stream_->msg).toAscii());
			return result;
		}
		output_position += CHUNK_SIZE;
	}
	while (zlib_stream_->avail_out == 0);
	//Q_ASSERT(zlib_stream_->avail_in == 0);
	if (zlib_stream_->avail_in != 0) {
		qWarning() << "Decompressor: Unexpected state: avail_in=" << zlib_stream_->avail_in << ",avail_out=" << zlib_stream_->avail_out << ",result=" << result;
		return Z_STREAM_ERROR; // FIXME: Should probably return 'result'
	}
	output_position -= zlib_stream_->avail_out;

	// Flush the data
	if (!flush) {
		do {
			output.resize(output_position + CHUNK_SIZE);
			zlib_stream_->avail_out = CHUNK_SIZE;
			zlib_stream_->next_out = (Bytef*) (output.data() + output_position);
			result = inflate(zlib_stream_,Z_SYNC_FLUSH);
			if (result == Z_STREAM_ERROR) {
				qWarning(QString("compressor.cpp: Error ('%1')").arg(zlib_stream_->msg).toAscii());
				return result;
			}
			output_position += CHUNK_SIZE;
		}
		while (zlib_stream_->avail_out == 0);
		output_position -= zlib_stream_->avail_out;
	}
	output.resize(output_position);

	// Write the compressed data
	device_->write(output);
	return 0;
}
