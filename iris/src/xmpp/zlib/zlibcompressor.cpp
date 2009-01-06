#include "xmpp/zlib/zlibcompressor.h"

#include <QtDebug>
#include <QObject>
#include <QIODevice>
#include <zlib.h>

#include "common.h"

ZLibCompressor::ZLibCompressor(QIODevice* device, int compression) : device_(device)
{
	zlib_stream_ = (z_stream*) malloc(sizeof(z_stream));
	initZStream(zlib_stream_);
	int result = deflateInit(zlib_stream_, compression);
	Q_ASSERT(result == Z_OK);
	Q_UNUSED(result);
	connect(device, SIGNAL(aboutToClose()), this, SLOT(flush()));
	flushed_ = false;
}

ZLibCompressor::~ZLibCompressor()
{
	flush();
	free(zlib_stream_);
}

void ZLibCompressor::flush()
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

int ZLibCompressor::write(const QByteArray& input)
{
	return write(input,false);
}

int ZLibCompressor::write(const QByteArray& input, bool flush)
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
		qWarning("ZLibCompressor: avail_in != 0");
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
