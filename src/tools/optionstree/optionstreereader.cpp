#include "optionstreereader.h"

#include <QSize>
#include <QRect>
#include <QBuffer>

#include "optionstree.h"
#include "varianttree.h"
#include "xmpp/base64/base64.h"

OptionsTreeReader::OptionsTreeReader(OptionsTree* options)
	: options_(options)
{
	Q_ASSERT(options_);
}

bool OptionsTreeReader::read(QIODevice* device)
{
	setDevice(device);

	while (!atEnd()) {
		readNext();

		if (isStartElement()) {
			readTree(&options_->tree_);
		}
	}

	return !error();
}

void OptionsTreeReader::readTree(VariantTree* tree)
{
	Q_ASSERT(isStartElement());
	Q_ASSERT(tree);

	while (!atEnd()) {
		readNext();

		if (isEndElement())
			break;

		if (isStartElement()) {
			if (!attributes().value("comment").isEmpty()) {
				tree->comments_[name().toString()] = attributes().value("comment").toString();
			}

			if (attributes().value("type").isEmpty()) {
				if (!tree->trees_.contains(name().toString()))
					tree->trees_[name().toString()] = new VariantTree(tree);
				readTree(tree->trees_[name().toString()]);
			}
			else {
				QVariant v = readVariant(attributes().value("type").toString());
				if (v.isValid()) {
					tree->values_[name().toString()] = v;
				}
				else {
					tree->unknowns2_[name().toString()] = unknown_;
				}
			}
		}
	}
}

QVariant OptionsTreeReader::readVariant(const QString& type)
{
	QVariant result;
	if (type == "QStringList") {
		result = readStringList();
	}
	else if (type == "QVariantList") {
		result = readVariantList();
	}
	else if (type == "QSize") {
		result = readSize();
	}
	else if (type == "QRect") {
		result = readRect();
	}
	else if (type == "QByteArray") {
		result = QByteArray();
		result = XMPP::Base64::decode(readElementText());
	}
	else {
		QVariant::Type varianttype;
		bool known = true;

		if (type=="QString") {
			varianttype = QVariant::String;
		} else if (type=="bool") {
			varianttype = QVariant::Bool;
		} else if (type=="int") {
			varianttype = QVariant::Int;
		} else if (type == "QKeySequence") {
			varianttype = QVariant::KeySequence;
		} else if (type == "QColor") {
			varianttype = QVariant::Color;
		} else {
			known = false;
		}

		if (known) {
			result = readElementText();
			result.convert(varianttype);
		}
		else {
			QString result;
			QByteArray ba;
			QBuffer buffer(&ba);
			buffer.open(QIODevice::WriteOnly);
			QXmlStreamWriter writer;
			writer.setDevice(&buffer);

			writer.writeStartDocument();
			readUnknownElement(&writer);
			writer.writeEndDocument();
			buffer.close();

			// qWarning("ba: %d, '%s'", ba.length(), qPrintable(QString::fromUtf8(ba)));
			unknown_ = QString::fromUtf8(ba);
		}
	}
	return result;
}

QStringList OptionsTreeReader::readStringList()
{
	QStringList list;
	while (!atEnd()) {
		readNext();

		if (isEndElement())
			break;

		if (isStartElement()) {
			if (name() == "item") {
				list << readElementText();
			}
		}
	}
	return list;
}

QVariantList OptionsTreeReader::readVariantList()
{
	QVariantList list;
	while (!atEnd()) {
		readNext();

		if (isEndElement())
			break;

		if (isStartElement()) {
			if (name() == "item") {
				list << readVariant(attributes().value("type").toString());
			}
		}
	}
	return list;
}

QSize OptionsTreeReader::readSize()
{
	int width = 0, height = 0;
	while (!atEnd()) {
		readNext();

		if (isEndElement())
			break;

		if (isStartElement()) {
			if (name() == "width") {
				width = readElementText().toInt();
			}
			else if (name() == "height") {
				height = readElementText().toInt();
			}
		}
	}
	return QSize(width, height);
}

QRect OptionsTreeReader::readRect()
{
	int x = 0, y = 0, width = 0, height = 0;
	while (!atEnd()) {
		readNext();

		if (isEndElement())
			break;

		if (isStartElement()) {
			if (name() == "width") {
				width = readElementText().toInt();
			}
			else if (name() == "height") {
				height = readElementText().toInt();
			}
			else if (name() == "x") {
				x = readElementText().toInt();
			}
			else if (name() == "y") {
				y = readElementText().toInt();
			}
		}
	}
	return QRect(x, y, width, height);
}

void OptionsTreeReader::readUnknownElement(QXmlStreamWriter* writer)
{
	Q_ASSERT(isStartElement());
	writer->writeStartElement(name().toString());
	foreach(QXmlStreamAttribute attr, attributes()) {
		writer->writeAttribute(attr.name().toString(), attr.value().toString());
	}

	while (!atEnd()) {
		writer->writeCharacters(text().toString());
		readNext();

		if (isEndElement())
			break;

		if (isStartElement())
			readUnknownElement(writer);
	}

	writer->writeEndElement();
}
