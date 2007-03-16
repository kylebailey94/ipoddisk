/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

#include "ipoddisk.h"

#define IPODDISK_MAX_IPOD       16
int ipodnr;
struct ipoddisk_node *ipods[IPODDISK_MAX_IPOD];
static struct ipoddisk_node *ipoddisk_tree;
static GError *error = NULL;


int
ipoddisk_statipods (struct statvfs *stbuf)
{
        struct statvfs tmp;
        int i;

        if (statvfs(ipods[0]->nd_data.ipod.ipod_mp, stbuf) == -1)
                return -errno;

        for (i = 1; i < ipodnr; i++) {
                if (statvfs(ipods[i]->nd_data.ipod.ipod_mp, &tmp) == -1)
                        return -errno;
                /* FIXME: this assumes that block sizes of all filesystems
                 * are the same */
                stbuf->f_blocks += tmp.f_blocks;
                stbuf->f_bavail += tmp.f_bavail;
                stbuf->f_bfree += tmp.f_bfree;
        }

        stbuf->f_flag = ST_RDONLY | ST_NOSUID;

        return 0;
}

#define IPODDISK_MAX_PATH_TOKENS	12
struct ipoddisk_node *
ipod_disk_parse_path(const char *path, int len)
{
	gchar **tokens, **orig;
	struct ipoddisk_node *node;
	struct ipoddisk_node *parent;

	UNUSED(len);

	tokens = g_strsplit(path, "/", IPODDISK_MAX_PATH_TOKENS);
	orig = tokens;
	parent = ipoddisk_tree;
	node = parent;
	while (*tokens) {
		gchar *token = *tokens;
		tokens++;
		if (strlen(token)) {
			if (parent->nd_type == IPOD_DISK_NODE_LEAF) {
				node = NULL;
				break;
			}
			node = g_datalist_get_data(&parent->nd_children, token);
			if (node == NULL)
				break;
			parent = node;
		}
	}
	g_strfreev(orig);
	return node;
}
#undef IPODDISK_MAX_PATH_TOKENS


#define IPODDISK_NEW_NODE()	g_slice_new(struct ipoddisk_node)

/* FIXME: assume track->ipod_path has an extension 
   of 4 char, e.g. '.mp3', '.m4a'. */
#define IPOD_TRACK_EXTENSION_LEN	4

static inline gchar *
ipod_get_track_extension (gchar * path)
{
	gchar *ext;

        assert (strlen(path) > IPOD_TRACK_EXTENSION_LEN);

	ext = path + strlen(path) - IPOD_TRACK_EXTENSION_LEN;
	return ext;
}

static void
ipod_encode_name(gchar ** strpp)
{
	int i, len;
	gchar *old = *strpp;

	/* encode path names to appease Finder:
	   0. leading . are treated as hidden file, encode as _
	   1. slash is Unix path separator, encode as : 
	   2. \r and \n are problematic, encode as space

	   then normalize the string to cope with tricky
	   things like umlaut */
	if (old) {
		gchar *nstr;			/* normalized utf-8 string */

		len = strlen(old);
		for (i = 0; i < len; i++) {
			if ((i == 0) && (old[i] == '.'))
				old[i] = '_';
			if (old[i] == '/')
				old[i] = ':';
			if ((old[i] == '\r') || (old[i] == '\n'))
				old[i] = ' ';
		}

		nstr = g_utf8_normalize(old, len, G_NORMALIZE_NFD);
		if (nstr) {
			g_free(old);
			*strpp = nstr;
		}
	}
}

/**
 * Adds a track into a tree structure
 * @param itdbtrk Pointer to the track's Itdb_Track structure
 * @param start Pointer to the root of the tree
 * @param track If not NULL, pointer to the node of the track
 * @return Pointer to the parent node of the track
 */
static struct ipoddisk_node *
__ipod_add_track(Itdb_Track *itdbtrk, struct ipoddisk_node *start,
                 struct ipoddisk_node *track, struct ipoddisk_ipod *ipod)
{
	struct ipoddisk_node *artist;
	struct ipoddisk_node *album;
	gchar *artist_name, *album_name, *track_name, *track_ext;

	artist_name = itdbtrk->artist ? itdbtrk->artist : "Unknown Artist";
	album_name = itdbtrk->album ? itdbtrk->album : "Unknown Album";
	track_name = itdbtrk->title ? itdbtrk->title : "Unknown Track";

