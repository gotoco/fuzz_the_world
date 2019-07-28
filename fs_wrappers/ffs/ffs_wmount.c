#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/param.h>

#include <dev/vndvar.h>

#include <disktab.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <paths.h>
#include <limits.h>

#define VND_CONFIG	1
#define VND_UNCONFIG	2
#define VND_GET		3


static int	verbose = 0;
static int	readonly = 0;
static int	fileio = 0;
static int	force = 0;
static int	compressed = 0;
static char	*tabname;

static int	show(int, int, const char * const);
static int	config(char *, char *, char *, int);
static int	getgeom(struct vndgeom *, char *);
static void	show_unused(int);
static int
run_config(int action, char *dev, char *fpath);


struct ufs_args {
	char      *fspec;             /* block special file to mount */
};

static
void usage() {
	fprintf(stderr, "Wrapper to mount a file using vndconfig and mount(2)");
	fprintf(stderr, "\n\n");
	fprintf(stderr, "Usage: wrapper [file path] ...vnd args\n");
}

static
int run_cycle(char *dev, char *fs_name, char *fpath)
{
	int fd;
	int ch, rv, action;
	int mntflags = 0;
	char *errcause = NULL;

	/* Tepmorar stuff */
	char *buffer = "TEST WRITE~!\n";

	// mount stuff
	struct ufs_args ufs_args; 
	ufs_args.fspec = dev;

	// Config first
	action = VND_CONFIG;
	rv = run_config(action, dev, fpath);
	if (rv) {
		printf("#: VND_CONFIG failed: rv: %d\n", rv);
	}

	// Mount it
	if (mount(MOUNT_FFS, fs_name, mntflags, &ufs_args, 8) == -1) {
		switch (errno) {
		case EMFILE:
                     errcause = "mount table full";
                     break;
		case EINVAL: 
                     if (mntflags & MNT_UPDATE)
                             errcause = 
                         "specified device does not match mounted device";
                     else 
                             errcause = "incorrect super block";
                     break;
		default:
                     errcause = strerror(errno);
                     break;
		}
		printf("#: Mount failed: %s on %s: %s",
			ufs_args.fspec, fs_name, errcause);
		goto after_mount;
	}

	// Test it: Create file and write to it
	fd = open("/mnt/bc", O_RDWR | O_CREAT);
	if (fd == -1) {
		printf("#: Opening file /mnt/b failed!\n");
	} else {
		write(fd, buffer, strlen(buffer));
		close(fd);
	}

	// Umount it
	if (unmount(fs_name, 0) == -1) {
		printf("#: Umount failed!\n");
	}

// If mount failed also
after_mount:
	//Unconfigure
	action = VND_UNCONFIG;
	rv = run_config(action, dev, fpath);
	if (rv) {
		printf("#: VND_UNCONFIG failed: rv: %d\n", rv);
	}
}

int
main(int argc, char *argv[])
{
	char *dev = "/dev/vnd0\0";
	char *fs_name = "/mnt\0";
	char *buffer = "TEST WRITE~!\n";

        if (argc < 2) {
		fprintf(stderr, "Missing input file to mount.\n");
		usage();
        }

	// Argument is a file that we will vnconfig to the vnd0
	if (argc >= 2)
	{
		// char *fpath = "/tmp/rand.tmp";
		char * fpath = argv[1];

		return run_cycle(dev, fs_name, fpath);
	}

	return -1;
}

/* To be run from .so object */
void load_hook(unsigned int argc, char**argv)
{
        return;
}

/* To be run from .so object */
int run(unsigned int argc, char** argv)
{
	char *dev = "/dev/vnd0\0";
	char *fs_name = "/mnt\0";
	char *buffer = "TEST WRITE~!\n";

        if (argc < 2) {
		fprintf(stderr, "Missing input file to mount.\n");
		usage();
        }

        if (argc >= 2)
        { 
		// char *fpath = "/tmp/rand.tmp";
		char * fpath = argv[1];

		return run_cycle(dev, fs_name, fpath);
        }

        return 0;
}

/**
 * Just a simple wrapper around :
 * ``config(char *dev, char *file, char *geom, int action)``
 *
 * Geometry currenty not supported
 */
static int
run_config(int action, char *dev, char *fpath)
{
	int rv = -1;

	if (action == VND_CONFIG) {
		rv = config(dev, fpath, NULL, action);
	} else if (action == VND_UNCONFIG) {
		rv = config(dev, NULL, NULL, action);
	} else {
		printf("Unsupported action!\n");
	}

	return rv;
}

static void
show_unused(int n)
{
	printf("#: %d: not in use\n", n);
}

