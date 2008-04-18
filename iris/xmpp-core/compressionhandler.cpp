#include <QTimer>
#include <QDebug>

#include "compressionhandler.h"
#include "compress.h"

CompressionHandler::CompressionHandler()
	: errorCode_(0)
{
	outgoing_buffer_.open(QIODevice::ReadWrite);
	compressor_ = new Compressor(&outgoing_buffer_);
	
	incoming_buffer_.open(QIODevice::ReadWrite);
	decompressor_ = new Decompressor(&incoming_buffer_);
}

CompressionHandler::~CompressionHandler()
{
	delete compressor_;
	delete decompressor_;
}

void CompressionHandler::writeIncoming(const QByteArray& a)
{
	//qDebug("CompressionHandler::writeIncoming");
	//qDebug() << (QString("Incoming %1 bytes").arg(a.size()).toAscii());
	errorCode_ = decompressor_->write(a);
	if (!errorCode_) 
		QTimer::singleShot(0, this, SIGNAL(readyRead()));
	else
		QTimer::singleShot(0, this, SIGNAL(error()));
}

void CompressionHandler::write(const QByteArray& a)
{
	//qDebug() << (QString("CompressionHandler::write(%1)").arg(a.size()).toAscii());
	errorCode_ = compressor_->write(a);
	if (!errorCode_)
		QTimer::singleShot(0, this, SIGNAL(readyReadOutgoing()));
	else
		QTimer::singleShot(0, this, SIGNAL(error()));
}

QByteArray CompressionHandler::read()
{
	//qDebug("CompressionHandler::read");
	QByteArray b = incoming_buffer_.buffer();
	incoming_buffer_.buffer().clear();
	incoming_buffer_.reset();
	return b;
}

QByteArray CompressionHandler::readOutgoing(int* i) 
{
	//qDebug("CompressionHandler::readOutgoing");
	//qDebug() << (QString("Outgoing %1 bytes").arg(outgoing_buffer_.size()).toAscii());
	QByteArray b = outgoing_buffer_.buffer();
	outgoing_buffer_.buffer().clear();
	outgoing_buffer_.reset();
	*i = b.size();
	return b;
}

int CompressionHandler::errorCode()
{
	return errorCode_;
}
