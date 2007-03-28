/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
#ifndef __IPODDISK_H
#define __IPODDISK_H

#ifndef MADE_WITHOUT_COMPROMISE
#define MADE_WITHOUT_COMPROMISE
#endif

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <gpod/itdb.h>

#define UNUSED(x) ( (void)(x) )
#define CONST_STR_LEN(x) x, x ? sizeof(x) - 1 : 0

/* Types of node */
typedef enum {
	IPODDISK_NODE_ROOT,
	IPODDISK_NODE_IPOD,
	IPODDISK_NODE_DEFAULT,
	IPODDISK_NODE_LEAF
} ipoddisk_node_type;

struct ipoddisk_ipod {
        gchar         *ipod_mp; /* mount point */
        Itdb_iTunesDB *ipod_itdb;
};

struct ipoddisk_track {
        struct ipoddisk_ipod *trk_ipod;
};

struct ipoddisk_node {
	GData              *nd_children;
	ipoddisk_node_type  nd_type;
        union {
                struct ipoddisk_ipod  ipod;
                struct ipoddisk_track track;
        } nd_data;
};

struct __add_playlist_member_arg {
	int nr;
	gchar const *format;
	struct ipoddisk_node *playlist;
        struct ipoddisk_ipod *ipod;
};

extern gchar *mount_point;

int ipoddisk_init_ipods (void);
int ipoddisk_statipods (struct statvfs *stbuf);
struct ipoddisk_node *ipoddisk_parse_path (const char *path, int len);
gchar *ipoddisk_node_path (struct ipoddisk_node *node);


#endif /* __IPODDISK_H */