	artist = g_datalist_get_data(&start->nd_children, artist_name);
	if (!artist) {
		artist = IPODDISK_NEW_NODE();
		artist->nd_name = artist_name;
		artist->nd_type = IPOD_DISK_NODE_DEFAULT;
		g_datalist_init(&artist->nd_children);
		g_datalist_set_data(&start->nd_children, artist->nd_name, artist);
	}
	album = g_datalist_get_data(&artist->nd_children, album_name);
	if (!album) {
		album = IPODDISK_NEW_NODE();
		album->nd_name = album_name;
		album->nd_type = IPOD_DISK_NODE_DEFAULT;
		g_datalist_init(&album->nd_children);
		g_datalist_set_data(&artist->nd_children, album->nd_name, album);
	}
	/* when track is not NULL, duplicate tracks already taken care 
	   of in the 1st run of this function, so no need to check for 
	   dups again. */
	if (!track) {
		int                   dup_cnt = 0;
		gchar                *dup_str = NULL;
		struct ipoddisk_node *dup_trk;

                assert(ipod != NULL);

		track_ext = ipod_get_track_extension(itdbtrk->ipod_path);
		track_name = g_strconcat(track_name, track_ext, NULL);
		dup_trk = g_datalist_get_data(&album->nd_children, track_name);
		while (dup_trk) {
			dup_cnt++;
			g_free(track_name);
			track_name = itdbtrk->title ? itdbtrk->title : "Unknown Track";
			dup_str = g_strdup_printf("(%d)", dup_cnt);
			track_name = g_strconcat(track_name, dup_str, track_ext, NULL);
			g_free(dup_str);
			dup_trk = g_datalist_get_data(&album->nd_children, track_name);
		}
		track = IPODDISK_NEW_NODE();
		track->nd_name = track_name;
		track->nd_children = (GData *) itdbtrk;
		track->nd_type = IPOD_DISK_NODE_LEAF;
                track->nd_data.track.trk_ipod = ipod;
		itdbtrk->userdata = track;
	}
	g_datalist_set_data(&album->nd_children, track->nd_name, track);

	return album;
}

static void
ipod_add_track(gpointer data, gpointer user_data)
{
	struct ipoddisk_node **nodes = (struct ipoddisk_node **) user_data;
	struct ipoddisk_node *artists = nodes[0];
	struct ipoddisk_node *genres = nodes[1];
	struct ipoddisk_node *albums = nodes[2];
	struct ipoddisk_node *ipod = nodes[3];
	struct ipoddisk_node *genre;
	struct ipoddisk_node *album;
	Itdb_Track *itdbtrk = (Itdb_Track *) data;

	if (itdbtrk->artist)
		ipod_encode_name(&itdbtrk->artist);
	if (itdbtrk->album)
		ipod_encode_name(&itdbtrk->album);
	if (itdbtrk->title)
		ipod_encode_name(&itdbtrk->title);
	if (itdbtrk->genre)
		ipod_encode_name(&itdbtrk->genre);

	album = __ipod_add_track(itdbtrk, artists, NULL, &ipod->nd_data.ipod);
	/* FIXME: if there're more than one album with a same name, only
	   the last one added is accessible */
	g_datalist_set_data(&albums->nd_children, album->nd_name, album);

	if ((itdbtrk->genre != NULL) && (strlen(itdbtrk->genre))) {
		genre = g_datalist_get_data(&genres->nd_children, itdbtrk->genre);
		if (!genre) {
			genre = IPODDISK_NEW_NODE();
			genre->nd_name = itdbtrk->genre;
			genre->nd_type = IPOD_DISK_NODE_DEFAULT;
			g_datalist_init(&genre->nd_children);
			g_datalist_set_data(&genres->nd_children, genre->nd_name, genre);
		}
		__ipod_add_track(itdbtrk, genre,
				 (struct ipoddisk_node *) itdbtrk->userdata, NULL);
	}

	return;
}

static void
ipod_add_playlist_member(gpointer data, gpointer user_data)
{
	struct __add_playlist_member_arg *argp =
		(struct __add_playlist_member_arg *) user_data;
	Itdb_Track *itdbtrk = (Itdb_Track *) data;
	struct ipoddisk_node *pl = argp->playlist;
	struct ipoddisk_node *track;
	gchar *track_ext;
	gchar *track_name = itdbtrk->title ? itdbtrk->title : "Unknown Track";

	/* can't reuse ipoddisk_node in itdbtrk->userdata, because
	   here a prefix is used in nd_name of every ipoddisk_node */
	track = IPODDISK_NEW_NODE();
	track_ext = ipod_get_track_extension(itdbtrk->ipod_path);
    if (argp->format) {
        gchar *prefix;

        prefix = g_strdup_printf(argp->format, argp->nr);
        track->nd_name = g_strconcat(prefix, track_name, track_ext, NULL);
        g_free(prefix);
    } else {
        track->nd_name = g_strconcat(track_name, track_ext, NULL);
    }
	track->nd_children = (GData *) itdbtrk;
	track->nd_type = IPOD_DISK_NODE_LEAF;
        track->nd_data.track.trk_ipod = argp->ipod;
	g_datalist_set_data(&pl->nd_children, track->nd_name, track);
	argp->nr++;

	return;
}

