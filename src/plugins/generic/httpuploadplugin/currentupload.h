/*
 * CurrentUpload.h
 *
 *  Created on: 24 Sep 2016
 *      Author: rkfg
 */

#ifndef SRC_PLUGINS_GENERIC_HTTPUPLOADPLUGIN_CURRENTUPLOAD_H_
#define SRC_PLUGINS_GENERIC_HTTPUPLOADPLUGIN_CURRENTUPLOAD_H_

struct CurrentUpload {
	QString from;
	QString to;
	int account = -1;
	QString getUrl;
	QString type;
};

#endif /* SRC_PLUGINS_GENERIC_HTTPUPLOADPLUGIN_CURRENTUPLOAD_H_ */
