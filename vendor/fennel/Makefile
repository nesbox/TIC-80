fennel.h: fennel.lua
	xxd -i $^ > $@

# This is simpler than hacking package.searchers for one file.
fennel.lua: fennel_original.lua fennelfriend.lua
	sed "s/return module/package.loaded['fennel'] = module/" $< > $@
	echo "package.preload.fennelfriend = function()" >> $@
	cat fennelfriend.lua >> $@
	echo "end" >> $@

# Assuming you have a checkout as a sibling dir to this repo.
fennel_original.lua: ../../../fennel/fennel.lua
	cp $< $@

fennelfriend.lua: ../../../fennel/fennelfriend.lua
	cp $< $@

update: fennel_original.lua fennelfriend.lua

clean: ; rm fennel.lua fennel_original.lua fennelfriend.lua

.PHONY: update clean
