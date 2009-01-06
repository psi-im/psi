/*
 * Copyright (C) 2008  Remko Troncon
 * See COPYING for license details.
 */

#ifndef QTTESTUTIL_TESTREGISTRATION_H
#define QTTESTUTIL_TESTREGISTRATION_H

#include "qttestutil/testregistry.h"

namespace QtTestUtil {

	/**
	 * A wrapper class around a test to manage registration and static
	 * creation of an instance of the test class.
	 * This class is used by QTTESTUTIL_REGISTER_TEST(), and you should not 
	 * use this class directly.
	 */
	template<typename TestClass>
	class TestRegistration 
	{
		public:
			TestRegistration() {
				test_ = new TestClass();
				TestRegistry::getInstance()->registerTest(test_);
			}

			~TestRegistration() {
				delete test_;
			}
		
		private:
			TestClass* test_;
	};

}

#endif
