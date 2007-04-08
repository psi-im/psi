#ifndef STATUS_H
#define STATUS_H

#include <QString>

class Status
{
public:
	enum Type { Online, Away, FFC, XA, DND, Offline };

	Status(Type type, const QString& message) : type_(type), message_(message) {};

	Type type() const { return type_; }
	const QString& message() const { return message_; }

private:
	Type type_;
	QString message_;
};

#endif
