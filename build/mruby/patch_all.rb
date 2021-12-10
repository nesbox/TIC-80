#!/usr/bin/env ruby
USERNAME = 'Bob'
USEREMAIL = 'bob@invalid'

system 'git', 'config', '--global', 'user.name', USERNAME if `git config --global user.name`.chomp.empty?
system 'git', 'config', '--global', 'user.email', USEREMAIL if `git config --global user.email`.chomp.empty?

Dir[File.expand_path('patches/*.patch', File.dirname(__FILE__))].each do |patch|
  system "git am <#{patch}"
end
