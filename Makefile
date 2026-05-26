SHELL = /bin/bash -O globstar

.PHONY: all
all:

.PHONY: clean
clean:
	rm -f   ./**/?*~   ./**/.?*~   ./**/\#?*\#   ./**/.\#?*
	rm -rf  bin
	rm -rf ./**/build
	rm -f  ./test_package/CMakeUserPresets.json
	rm -f  ./**/?*.el[cn]
	rm -f  ./**/?*.{so,dylib,dll}

%/:
	mkdir -p $@
	-chmod -R a+rwx $@

FORCE:

git-push: FORCE
	git pull
	git add .
	GIT_EDITOR=emacs git commit -v
	git push