static void
ipod_add_playlist(gpointer data, gpointer user_data)
{
	struct ipoddisk_node **nodes = (struct ipoddisk_node **) user_data;
	struct ipoddisk_node *start = nodes[0];
	struct ipoddisk_node *ipod = nodes[1];
	struct ipoddisk_node *playlist;
	gchar *pl_name;
	Itdb_Playlist *pl = (Itdb_Playlist *) data;

	if (itdb_playlist_is_mpl(pl))
		return;

	if (pl->name)
		ipod_encode_name(&pl->name);

	pl_name = pl->name ? pl->name : "Unknown Playlist";

	playlist = g_datalist_get_data(&start->nd_children, pl_name);
	if (!playlist) {
		playlist = IPODDISK_NEW_NODE();
		playlist->nd_name = pl_name;
		playlist->nd_type = IPOD_DISK_NODE_DEFAULT;
		g_datalist_init(&playlist->nd_children);
		g_datalist_set_data(&start->nd_children, playlist->nd_name, playlist);
	}

	{
		struct __add_playlist_member_arg arg;
		/* TODO: check why pl->num is incorrect */
		guint cnt = g_list_length(pl->members);

		if (cnt < 10) {
			arg.format = "%d. ";
            if (cnt == 1)
                arg.format = NULL;
		} else if (cnt < 100) {
			arg.format = "%.2d. ";
		} else if (cnt < 1000) {
			arg.format = "%.3d. ";
		} else {
			arg.format = "%.4d. ";
		}
		arg.nr = 1;
		arg.playlist = playlist;
                arg.ipod     = &ipod->nd_data.ipod;
		/* FIXME: the order of tracks in the 'members' list is different than
		   the order in iPod's menu if the playlist is "Recently Played".
		 */
		g_list_foreach(pl->members, ipod_add_playlist_member, &arg);
	}

	return;
}

void
ipoddisk_build_ipod_node (struct ipoddisk_node *root, Itdb_iTunesDB *itdb)
{
        struct ipoddisk_node *genres;
        struct ipoddisk_node *albums;
        struct ipoddisk_node *artists;
        struct ipoddisk_node *nodes[4];
        struct ipoddisk_node *playlists;

        artists = IPODDISK_NEW_NODE();
        g_datalist_init(&artists->nd_children);
        artists->nd_name = "Artists";
        artists->nd_type = IPOD_DISK_NODE_DEFAULT;

        playlists = IPODDISK_NEW_NODE();
        g_datalist_init(&playlists->nd_children);
        playlists->nd_name = "Playlists";
        playlists->nd_type = IPOD_DISK_NODE_DEFAULT;

        genres = IPODDISK_NEW_NODE();
        g_datalist_init(&genres->nd_children);
        genres->nd_name = "Genres";
        genres->nd_type = IPOD_DISK_NODE_DEFAULT;

        albums = IPODDISK_NEW_NODE();
        g_datalist_init(&albums->nd_children);
        albums->nd_name = "Albums";
        albums->nd_type = IPOD_DISK_NODE_DEFAULT;

        g_datalist_set_data(&root->nd_children, artists->nd_name, artists);
        g_datalist_set_data(&root->nd_children, playlists->nd_name, playlists);
        g_datalist_set_data(&root->nd_children, genres->nd_name, genres);
        g_datalist_set_data(&root->nd_children, albums->nd_name, albums);

        /* must call ipod_add_track() before ipod_add_playlist()
           because strings in the_itdb->tracks are encoded in the former.
         */
        nodes[0] = artists;
        nodes[1] = genres;
        nodes[2] = albums;
        nodes[3] = root;
        /* populate iPodDisk/(Artists|Albums|Genres) */
        g_list_foreach(itdb->tracks, ipod_add_track, nodes);
        /* populate iPodDisk/Playlists */
        nodes[0] = playlists;
        nodes[1] = root;
        g_list_foreach(itdb->playlists, ipod_add_playlist, nodes);

        return;
}

