#ifndef OPTIONSTREEREADER_H
#define OPTIONSTREEREADER_H

#include "atomicxmlfile/atomicxmlfile.h"

#include <QVariant>

class OptionsTree;
class QRect;
class QSize;
class QStringList;
class VariantTree;

class OptionsTreeReader : public AtomicXmlFileReader {
public:
    OptionsTreeReader(OptionsTree *);

    // reimplemented
    virtual bool read(QIODevice *device);

protected:
    void     readTree(VariantTree *tree);
    QVariant readVariant(const QString &type);
    void     readUnknownElement(QXmlStreamWriter *writer);

    QStringList  readStringList();
    QVariantList readVariantList();
    QSize        readSize();
    QRect        readRect();

private:
    OptionsTree *options_;
    QString      unknown_;
};

#endif // OPTIONSTREEREADER_H
