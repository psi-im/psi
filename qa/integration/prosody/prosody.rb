#!/usr/bin/env ruby -wKU

require "../integration.rb"

PWD= File.expand_path(File.dirname(__FILE__))
CONFIG = PWD + "/prosody.cfg.lua"
DATA   = PWD + "/data"

ENV["PROSODY_CFGDIR"] = PWD
Dir.chdir(PROSODY_DIR)
puts `./prosody`
Dir.chdir(PWD)
