#ifndef COMPRESSIONHANDLER_H
#define COMPRESSIONHANDLER_H

#include <QObject>
#include <QBuffer>

class ZLibCompressor;
class ZLibDecompressor;

class CompressionHandler : public QObject
{
	Q_OBJECT

public:
	CompressionHandler();
	~CompressionHandler();
	void writeIncoming(const QByteArray& a);
	void write(const QByteArray& a);
	QByteArray read();
	QByteArray readOutgoing(int*);
	int errorCode();

signals:
	void readyRead();
	void readyReadOutgoing();
	void error();

private: 
	ZLibCompressor* compressor_;
	ZLibDecompressor* decompressor_;
	QBuffer outgoing_buffer_, incoming_buffer_;
	int errorCode_;
};

#endif
