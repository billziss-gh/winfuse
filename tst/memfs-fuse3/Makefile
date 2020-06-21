usage:
	@echo "make linux" 1>&2
	@echo "" 1>&2
	@echo "   linux           Linux build" 1>&2
	@exit 2

linux: memfs-fuse3

memfs-fuse3: memfs-fuse3.cpp
	g++ $^ -o $@ -g -std=c++17 -DFUSE_USE_VERSION=32 -Wall `pkg-config fuse3 --cflags --libs`
