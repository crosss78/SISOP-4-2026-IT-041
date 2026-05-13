#define FUSE_USE_VERSION 28

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>

#define XOR_KEY 0x76
char STORAGE_DIR[1024];

static void xor_buffer(char *buf, size_t size)
{
    for (size_t i = 0; i < size; i++)
        buf[i] ^= XOR_KEY;
}

static void fullpath(char fpath[1024], const char *path)
{
    strcpy(fpath, STORAGE_DIR);
    strcat(fpath, path);
}

static void encpath(char fpath[1024], const char *path)
{
    strcpy(fpath, STORAGE_DIR);

    if (strcmp(path, "/") != 0)
        strcat(fpath, path);

    struct stat st;

    if (stat(fpath, &st) == -1) {

        if (strcmp(path, "/") != 0)
            strcat(fpath, ".enc");
    }
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
    int res;
    char fpath[1024];

    memset(stbuf, 0, sizeof(struct stat));

    encpath(fpath, path);

    res = lstat(fpath, stbuf);

    if (res == -1) {

        fullpath(fpath, path);

        res = lstat(fpath, stbuf);

        if (res == -1)
            return -errno;
    }

    return 0;
}

static int xmp_access(const char *path, int mask)
{
    char fpath[1024];

    encpath(fpath, path);

    if (access(fpath, mask) == -1) {

        fullpath(fpath, path);

        if (access(fpath, mask) == -1)
            return -errno;
    }

    return 0;
}

static int xmp_readdir(const char *path,
                       void *buf,
                       fuse_fill_dir_t filler,
                       off_t offset,
                       struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
    char fpath[1024];

    (void) offset;
    (void) fi;

    fullpath(fpath, path);

    dp = opendir(fpath);

    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {

        struct stat st;

        memset(&st, 0, sizeof(st));

        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char name[512];
        strcpy(name, de->d_name);

        if (strstr(name, ".enc"))
            name[strlen(name) - 4] = '\0';

        if (filler(buf, name, &st, 0))
            break;
    }

    closedir(dp);

    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
    char fpath[1024];

    fullpath(fpath, path);

    if (mkdir(fpath, mode) == -1)
        return -errno;

    return 0;
}

static int xmp_rmdir(const char *path)
{
    char fpath[1024];

    fullpath(fpath, path);

    if (rmdir(fpath) == -1)
        return -errno;

    return 0;
}

static int xmp_create(const char *path,
                      mode_t mode,
                      struct fuse_file_info *fi)
{
    char fpath[1024];

    (void) fi;

    encpath(fpath, path);

    int fd = creat(fpath, mode);

    if (fd == -1)
        return -errno;

    close(fd);

    return 0;
}

static int xmp_open(const char *path,
                    struct fuse_file_info *fi)
{
    char fpath[1024];

    encpath(fpath, path);

    int fd = open(fpath, fi->flags);

    if (fd == -1)
        return -errno;

    close(fd);

    return 0;
}

static int xmp_read(const char *path,
                    char *buf,
                    size_t size,
                    off_t offset,
                    struct fuse_file_info *fi)
{
    char fpath[1024];

    (void) fi;

    encpath(fpath, path);

    int fd = open(fpath, O_RDONLY);

    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);

    if (res == -1)
        res = -errno;
    else
        xor_buffer(buf, res);

    close(fd);

    return res;
}

static int xmp_write(const char *path,
                     const char *buf,
                     size_t size,
                     off_t offset,
                     struct fuse_file_info *fi)
{
    char fpath[1024];

    (void) fi;

    encpath(fpath, path);

    int fd = open(fpath, O_WRONLY);

    if (fd == -1)
        return -errno;

    char *tmp = malloc(size);

    memcpy(tmp, buf, size);

    xor_buffer(tmp, size);

    int res = pwrite(fd, tmp, size, offset);

    free(tmp);

    if (res == -1)
        res = -errno;

    close(fd);

    return res;
}

static int xmp_truncate(const char *path, off_t size)
{
    char fpath[1024];

    encpath(fpath, path);

    if (truncate(fpath, size) == -1)
        return -errno;

    return 0;
}

static int xmp_unlink(const char *path)
{
    char fpath[1024];

    encpath(fpath, path);

    if (unlink(fpath) == -1)
        return -errno;

    return 0;
}

static int xmp_utimens(const char *path,
                       const struct timespec ts[2])
{
    char fpath[1024];

    encpath(fpath, path);

    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;

    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    if (utimes(fpath, tv) == -1)
        return -errno;

    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .access = xmp_access,
    .readdir = xmp_readdir,
    .mkdir = xmp_mkdir,
    .rmdir = xmp_rmdir,
    .create = xmp_create,
    .open = xmp_open,
    .read = xmp_read,
    .write = xmp_write,
    .truncate = xmp_truncate,
    .unlink = xmp_unlink,
    .utimens = xmp_utimens,
};

int main(int argc, char *argv[])
{
    umask(0);

    char cwd[1024];

    getcwd(cwd, sizeof(cwd));

    snprintf(STORAGE_DIR,sizeof(STORAGE_DIR),"%s/encrypted_storage",cwd);

    return fuse_main(argc, argv, &xmp_oper, NULL);
}