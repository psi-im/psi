#ifndef ZLIBDECOMPRESSOR_H
#define ZLIBDECOMPRESSOR_H

#include <QObject>

#include "zlib.h"

class QIODevice;

class ZLibDecompressor : public QObject
{
	Q_OBJECT

public:
	ZLibDecompressor(QIODevice* device);
	~ZLibDecompressor();

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
