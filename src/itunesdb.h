/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
#ifndef IPODDISK_ITDB_H
#define IPODDISK_ITDB_H

#ifndef MADE_WITHOUT_COMPROMISE
#define MADE_WITHOUT_COMPROMISE
#endif

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <gpod/itdb.h>

#define UNUSED(x) ( (void)(x) )
#define CONST_STR_LEN(x) x, x ? sizeof(x) - 1 : 0

/* Types of node */
typedef enum {
	IPOD_DISK_NODE_ROOT,
	IPOD_DISK_NODE_DEFAULT,
	IPOD_DISK_NODE_LEAF
} ipoddisk_node_type;

struct ipoddisk_node {
	gchar *nd_name;
	GData *nd_children;
	ipoddisk_node_type nd_type;
};

struct __add_playlist_member_arg {
	int nr;
	gchar const *format;
	struct ipoddisk_node *playlist;
};

extern gchar *mount_point;

int itdb_init(void);
void ipod_free(void);
struct ipoddisk_node *get_ipoddisk_tree(void);
struct ipoddisk_node *ipod_disk_parse_path(const char *path, int len);
gchar *ipoddisk_node_path(struct ipoddisk_node *node);


#endif /* IPODDISK_ITDB_H */
