/*
 * uploadservice.cpp
 *
 *  Created on: 24 Sep 2016
 *      Author: rkfg
 */

#include "uploadservice.h"

UploadService::UploadService(const QString& serviceName, int sizeLimit) :
		serviceName_(serviceName), sizeLimit_(sizeLimit) {
}
