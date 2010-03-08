#!/usr/bin/env ruby -wKU

require "config.rb"

PWD = File.expand_path(File.dirname(__FILE__))
PSI = PWD + "/psi.rb -reset"
Kernel.exec(PSI)
