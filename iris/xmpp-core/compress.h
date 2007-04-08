#ifndef COMPRESS_H
#define COMPRESS_H

#include <QObject>

#include "zlib.h"

class QIODevice;

class Compressor : public QObject
{
	Q_OBJECT

public:
	Compressor(QIODevice* device, int compression = Z_DEFAULT_COMPRESSION);
	~Compressor();

	int write(const QByteArray&);

protected slots:
	void flush();

protected:
	int write(const QByteArray&, bool flush);

private:
	QIODevice* device_;
	z_stream* zlib_stream_;
	bool flushed_;
};

class Decompressor : public QObject
{
	Q_OBJECT

public:
	Decompressor(QIODevice* device);
	~Decompressor();

	int write(const QByteArray&);

protected slots:
	void flush();

protected:
	int write(const QByteArray&, bool flush);

private:
	QIODevice* device_;
	z_stream* zlib_stream_;
	bool flushed_;
};

#endif
