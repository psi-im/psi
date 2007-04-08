#ifndef COMPRESSIONHANDLER_H
#define COMPRESSIONHANDLER_H

#include <QObject>
#include <QBuffer>

class Compressor;
class Decompressor;

class CompressionHandler : public QObject
{
	Q_OBJECT

public:
	CompressionHandler();
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
	Compressor* compressor_;
	Decompressor* decompressor_;
	QBuffer outgoing_buffer_, incoming_buffer_;
	int errorCode_;
};

#endif
