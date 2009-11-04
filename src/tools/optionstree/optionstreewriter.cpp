#include "optionstreewriter.h"

#include <QSize>
#include <QRect>
#include <QKeySequence>
#include <QBuffer>

#include "optionstree.h"
#include "varianttree.h"
#include "xmpp/base64/base64.h"

OptionsTreeWriter::OptionsTreeWriter(const OptionsTree* options)
	: options_(options)
{
	Q_ASSERT(options_);
}

void OptionsTreeWriter::setName(const QString& configName)
{
	configName_ = configName;
}

void OptionsTreeWriter::setNameSpace(const QString& configNS)
{
	configNS_ = configNS;
}

void OptionsTreeWriter::setVersion(const QString& configVersion)
{
	configVersion_ = configVersion;
}

bool OptionsTreeWriter::write(QIODevice* device)
{
	setDevice(device);

	// turn it off for even more speed
	setAutoFormatting(true);
	setAutoFormattingIndent(1);

	writeStartDocument();
	writeDTD(QString("<!DOCTYPE %1>").arg(configName_));
	writeStartElement(configName_);
	writeAttribute("version", configVersion_);
	writeAttribute("xmlns", configNS_);

	writeTree(&options_->tree_);

	writeEndDocument();
	return true;
}

void OptionsTreeWriter::writeTree(const VariantTree* tree)
{
	foreach(QString node, tree->trees_.keys()) {
		Q_ASSERT(!node.isEmpty());
		writeStartElement(node);
		if (tree->comments_.contains(node))
			writeAttribute("comment", tree->comments_[node]);

		writeTree(tree->trees_[node]);
		writeEndElement();
	}

	foreach(QString child, tree->values_.keys()) {
		Q_ASSERT(!child.isEmpty());
		writeStartElement(child);
		if (tree->comments_.contains(child))
			writeAttribute("comment", tree->comments_[child]);

		writeVariant(tree->values_[child]);
		writeEndElement();
	}

	foreach(QString unknown, tree->unknowns2_.keys()) {
		writeUnknown(tree->unknowns2_[unknown]);
	}
}

void OptionsTreeWriter::writeVariant(const QVariant& variant)
{
	writeAttribute("type", variant.typeName());
	if (variant.type() == QVariant::StringList) {
		foreach(QString s, variant.toStringList()) {
			writeStartElement("item");
			writeCharacters(s);
			writeEndElement();
		}
	}
	else if (variant.type() == QVariant::List) {
		foreach(QVariant v, variant.toList()) {
			writeStartElement("item");
			writeVariant(v);
			writeEndElement();
		}
	}
	else if (variant.type() == QVariant::Size) {
		writeTextElement("width", QString::number(variant.toSize().width()));
		writeTextElement("height", QString::number(variant.toSize().height()));
	}
	else if (variant.type() == QVariant::Rect) {
		writeTextElement("x", QString::number(variant.toRect().x()));
		writeTextElement("y", QString::number(variant.toRect().y()));
		writeTextElement("width", QString::number(variant.toRect().width()));
		writeTextElement("height", QString::number(variant.toRect().height()));
	}
	else if (variant.type() == QVariant::ByteArray) {
		writeCharacters(XMPP::Base64::encode(variant.toByteArray()));
	}
	else if (variant.type() == QVariant::KeySequence) {
		QKeySequence k = variant.value<QKeySequence>();
		writeCharacters(k.toString());
	}
	else {
		writeCharacters(variant.toString());
	}
}

void OptionsTreeWriter::writeUnknown(const QString& unknown)
{
	QByteArray ba = unknown.toUtf8();
	QBuffer buffer(&ba);
	buffer.open(QIODevice::ReadOnly);
	QXmlStreamReader reader;
	reader.setDevice(&buffer);

	while (!reader.atEnd()) {
		reader.readNext();

		if (reader.isStartElement()) {
			readUnknownTree(&reader);
		}
	}
}

void OptionsTreeWriter::readUnknownTree(QXmlStreamReader* reader)
{
	Q_ASSERT(reader->isStartElement());
	writeStartElement(reader->name().toString());
	foreach(QXmlStreamAttribute attr, reader->attributes()) {
		writeAttribute(attr.name().toString(), attr.value().toString());
	}

	while (!reader->atEnd()) {
		writeCharacters(reader->text().toString());
		reader->readNext();

		if (reader->isEndElement())
			break;

		if (reader->isStartElement())
			readUnknownTree(reader);
	}

	writeEndElement();
}
