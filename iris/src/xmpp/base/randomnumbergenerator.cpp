/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#include "xmpp/base/randomnumbergenerator.h"

#include <cassert>

namespace XMPP {

RandomNumberGenerator::~RandomNumberGenerator()
{
}

double RandomNumberGenerator::generateNumberBetween(double a, double b) const
{
	assert(b > a);
	return a + (generateNumber()/getMaximumGeneratedNumber())*(b-a);
}

}
