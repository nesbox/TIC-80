fennel.h: loadfennel.lua
	xxd -i $^ > $@

# A modified copy of Fennel that puts itself into package.loaded rather
# than simply returning module, since it won't be loaded properly with require.
loadfennel.lua: fennel.lua
	sed "s/return mod/package.loaded['fennel'] = mod/" $< > $@

# Assuming you have a checkout as a sibling dir to this repo.
fennel.lua: ../../../fennel/fennel.lua
	cp $< $@