gchar *
ipoddisk_node_path (struct ipoddisk_node *node)
{
        gchar      *rpath; /* relative path */
        gchar      *apath; /* absolute path */
        Itdb_Track *track;

        assert(node->nd_type == IPOD_DISK_NODE_LEAF);

        track = (Itdb_Track *) node->nd_children;
        rpath = g_strdup(track->ipod_path);
        itdb_filename_ipod2fs(rpath);

        assert (*rpath == '/');

	apath = g_strconcat(node->nd_data.track.trk_ipod->ipod_mp, rpath, NULL);

        g_free(rpath);
        return apath;
}

struct ipoddisk_node *
ipoddisk_init_one_ipod (gchar *dbfile)
{
        struct ipoddisk_node *node;
        Itdb_iTunesDB        *the_itdb;

        the_itdb = itdb_parse_file(dbfile, &error);
	if (error != NULL) {
                fprintf(stderr,
                        "itdb_parse_file() failed: %s!\n",
                        error->message);
		g_error_free(error);
		error = NULL;
                return NULL;
	}

	if (the_itdb == NULL)
		return NULL;

        node = IPODDISK_NEW_NODE();

        node->nd_type                = IPOD_DISK_NODE_IPOD;
        node->nd_data.ipod.ipod_itdb = the_itdb;
        g_datalist_init(&node->nd_children);

        ipoddisk_build_ipod_node(node, the_itdb);

        open(dbfile, O_RDONLY); /* leave me not, babe */

	return node;
}

int
ipoddisk_init_ipods (void)
{
	int                  i;
        int                  fsnr;
	struct statfs        *stats = NULL;

	fsnr = getfsstat(NULL, 0, MNT_NOWAIT);
	if (fsnr <= 0)
		return ENOENT;

	stats = g_malloc0(fsnr * sizeof(struct statfs));
        if (stats == NULL)
                return ENOMEM;

	fsnr = getfsstat(stats, fsnr * sizeof(struct statfs), MNT_NOWAIT);
	if (fsnr <= 0) {
		g_free(stats);
		return ENOENT;
        }

        for (i = 0, ipodnr = 0; i < fsnr && ipodnr < IPODDISK_MAX_IPOD; i++) {
                gchar                *dbpath;
                struct ipoddisk_node *node;

                if (strncasecmp(stats[i].f_mntfromname,
                                CONST_STR_LEN("/dev/disk")))
                        continue;  /* fs not disk-based */

                if (!strcmp(stats[i].f_mntonname, "/"))
                        continue;  /* skip root fs */

                dbpath = g_strconcat(stats[i].f_mntonname,
                                     "/iPod_Control/iTunes/iTunesDB", NULL);

                if (strlen(dbpath) >= MAXPATHLEN ||
                    !g_file_test(dbpath, G_FILE_TEST_EXISTS) ||
                    !g_file_test(dbpath, G_FILE_TEST_IS_REGULAR)) {
                        g_free(dbpath);
                        continue;
                }

                node = ipoddisk_init_one_ipod(dbpath);
                if (node == NULL) {
                        g_free(dbpath);
                        continue;
                }

                node->nd_name = g_path_get_basename(stats[i].f_mntonname);
                node->nd_data.ipod.ipod_mp = g_strdup(stats[i].f_mntonname);

                ipods[ipodnr] = node;
                ipodnr++;
                
                g_free(dbpath);
        }

        g_free(stats);

        if (ipodnr == 0)
                return ENOENT;

        if (ipodnr == 1) {
                ipoddisk_tree = ipods[0];
                ipoddisk_tree->nd_type = IPOD_DISK_NODE_ROOT;
                return 0;
        }

        ipoddisk_tree = IPODDISK_NEW_NODE();
        g_datalist_init(&ipoddisk_tree->nd_children);
        ipoddisk_tree->nd_name = "*Mount Point*";
        ipoddisk_tree->nd_type = IPOD_DISK_NODE_ROOT;

        for (i = 0; i < ipodnr; i++)
                g_datalist_set_data(&ipoddisk_tree->nd_children,
                                    ipods[i]->nd_name, ipods[i]);

        return 0;
}

void
ipod_free(void)
{
	/* no need to clean up:
           if (the_itdb)
                   itdb_free(the_itdb);
	   if (artists)
	   g_datalist_clear(&artists);
	   if (playlists)
	   g_datalist_clear(&playlists);
	 */

	/* TODO: clear datalists within artists and playlists */

	return;
}
