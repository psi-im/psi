/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#ifndef INCREMENTINGRANDOMNUMBERGENERATOR_H
#define INCREMENTINGRANDOMNUMBERGENERATOR_H

#include <QtDebug>

#include "xmpp/base/randomnumbergenerator.h"

namespace XMPP {
	class IncrementingRandomNumberGenerator : public RandomNumberGenerator
	{
		public:
			IncrementingRandomNumberGenerator(int maximumNumber = 10) : maximumNumber_(maximumNumber), currentNumber_(maximumNumber_) {}
		
			virtual double generateNumber() const {
				currentNumber_ = (currentNumber_ + 1) % (maximumNumber_ + 1); 
				return currentNumber_;
			}

			virtual double getMaximumGeneratedNumber() const {
				return maximumNumber_;
			}
		
		private:
			int maximumNumber_;
			mutable int currentNumber_;
	};
}

#endif
