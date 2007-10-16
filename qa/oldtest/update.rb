#! /usr/bin/env ruby
# This script is used to update auto-generated 
# files named 'checkall' and 'unittest.pro'.

# get list of path names
tests = File.new('tests.txt').readlines.find_all { |i| i =~ /^[^#]/ }.collect { |i| i.chomp }

# this string is used to indicate the file is auto-generated
WARNING = "WARNING! All changes made in this file will be lost!"

# generate 'checkall' script
File::open('checkall', 'w') do |f|
  f << "#!/bin/bash\n"
  f << "# #{WARNING}\n"
  f << "basedir=`pwd`\n"
  f << "do_make() {\n"
  f << "  if test ! -e Makefile; then qmake; fi\n"
  f << "  make check\n"
  f << "}\n"
  f << tests.collect { |dir| "cd #{dir} && do_make && cd $basedir" }.join(" && \\\n") + "\n"
end

# generate project file
File::open('unittest.pro', 'w') do |f|
  f << "# #{WARNING}\n"
  f << "TEMPLATE = subdirs\n\n"
  f << "SUBDIRS += \\\n"
  f << tests.collect { |dir| "\t#{dir}" }.join(" \\\n") + "\n"
  f << "\n"
  f << "QMAKE_EXTRA_TARGETS += check\n"
  f << "check.commands = sh ./checkall\n"
end
