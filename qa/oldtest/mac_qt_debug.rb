#!/usr/bin/env ruby -wKU

ENABLE_DEBUG = ARGV[0] == "true"
EXEC_TARGET = ARGV[1]

QT_FRAMEWORK_VERSION = 4
QT_FRAMEWORKS = %w(QtCore QtXml QtNetwork QtGui QtSql Qt3Support)

QT_FRAMEWORKS.each do |f|
  framework = "$QTDIR/lib/#{f}.framework/Versions/#{QT_FRAMEWORK_VERSION}/#{f}"
  if ENABLE_DEBUG
    `install_name_tool -id "#{framework}" "#{framework}_debug"`
    `install_name_tool -change "#{framework}" "#{framework}_debug" "./#{EXEC_TARGET}"`
  else
    `install_name_tool -id "#{framework}_debug" "#{framework}_debug"`
    `install_name_tool -change "#{framework}_debug" "#{framework}" "./#{EXEC_TARGET}"`
  end
end
