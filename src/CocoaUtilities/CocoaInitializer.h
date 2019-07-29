/*
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2008  Remko Troncon
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
