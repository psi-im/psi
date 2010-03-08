#!/usr/bin/env ruby -wKU

require "config.rb"

PWD= File.expand_path(File.dirname(__FILE__))
PSIDATADIR = PWD + "/psi-data"
ENV["PSIDATADIR"] = PSIDATADIR
["/profiles/default/accounts.xml",
 "/profiles/default/options.xml"
].each do |f|
  file_name = "#{PSIDATADIR}#{f}"
  `git checkout "#{file_name}"`
end

exit if !ARGV.empty?
PSI = PWD + "/../../src/psi.app/Contents/MacOS/psi"
Kernel.exec(PSI)
