/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

/* for pread(2) */
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <sys/xattr.h>

#include "ipoddisk.h"


static uid_t the_uid;
static gid_t the_gid;
static struct timeval the_time;

static int 
ipoddisk_statfs (const char *path, struct statvfs *stbuf)
{
        int rc;

        memset(stbuf, 0, sizeof(*stbuf));

        rc = (statvfs(mount_point, stbuf) == -1) ? -errno : 0;

        stbuf->f_flag = ST_RDONLY | ST_NOSUID;

        return rc;
}

static int 
ipoddisk_getattr (const char *path, struct stat *stbuf)
{
        int                   rc;
        struct ipoddisk_node *node;

        memset(stbuf, 0, sizeof(*stbuf));

        node = ipod_disk_parse_path(path, strlen(path));
        if (node == NULL)
                return -ENOENT;

        if (node->nd_type == IPOD_DISK_NODE_LEAF) {
                gchar *file = ipoddisk_node_path(node);

                rc = (lstat(file, stbuf) == -1) ? -errno : 0;

                stbuf->st_mode = S_IFREG |                    /* regular */
                                 S_IRUSR | S_IRGRP | S_IROTH; /* readable */

                g_free(file);
        } else {
                rc = 0;

                stbuf->st_mode = S_IFDIR |                     /* directory */
                                 S_IRUSR | S_IRGRP | S_IROTH | /* readable */
                                 S_IXUSR | S_IXGRP | S_IXOTH;  /* executable */

                stbuf->st_nlink = 2;
                stbuf->st_size  = 1024;
                stbuf->st_ino   = (ino_t) node;
                stbuf->st_atime = 
                stbuf->st_mtime =
                stbuf->st_ctime = the_time.tv_sec;
        }

        stbuf->st_uid = the_uid;
        stbuf->st_gid = the_gid;

        return rc;
}

static int 
ipoddisk_access (const char *path, int mask)
{
        struct ipoddisk_node *node;

        node = ipod_disk_parse_path(path, strlen(path));
        if (node == NULL)
                return -ENOENT;

        if (mask & W_OK)
                return -EROFS;

        if (node->nd_type == IPOD_DISK_NODE_LEAF &&
            (mask & X_OK))
                return -EACCES;

        return 0;
}

struct foo_arg {
        fuse_fill_dir_t filler;
        void *buf;
};

void
ipoddisk_add_node_prop(GQuark key_id, gpointer data, gpointer user_data)
{
	struct ipoddisk_node *node = (struct ipoddisk_node *) data;
        struct foo_arg *arg = user_data;


        (arg->filler)(arg->buf, node->nd_name, NULL, 0);
}

static int ipoddisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
        struct ipoddisk_node *node;
        struct foo_arg arg;

        UNUSED(fi);
        UNUSED(offset);

        node = ipod_disk_parse_path(path, strlen(path));
        if(node == NULL || node->nd_type == IPOD_DISK_NODE_LEAF)
                return -ENOENT;

        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        arg.filler = filler;
        arg.buf = buf;

        g_datalist_foreach(&node->nd_children,
                           ipoddisk_add_node_prop, &arg);

        return 0;
}

static int ipoddisk_open(const char *path, struct fuse_file_info *fi)
{
        struct ipoddisk_node *node;

        node = ipod_disk_parse_path(path, strlen(path));
        if (node == NULL)
                return -ENOENT;

        if((fi->flags & O_ACCMODE) != O_RDONLY)
                return -EACCES;

        return 0;
}

static int
ipoddisk_read (const char *path, char *buf, size_t size,
               off_t offset, struct fuse_file_info *fi)
{
        int                   fd;
        int                   rc;
        gchar                *file;
        struct ipoddisk_node *node;

        UNUSED (fi);

        node = ipod_disk_parse_path(path, strlen(path));
        if(node == NULL ||
           node->nd_type != IPOD_DISK_NODE_LEAF)
                return -ENOENT;

        file = ipoddisk_node_path(node);

        fd = open(file, O_RDONLY);
        if (fd == -1) {
                rc = -errno;
                g_free(file);
                return rc;
        }

        rc = pread(fd, buf, size, offset);
        if (rc == -1)
                rc = -errno;

        close(fd);
        g_free(file);

        return rc;
}

#if 0
static int
ipoddisk_getxattr (const char *path, const char *name, char *value, size_t size)
{
        int                   rc;
        gchar                *file;
        struct ipoddisk_node *node;

        node = ipod_disk_parse_path(path, strlen(path));
        if (node == NULL)
                return -ENOENT;

        if (node->nd_type != IPOD_DISK_NODE_LEAF)
                return -EPERM;

        file = ipoddisk_node_path(node);

        rc = (getxattr(file, name, value, size, 0, 0) == -1) ? -errno : 0;

        g_free(file);
        return rc;
}

static int 
ipoddisk_listxattr (const char *path, char *list, size_t size)
{
        int                   rc;
        gchar                *file;
        struct ipoddisk_node *node;

        node = ipod_disk_parse_path(path, strlen(path));
        if (node == NULL)
                return -ENOENT;

        if (node->nd_type != IPOD_DISK_NODE_LEAF)
                return -EPERM;

        file = ipoddisk_node_path(node);

        rc = (listxattr(file, list, size, 0) == -1) ? -errno : 0;

        g_free(file);
        return rc;
}
#endif

static struct fuse_operations ipoddisk_oper = {
        .statfs    = ipoddisk_statfs,
        .getattr   = ipoddisk_getattr,
        .access	   = ipoddisk_access,
        .readdir   = ipoddisk_readdir,
        .open      = ipoddisk_open,
        .read      = ipoddisk_read,
#if 0
        .getxattr  = ipoddisk_getxattr,
        .listxattr = ipoddisk_listxattr,
#endif
};

int main(int argc, char *argv[])
{
        the_uid = getuid();
        the_gid = getgid();

        if (gettimeofday(&the_time, NULL) != 0)
                perror("failed to get current time: ");

        if (itdb_init() != 0) {
                fprintf(stderr, "itdb_init() has failed.\n");
                return 1;
        }
        
        return fuse_main(argc, argv, &ipoddisk_oper, NULL);
}
