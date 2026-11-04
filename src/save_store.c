#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "save_store.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(int, browser_read, (int backup, unsigned char *out, int cap), {
    return Module.paldoSaveStore.read(backup, out, cap);
});
EM_JS(int, browser_write, (const unsigned char *data, int size, int preserve), {
    return Module.paldoSaveStore.write(data, size, preserve);
});
#else
#ifdef _WIN32
#include <windows.h>
#include <io.h>
static bool wide(const char *p, wchar_t *w) {
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, -1, w, 4096) > 0;
}
static FILE *open_file(const char *p, const char *mode) {
    wchar_t path[4096], m[16];
    if (!wide(p, path)) { errno = EINVAL; return NULL; }
    int i = 0; do { m[i] = mode[i]; } while (mode[i++] && i < 15);
    return _wfopen(path, m);
}
static bool replace_file(const char *from, const char *to) {
    wchar_t f[4096], t[4096];
    if (!wide(from, f) || !wide(to, t)) return false;
    if (MoveFileExW(f, t, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    errno = GetLastError() == ERROR_DISK_FULL ? ENOSPC : EIO; return false;
}
#else
#include <unistd.h>
#include <fcntl.h>
#define open_file fopen
static bool replace_file(const char *from, const char *to) { return rename(from, to) == 0; }
#endif
static SaveResult file_error(void) {
    return errno == ENOSPC ? SAVE_FULL : errno == EACCES || errno == EPERM ? SAVE_UNAVAILABLE : SAVE_IO;
}
static bool path_for(const SaveStore *s, const char *name, char *out, size_t cap) {
    if (!s->directory[0]) return false;
    int n = snprintf(out, cap, "%s/%s", s->directory, name);
    return n > 0 && (size_t)n < cap;
}
static SaveResult read_file(const SaveStore *s, const char *name, unsigned char *b, size_t *size) {
    char path[2304];
    if (!path_for(s, name, path, sizeof path)) return SAVE_UNAVAILABLE;
    FILE *f = open_file(path, "rb");
    if (!f) return errno == ENOENT ? SAVE_MISSING : file_error();
    *size = fread(b, 1, SAVE_CAPACITY, f);
    bool failed = ferror(f) != 0;
    int extra = fgetc(f);
    if (fclose(f) != 0) failed = true;
    return failed ? SAVE_IO : extra != EOF ? SAVE_CORRUPT : SAVE_OK;
}
static SaveResult write_file(const SaveStore *s, const char *name, const unsigned char *b, size_t size) {
    char path[2304];
    if (!path_for(s, name, path, sizeof path)) return SAVE_UNAVAILABLE;
    FILE *f = open_file(path, "wb");
    if (!f) return file_error();
    SaveResult result = SAVE_OK;
    if (fwrite(b, 1, size, f) != size || fflush(f) != 0) result = file_error();
#ifdef _WIN32
    if (result == SAVE_OK && _commit(_fileno(f)) != 0) result = file_error();
#else
    if (result == SAVE_OK && fsync(fileno(f)) != 0) result = file_error();
#endif
    if (fclose(f) != 0 && result == SAVE_OK) result = file_error();
    return result;
}
static SaveResult replace(const SaveStore *s, const char *from, const char *to) {
    char f[2304], t[2304];
    if (!path_for(s, from, f, sizeof f) || !path_for(s, to, t, sizeof t)) return SAVE_UNAVAILABLE;
    if (!replace_file(f, t)) return file_error();
#ifndef _WIN32
    /* Best effort directory durability; the renamed valid file is already visible. */
    int fd = open(s->directory, O_RDONLY);
    if (fd >= 0) { (void)fsync(fd); close(fd); }
#endif
    return SAVE_OK;
}
#endif
void save_store_init(SaveStore *s, const char *directory) {
    memset(s, 0, sizeof *s);
    if (directory && strlen(directory) < sizeof s->directory)
        snprintf(s->directory, sizeof s->directory, "%s", directory);
}
static SaveResult read_slot(const SaveStore *s, bool backup, unsigned char *b, size_t *size) {
#ifdef __EMSCRIPTEN__
    (void)s;
    int n = browser_read(backup, b, SAVE_CAPACITY);
    if (n < 0) return (SaveResult)-n;
    *size = (size_t)n; return SAVE_OK;
#else
    return read_file(s, backup ? "journey-autosave.bak" : "journey-autosave.sav", b, size);
#endif
}
SaveResult save_store_load(const SaveStore *s, Game *game, bool *recovered) {
    unsigned char bytes[SAVE_CAPACITY]; size_t size = 0;
    *recovered = false;
    SaveResult primary = read_slot(s, false, bytes, &size);
    if (primary == SAVE_OK) primary = save_decode(game, bytes, size);
    if (primary == SAVE_OK) return SAVE_OK;
    SaveResult backup = read_slot(s, true, bytes, &size);
    if (backup == SAVE_OK) backup = save_decode(game, bytes, size);
    if (backup == SAVE_OK) { *recovered = true; return SAVE_OK; }
    return primary == SAVE_MISSING ? backup : primary;
}
SaveResult save_store_write(const SaveStore *s, const Game *game) {
    unsigned char bytes[SAVE_CAPACITY], old[SAVE_CAPACITY]; size_t size = 0, old_size = 0;
    SaveResult result = save_encode(game, bytes, &size);
    if (result != SAVE_OK) return result;
    SaveResult existing = read_slot(s, false, old, &old_size);
    if (existing == SAVE_IO || existing == SAVE_UNAVAILABLE || existing == SAVE_FULL) return existing;
    bool preserve = existing == SAVE_OK && save_check(old, old_size) == SAVE_OK;
#ifdef __EMSCRIPTEN__
    return (SaveResult)browser_write(bytes, (int)size, preserve);
#else
    result = write_file(s, "journey-autosave.tmp", bytes, size);
    if (result != SAVE_OK) return result;
    if (preserve) {
        result = write_file(s, "journey-autosave.bak.tmp", old, old_size);
        if (result != SAVE_OK) return result;
        result = replace(s, "journey-autosave.bak.tmp", "journey-autosave.bak");
        if (result != SAVE_OK) return result;
    }
    return replace(s, "journey-autosave.tmp", "journey-autosave.sav");
#endif
}
