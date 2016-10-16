/*
 * uploadservice.h
 *
 *  Created on: 24 Sep 2016
 *      Author: rkfg
 */

#ifndef SRC_PLUGINS_GENERIC_HTTPUPLOADPLUGIN_UPLOADSERVICE_H_
#define SRC_PLUGINS_GENERIC_HTTPUPLOADPLUGIN_UPLOADSERVICE_H_

#include <QString>

class UploadService {
public:
	UploadService(const QString& serviceName, int sizeLimit);

	const QString& serviceName() const {
		return serviceName_;
	}

	int sizeLimit() const {
		return sizeLimit_;
	}

private:
	QString serviceName_;
	int sizeLimit_;
};

#endif /* SRC_PLUGINS_GENERIC_HTTPUPLOADPLUGIN_UPLOADSERVICE_H_ */
