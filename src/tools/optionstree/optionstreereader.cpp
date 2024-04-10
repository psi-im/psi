#include "optionstreereader.h"

#include "optionstree.h"
#include "varianttree.h"

#include <QBuffer>
#include <QRect>
#include <QSize>

OptionsTreeReader::OptionsTreeReader(OptionsTree *options) : options_(options) { Q_ASSERT(options_); }

bool OptionsTreeReader::read(QIODevice *device)
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

void OptionsTreeReader::readTree(VariantTree *tree)
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
            } else {
                QVariant v = readVariant(attributes().value("type").toString());
                if (v.isValid()) {
                    tree->values_[name().toString()] = v;
                } else {
                    tree->unknowns2_[name().toString()] = unknown_;
                }
            }
        }
    }
}

QVariant OptionsTreeReader::readVariant(const QString &type)
{
    QVariant result;
    if (type == QLatin1String("QStringList")) {
        result = readStringList();
    } else if (type == QLatin1String("QVariantList")) {
        result = readVariantList();
    } else if (type == QLatin1String("QSize")) {
        result = readSize();
    } else if (type == QLatin1String("QRect")) {
        result = readRect();
    } else if (type == QLatin1String("QByteArray")) {
        result = QByteArray();
        result = QByteArray::fromBase64(readElementText().toLatin1());
    } else {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QVariant::Type variantType;
#else
        QMetaType::Type variantType;
#endif
        bool known = true;

        if (type == QLatin1String("QString")) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            variantType = QVariant::String;
#else
            variantType = QMetaType::QString;
#endif
        } else if (type == QLatin1String("bool")) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            variantType = QVariant::Bool;
#else
            variantType = QMetaType::Bool;
#endif
        } else if (type == QLatin1String("int")) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            variantType = QVariant::Int;
#else
            variantType = QMetaType::Int;
#endif
        } else if (type == QLatin1String("QKeySequence")) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            variantType = QVariant::KeySequence;
#else
            variantType = QMetaType::QKeySequence;
#endif
        } else if (type == QLatin1String("QColor")) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            variantType = QVariant::Color;
#else
            variantType = QMetaType::QColor;
#endif
        } else {
            known = false;
        }

        if (known) {
            result = readElementText();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            result.convert(int(variantType));
#else
            result.convert(QMetaType(variantType));
#endif
        } else {
            [[maybe_unused]] QString result;
            QByteArray               ba;
            QBuffer                  buffer(&ba);
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
            if (name() == QLatin1String { "item" }) {
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
            if (name() == QLatin1String { "item" }) {
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
            if (name() == QLatin1String { "width" }) {
                width = readElementText().toInt();
            } else if (name() == QLatin1String { "height" }) {
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
            if (name() == QLatin1String { "width" }) {
                width = readElementText().toInt();
            } else if (name() == QLatin1String { "height" }) {
                height = readElementText().toInt();
            } else if (name() == QLatin1String { "x" }) {
                x = readElementText().toInt();
            } else if (name() == QLatin1String { "y" }) {
                y = readElementText().toInt();
            }
        }
    }
    return QRect(x, y, width, height);
}

void OptionsTreeReader::readUnknownElement(QXmlStreamWriter *writer)
{
    Q_ASSERT(isStartElement());
    writer->writeStartElement(name().toString());
    const auto &attrs = attributes();
    for (const QXmlStreamAttribute &attr : attrs) {
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
