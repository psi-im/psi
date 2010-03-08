#!/usr/bin/env ruby -wKU

require "../config.rb"

PWD= File.expand_path(File.dirname(__FILE__))
CONFIG = PWD + "/prosody.cfg.lua"
DATA   = PWD + "/data"

ENV["PROSODY_CFGDIR"] = PWD
ENV["PROSODY_DATADIR"] = DATA
Dir.chdir(PROSODY_DIR)
Kernel.exec('./prosody')
