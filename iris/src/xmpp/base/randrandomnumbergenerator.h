/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#ifndef RANDRANDOMNUMBERGENERATOR_H
#define RANDRANDOMNUMBERGENERATOR_H

#include "xmpp/base/randomnumbergenerator.h"

namespace XMPP {
	class RandRandomNumberGenerator : public RandomNumberGenerator
	{
		public:
			RandRandomNumberGenerator() {}

			virtual double generateNumber() const {
				return rand();
			}

			virtual double getMaximumGeneratedNumber() const {
				return RAND_MAX;
			}
	};
}

#endif
