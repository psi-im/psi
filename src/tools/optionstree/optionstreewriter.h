#ifndef OPTIONSTREEWRITER_H
#define OPTIONSTREEWRITER_H

#include "atomicxmlfile/atomicxmlfile.h"

#include <QVariant>

class OptionsTree;
class VariantTree;

class OptionsTreeWriter : public AtomicXmlFileWriter
{
public:
	OptionsTreeWriter(const OptionsTree*);

	void setName(const QString& configName);
	void setNameSpace(const QString& configNS);
	void setVersion(const QString& configVersion);

	// reimplemented
	virtual bool write(QIODevice* device);

protected:
	void writeTree(const VariantTree* tree);
	void writeVariant(const QVariant& variant);
	void writeUnknown(const QString& unknown);
	void readUnknownTree(QXmlStreamReader* reader);

private:
	const OptionsTree* options_;
	QString configName_;
	QString configNS_;
	QString configVersion_;
};

#endif