static int
show(int v, int n, const char * const name)
{
	struct vnd_user vnu;
	char *dev;
	struct statvfs *mnt;
	int i, nmount;

	vnu.vnu_unit = n;
	if (ioctl(v, VNDIOCGET, &vnu) == -1) {
		if (errno != ENXIO) {
			if (n != -1)
				err(1, "VNDIOCGET");
			warn("%s: VNDIOCGET", name);
		}
		return 0;
	}

	if (vnu.vnu_ino == 0) {
		show_unused(vnu.vnu_unit);
		return -1;
	}

	printf("vnd%d: ", vnu.vnu_unit);

	dev = devname(vnu.vnu_dev, S_IFBLK);
	if (dev != NULL)
		nmount = getmntinfo(&mnt, MNT_NOWAIT);
	else {
		mnt = NULL;
		nmount = 0;
	}

	if (mnt != NULL) {
		for (i = 0; i < nmount; i++) {
			if (strncmp(mnt[i].f_mntfromname, "/dev/", 5) == 0 &&
			    strcmp(mnt[i].f_mntfromname + 5, dev) == 0)
				break;
		}
		if (i < nmount)
			printf("%s (%s) ", mnt[i].f_mntonname,
			    mnt[i].f_mntfromname);
		else
			printf("%s ", dev);
	}
	else if (dev != NULL)
		printf("%s ", dev);
	else
		printf("dev %llu,%llu ",
		    (unsigned long long)major(vnu.vnu_dev),
		    (unsigned long long)minor(vnu.vnu_dev));

	printf("inode %llu\n", (unsigned long long)vnu.vnu_ino);
	return 1;
}

static int
config(char *dev, char *file, char *geom, int action)
{
	struct vnd_ioctl vndio;
	struct disklabel *lp;
	char rdev[MAXPATHLEN + 1];
	int fd, rv;

	fd = opendisk(dev, O_RDWR, rdev, sizeof(rdev), 0);
	if (fd < 0) {
		warn("%s: opendisk", rdev);
		return (1);
	}

	memset(&vndio, 0, sizeof(vndio));
#ifdef __GNUC__
	rv = 0;			/* XXX */
#endif

	vndio.vnd_file = file;
	if (geom != NULL) {
		rv = getgeom(&vndio.vnd_geom, geom);
		if (rv != 0)
			errx(1, "invalid geometry: %s", geom);
		vndio.vnd_flags = VNDIOF_HASGEOM;
	} else if (tabname != NULL) {
		lp = getdiskbyname(tabname);
		if (lp == NULL)
			errx(1, "unknown disk type: %s", tabname);
		vndio.vnd_geom.vng_secsize = lp->d_secsize;
		vndio.vnd_geom.vng_nsectors = lp->d_nsectors;
		vndio.vnd_geom.vng_ntracks = lp->d_ntracks;
		vndio.vnd_geom.vng_ncylinders = lp->d_ncylinders;
		vndio.vnd_flags = VNDIOF_HASGEOM;
	}

	if (readonly)
		vndio.vnd_flags |= VNDIOF_READONLY;

	if (compressed)
		vndio.vnd_flags |= VNF_COMP;

	if (fileio)
		vndio.vnd_flags |= VNDIOF_FILEIO;

	/*
	 * Clear (un-configure) the device
	 */
	if (action == VND_UNCONFIG) {
		if (force)
			vndio.vnd_flags |= VNDIOF_FORCE;
		rv = ioctl(fd, VNDIOCCLR, &vndio);
#ifdef VNDIOOCCLR
		if (rv && errno == ENOTTY)
			rv = ioctl(fd, VNDIOOCCLR, &vndio);
#endif
		if (rv)
			warn("%s: VNDIOCCLR", rdev);
		else if (verbose)
			printf("%s: cleared\n", rdev);
	}
	/*
	 * Configure the device
	 */
	if (action == VND_CONFIG) {
		int	ffd;

		ffd = open(file, readonly ? O_RDONLY : O_RDWR);
		if (ffd < 0) {
			warn("%s", file);
			rv = -1;
		} else {
			(void) close(ffd);

			rv = ioctl(fd, VNDIOCSET, &vndio);
#ifdef VNDIOOCSET
			if (rv && errno == ENOTTY) {
				rv = ioctl(fd, VNDIOOCSET, &vndio);
				vndio.vnd_size = vndio.vnd_osize;
			}
#endif
			if (rv)
				warn("%s: VNDIOCSET", rdev);
			else if (verbose) {
				printf("%s: %" PRIu64 " bytes on %s", rdev,
				    vndio.vnd_size, file);
				if (vndio.vnd_flags & VNDIOF_HASGEOM)
					printf(" using geometry %d/%d/%d/%d",
					    vndio.vnd_geom.vng_secsize,
					    vndio.vnd_geom.vng_nsectors,
					    vndio.vnd_geom.vng_ntracks,
				    vndio.vnd_geom.vng_ncylinders);
				printf("\n");
			}
		}
	}

	(void) close(fd);
	fflush(stdout);
	return (rv < 0);
}

static int
getgeom(struct vndgeom *vng, char *cp)
{
	char *secsize, *nsectors, *ntracks, *ncylinders;

#define	GETARG(arg) \
	do { \
		if (cp == NULL || *cp == '\0') \
			return (1); \
		arg = strsep(&cp, "/"); \
		if (arg == NULL) \
			return (1); \
	} while (0)

	GETARG(secsize);
	GETARG(nsectors);
	GETARG(ntracks);
	GETARG(ncylinders);

#undef GETARG

	/* Too many? */
	if (cp != NULL)
		return (1);

#define	CVTARG(str, num) \
	do { \
		num = strtol(str, &cp, 10); \
		if (*cp != '\0') \
			return (1); \
	} while (0)

	CVTARG(secsize, vng->vng_secsize);
	CVTARG(nsectors, vng->vng_nsectors);
	CVTARG(ntracks, vng->vng_ntracks);
	CVTARG(ncylinders, vng->vng_ncylinders);

#undef CVTARG

	return (0);
}

