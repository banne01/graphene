apps = lmbench gcc busybox lighttpd bash apache make

targets = pal pal_sec
clean-extra = clean-all

level = ../
include ../Makefile

manifest_rules = \
	-e 's:\$$(PAL):$(abspath $(PWD)/../pal_loader):g' \
	-e 's:\$$(PWD):$(PWD):g' \
	-e 's:\$$(BIN):$(subst .manifest,,$(notdir $@)):g' \
	-e 's:\$$(SHIMPATH):$(abspath ../../src/libsysdb.so):g' \
	-e 's:\$$(GLIBCDIR):$(abspath $(glibc_dir)):g' \
	$(extra_rules)

%.manifest: %.manifest.template
	rm -f $@ $@.backup
	sed $(manifest_rules) $< >$@
	chmod +x $@

build-all:
	for f in $(apps); do (cd $$f; make $(DEBUG) || true; cd ..); done

clean-all:
	for f in $(apps); do (cd $$f; make clean; cd ..); done
