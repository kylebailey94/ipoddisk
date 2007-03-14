ub_flags=-O -arch i386 -arch ppc -isysroot /Developer/SDKs/MacOSX10.4u.sdk

ipoddisk: ipoddisk_fuse.c ipoddisk_ipod.c ipoddisk.h
	gcc ${ub_flags} -Wall `pkg-config --cflags --libs glib-2.0 gobject-2.0 libgpod-1.0 fuse` $+ -o $@

test: ipoddisk
	./ipoddisk -oping_diskarb,volname=iPodDisk,fsname=iPodDisk ../../.mnt

SetOpenWindow: SetOpenWindow.c
	gcc -framework CoreServices $+ -o $@
clean:
	-rm -f ipoddisk ipoddisk.o
