TEMPLATE = lib
QT -= gui
CONFIG += staticlib
TARGET = cppunit_psi

CPPUNIT_DIR = cppunit
MOC_DIR = .moc
OBJECTS_DIR = .obj

windows {
	CONFIG -= debug_and_release
}

INCLUDEPATH += \
  $$CPPUNIT_DIR/include

SOURCES += \
  $$CPPUNIT_DIR/src/cppunit/AdditionalMessage.cpp \
  $$CPPUNIT_DIR/src/cppunit/Asserter.cpp \
  $$CPPUNIT_DIR/src/cppunit/BeOsDynamicLibraryManager.cpp \
  $$CPPUNIT_DIR/src/cppunit/BriefTestProgressListener.cpp \
  $$CPPUNIT_DIR/src/cppunit/CompilerOutputter.cpp \
  $$CPPUNIT_DIR/src/cppunit/DefaultProtector.cpp \
  $$CPPUNIT_DIR/src/cppunit/DynamicLibraryManager.cpp \
  $$CPPUNIT_DIR/src/cppunit/DynamicLibraryManagerException.cpp \
  $$CPPUNIT_DIR/src/cppunit/Exception.cpp \
  $$CPPUNIT_DIR/src/cppunit/Message.cpp \
  $$CPPUNIT_DIR/src/cppunit/PlugInManager.cpp \
  $$CPPUNIT_DIR/src/cppunit/PlugInParameters.cpp \
  $$CPPUNIT_DIR/src/cppunit/Protector.cpp \
  $$CPPUNIT_DIR/src/cppunit/ProtectorChain.cpp \
  $$CPPUNIT_DIR/src/cppunit/RepeatedTest.cpp \
  $$CPPUNIT_DIR/src/cppunit/ShlDynamicLibraryManager.cpp \
  $$CPPUNIT_DIR/src/cppunit/SourceLine.cpp \
  $$CPPUNIT_DIR/src/cppunit/StringTools.cpp \
  $$CPPUNIT_DIR/src/cppunit/SynchronizedObject.cpp \
  $$CPPUNIT_DIR/src/cppunit/Test.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestAssert.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestCase.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestCaseDecorator.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestComposite.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestDecorator.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestFactoryRegistry.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestFailure.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestLeaf.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestNamer.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestPath.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestPlugInDefaultImpl.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestResult.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestResultCollector.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestRunner.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestSetUp.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestSuccessListener.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestSuite.cpp \
  $$CPPUNIT_DIR/src/cppunit/TestSuiteBuilderContext.cpp \
  $$CPPUNIT_DIR/src/cppunit/TextOutputter.cpp \
  $$CPPUNIT_DIR/src/cppunit/TextTestProgressListener.cpp \
  $$CPPUNIT_DIR/src/cppunit/TextTestResult.cpp \
  $$CPPUNIT_DIR/src/cppunit/TextTestRunner.cpp \
  $$CPPUNIT_DIR/src/cppunit/TypeInfoHelper.cpp \
  $$CPPUNIT_DIR/src/cppunit/XmlDocument.cpp \
  $$CPPUNIT_DIR/src/cppunit/XmlElement.cpp \
  $$CPPUNIT_DIR/src/cppunit/XmlOutputter.cpp \
  $$CPPUNIT_DIR/src/cppunit/XmlOutputterHook.cpp

unix {
  SOURCES += \
    $$CPPUNIT_DIR/src/cppunit/UnixDynamicLibraryManager.cpp \
}
windows {
  SOURCES += \
    $$CPPUNIT_DIR/src/cppunit/Win32DynamicLibraryManager.cpp \
    $$CPPUNIT_DIR/src/cppunit/DllMain.cpp
}

include(../../conf.pri)
