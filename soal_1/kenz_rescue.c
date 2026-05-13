#define _GNU_SOURCE
#define FUSE_USE_VERSION 28
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>

static char *g_source_root = NULL;

static void build_source_path(char *dst, size_t dstsz, const char *path)
{
    if (strcmp(path, "/") == 0) {
        snprintf(dst, dstsz, "%s", g_source_root);
    } else {
        snprintf(dst, dstsz, "%s%s", g_source_root, path);
    }
}

static int is_virtual_tujuan(const char *path)
{
    return strcmp(path, "/tujuan.txt") == 0;
}

static void append_buf(char **buf, size_t *len, size_t *cap, const char *data, size_t data_len)
{
    if (*len + data_len + 1 > *cap) {
        size_t new_cap = *cap ? *cap : 128;
        while (*len + data_len + 1 > new_cap) {
            new_cap *= 2;
        }
        char *tmp = realloc(*buf, new_cap);
        if (!tmp) {
            free(*buf);
            *buf = NULL;
            *len = *cap = 0;
            return;
        }
        *buf = tmp;
        *cap = new_cap;
    }
    memcpy(*buf + *len, data, data_len);
    *len += data_len;
    (*buf)[*len] = '\0';
}

static char *build_tujuan_content(size_t *out_len)
{
    const char *prefix = "Tujuan Mas Amba: ";
    char *out = NULL;
    size_t len = 0, cap = 0;

    append_buf(&out, &len, &cap, prefix, strlen(prefix));
    if (!out) return NULL;

    for (int i = 1; i <= 7; i++) {
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%d.txt", g_source_root, i);

        FILE *fp = fopen(file_path, "r");
        if (!fp) continue;

        char *line = NULL;
        size_t line_cap = 0;
        ssize_t nread;
        while ((nread = getline(&line, &line_cap, fp)) != -1) {
            char *p = strstr(line, "KOORD:");
            if (!p) continue;
            p += strlen("KOORD:");
            while (*p == ' ' || *p == '\t') p++;
            char *end = p + strcspn(p, "\r\n");
            append_buf(&out, &len, &cap, p, (size_t)(end - p));
            break;
        }
        free(line);
        fclose(fp);
    }

    append_buf(&out, &len, &cap, "\n", 1);
    if (out_len) *out_len = len;
    return out;
}

static int kenz_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    if (is_virtual_tujuan(path)) {
        size_t len = 0;
        char *content = build_tujuan_content(&len);
        if (!content) return -ENOMEM;
        free(content);

        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = (off_t)len;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_mtime = time(NULL);
        stbuf->st_atime = stbuf->st_mtime;
        stbuf->st_ctime = stbuf->st_mtime;
        return 0;
    }

    char fpath[PATH_MAX];
    build_source_path(fpath, sizeof(fpath), path);
    if (lstat(fpath, stbuf) == -1) return -errno;
    return 0;
}

static int kenz_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                        struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    char fpath[PATH_MAX];
    build_source_path(fpath, sizeof(fpath), path);

    DIR *dp = opendir(fpath);
    if (!dp) return -errno;

    if (filler(buf, ".", NULL, 0)) {
        closedir(dp);
        return 0;
    }
    if (filler(buf, "..", NULL, 0)) {
        closedir(dp);
        return 0;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0)) break;
    }
    closedir(dp);

    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        st.st_nlink = 1;
        if (filler(buf, "tujuan.txt", &st, 0)) return 0;
    }

    return 0;
}

static int kenz_open(const char *path, struct fuse_file_info *fi)
{
    if (is_virtual_tujuan(path)) {
        fi->fh = 0;
        return 0;
    }

    char fpath[PATH_MAX];
    build_source_path(fpath, sizeof(fpath), path);

    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    fi->fh = (uint64_t)fd;
    return 0;
}

static int kenz_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    if (is_virtual_tujuan(path)) {
        size_t content_len = 0;
        char *content = build_tujuan_content(&content_len);
        if (!content) return -ENOMEM;

        if ((size_t)offset < content_len) {
            if (offset + (off_t)size > (off_t)content_len) {
                size = content_len - (size_t)offset;
            }
            memcpy(buf, content + offset, size);
        } else {
            size = 0;
        }

        free(content);
        return (int)size;
    }

    int fd = (int)fi->fh;
    ssize_t res = pread(fd, buf, size, offset);
    if (res == -1) return -errno;
    return (int)res;
}

static int kenz_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;
    if (!is_virtual_tujuan(path) && fi->fh > 0) {
        close((int)fi->fh);
    }
    return 0;
}

static int kenz_access(const char *path, int mask)
{
    if (is_virtual_tujuan(path)) return 0;

    char fpath[PATH_MAX];
    build_source_path(fpath, sizeof(fpath), path);
    if (access(fpath, mask) == -1) return -errno;
    return 0;
}

static int setup_environment(void)
{
    struct stat st;

    // kalau sudah pernah dijalankan
    if (stat("mnt/tujuan.txt", &st) == 0) {
        fprintf(stderr, "command sudah dijalankan\n");
        return -1;
    }

    // pastikan source ada
    if (stat("amba_files", &st) != 0) {
        fprintf(stderr, "folder amba_files tidak ditemukan\n");
        return -1;
    }

    // buat mount point otomatis
    mkdir("mnt", 0755);

    return 0;
}

static struct fuse_operations kenz_oper = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open = kenz_open,
    .read = kenz_read,
    .release = kenz_release,
    .access = kenz_access,
};

int main(int argc, char *argv[])
{
    if (
        argc != 3 ||
        strcmp(argv[1], "amba_files") != 0 ||
        strcmp(argv[2], "mnt") != 0
    ) {
        fprintf(stderr,
            "hanya menerima perintah: ./kenz_rescue amba_files mnt\n");
        return 1;
    }

    if (setup_environment() != 0) {
        return 1;
    }

    g_source_root = realpath(argv[1], NULL);
    if (!g_source_root) {
        perror("realpath");
        return 1;
    }

    int fuse_argc = argc - 1;
    char **fuse_argv = calloc((size_t)fuse_argc + 1, sizeof(char *));
    if (!fuse_argv) {
        perror("calloc");
        free(g_source_root);
        return 1;
    }

    fuse_argv[0] = argv[0];
    fuse_argv[1] = argv[2];
    for (int i = 3; i < argc; i++) {
        fuse_argv[i - 1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &kenz_oper, NULL);

    free(fuse_argv);
    free(g_source_root);
    return ret;
}