/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#ifndef RANDOMNUMBERGENERATOR_H
#define RANDOMNUMBERGENERATOR_H

namespace XMPP {
	class RandomNumberGenerator
	{
		public:
			virtual ~RandomNumberGenerator();

			double generateNumberBetween(double a, double b) const;

		protected:
			virtual double generateNumber() const = 0;
			virtual double getMaximumGeneratedNumber() const = 0;
	};
}

#endif
