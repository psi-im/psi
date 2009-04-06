/*
 * Copyright (C) 2008 Remko Troncon
 * See COPYING file for the detailed license.
 */

#ifndef COCOAINITIALIZER_H
#define COCOAINITIALIZER_H

class CocoaInitializer
{
	public:
		CocoaInitializer();
		~CocoaInitializer();
	
	private:
		class Private;
		Private* d;
};

#endif
