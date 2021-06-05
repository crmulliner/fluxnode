#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <esp_system.h>
#include "sdkconfig.h"
#include "esp_spiffs.h"

#include <duktape.h>

#include "vfs_config.h"
#include "duk_helpers.h"

/*
 * based on: https://github.com/nkolban/duktape-esp32.git
 * license: Apache 2.0
 */

/* jsondoc
{
"class": "Platform",
"longtext": "
Documentation for the FileSystem API.

Most of the code is either directly copied from or based on: https://github.com/nkolban/duktape-esp32.git
under the Apache 2.0 license.
"
}
*/

/**
 * Convert a string to posix open() flags.
 * "r" - O_RDONLY
 * "w" - O_WRONLY
 */
static int stringToPosixFlags(const char *flags)
{
    int posixFlags = 0;
    if (strcmp(flags, "r") == 0)
    {
        posixFlags = O_RDONLY;
    }
    if (strcmp(flags, "r+") == 0)
    {
        posixFlags = O_RDWR;
    }
    if (strcmp(flags, "w") == 0)
    {
        posixFlags = O_WRONLY | O_TRUNC | O_CREAT;
    }
    if (strcmp(flags, "w+") == 0)
    {
        posixFlags = O_RDWR | O_TRUNC | O_CREAT;
    }
    if (strcmp(flags, "a") == 0)
    {
        posixFlags = O_WRONLY | O_APPEND | O_CREAT;
    }
    if (strcmp(flags, "a+") == 0)
    {
        posixFlags = O_RDWR | O_APPEND | O_CREAT;
    }
    return posixFlags;
}

/* jsondoc
{
"name": "write",
"args": [
{"name": "fd", "vtype": "uint", "text": "file descriptor"},
{"name": "buffer", "vtype": "Plain Buffer", "text": "data"},
{"name": "offset", "vtype": "uint", "text": "offset (optional)"},
{"name": "length", "vtype": "uint", "text": "length (optional)"}
],
"text": "Write to file descriptor.",
"return": "num bytes written",
"example": "
"
}
*/

/* jsondoc
{
"name": "write&nbsp;",
"args": [
{"name": "fd", "vtype": "uint", "text": "file descriptor"},
{"name": "string", "vtype": "string", "text": "data"}
],
"return": "num bytes written",
"text": "Write string to file descriptor.",
"example": "
"
}
*/
static duk_ret_t js_fs_write(duk_context *ctx)
{
    int fd;
    duk_size_t bufferSize;
    uint8_t *bufPtr;
    int rc;
    duk_size_t offset = 0; // Default offset is 0.
    duk_size_t length;

    fd = duk_get_int(ctx, 0);
    if (duk_is_buffer(ctx, 1))
    {
        bufPtr = duk_require_buffer_data(ctx, 1, &bufferSize);
        // Unless specified, the length is all of the buffer
        length = bufferSize;

        if (duk_is_number(ctx, 2))
        {
            offset = duk_get_int(ctx, 2);
            if (duk_is_number(ctx, 3))
            {
                length = duk_get_int(ctx, 3);
                if (length > bufferSize)
                {
                    printf("Specified length is > buffer size\n");
                    length = bufferSize;
                } // length > bufferSize
            }     // We have a length
        }         // We have an offset
    }             // Data is buffer
    else
    {
        // Handle the case where the data type is a string.
        bufPtr = (uint8_t *)duk_get_string(ctx, 1);
        length = bufferSize = strlen((char *)bufPtr);
    }
    if (length > (bufferSize - offset))
    {
#ifdef FS_DEBUG_1
        printf("Specified length + offset is > buffer size\n");
#endif
        length = bufferSize - offset;
    }
    rc = write(fd, bufPtr + offset, length);

    duk_push_int(ctx, rc);
    return 1;
}

/* jsondoc
{
"name": "open",
"args": [
{"name": "path", "vtype": "string", "text": "file path"},
{"name": "flags", "vtype": "string", "text": "flags ('r', 'w', 'a', ...)"}
],
"return": "file descriptor",
"text": "Open file.",
"example": "
"
}
*/
static duk_ret_t js_fs_open(duk_context *ctx)
{
#ifdef FS_DEBUG_1
    printf("js_fs_open\n");
#endif

    // Get the path
    const char *path = duk_get_string(ctx, 0);

    if (path == NULL)
    {
#ifdef FS_DEBUG_1
        printf("js_fs_open: Unable to get path\n");
#endif
        return DUK_RET_ERROR;
    }

    // Get the flags
    const char *flags = duk_get_string(ctx, 1);

    if (flags == NULL)
    {
#ifdef FS_DEBUG_1
        printf("js_fs_open: Unable to get flags\n");
#endif
        return DUK_RET_ERROR;
    }
#ifdef FS_DEBUG_1
    printf(" - Path to open is '%s' and flags are '%s'\n", path, flags);
#endif
    int posixOpenFlags = stringToPosixFlags(flags);

    int fd = open(path, posixOpenFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (fd < 0)
    {
#ifdef FS_DEBUG_1
        printf("<< js_fs_open: Error: open(%s, 0x%x): %d errno=%d [%s]\n", path, posixOpenFlags, fd, errno, strerror(errno));
#endif
        return DUK_RET_ERROR;
    }
    // Return the open file descriptor
    duk_push_int(ctx, fd);

#ifdef FS_DEBUG_1
    printf("js_fs_open fd: %d\n", fd);
#endif

    return 1;
}

/* jsondoc
{
"name": "close",
"args": [
{"name": "fd", "vtype": "int", "text": "file descriptor"}
],
"text": "Close file descriptor.",
"example": "
"
}
*/
static duk_ret_t js_fs_close(duk_context *ctx)
{
    int fd = duk_require_int(ctx, 0);
    close(fd);
    return 0;
}

/* jsondoc
{
"name": "fstat",
"args": [
{"name": "fd", "vtype": "int", "text": "file descriptor"}
],
"return": "{'size': file_size}",
"text": "Stat a file descriptor but inly return size.",
"example": "
"
}
*/
static duk_ret_t js_fs_fstat(duk_context *ctx)
{
    struct stat statBuf;
    int fd = duk_require_int(ctx, 0);
    int rc = fstat(fd, &statBuf);
    if (rc == -1)
    {
#ifdef FS_DEBUG_1
        printf("Error from stat of fd %d: %d %s\n", fd, errno, strerror(errno));
#endif
        return 0;
    }
    duk_push_object(ctx);
    duk_push_int(ctx, statBuf.st_size);
    duk_put_prop_string(ctx, -2, "size");
    return 1;
}

/* jsondoc
{
"name": "stat",
"args": [
{"name": "path", "vtype": "string", "text": "filepath"}
],
"return": "{'size': file_size}",
"text": "Stat a file path but inly return size.",
"example": "
"
}
*/
static duk_ret_t js_fs_stat(duk_context *ctx)
{
    struct stat statBuf;
    const char *path = duk_require_string(ctx, 0);
#ifdef FS_DEBUG_1
    printf("js_fs_stat: %s\n", path);
#endif
    int rc = stat(path, &statBuf);
    if (rc == -1)
    {
#ifdef FS_DEBUG_1
        printf("js_fs_stat: Error from stat of file %s: %d %s\n", path, errno, strerror(errno));
#endif
        return DUK_RET_ERROR;
    }
    duk_push_object(ctx);
    duk_push_int(ctx, statBuf.st_size);
    duk_put_prop_string(ctx, -2, "size");
#ifdef FS_DEBUG_1
    printf("js_fs_statS: %s - size: %ld\n", path, statBuf.st_size);
#endif
    return 1;
}

/* jsondoc
{
"name": "read",
"args": [
{"name": "fd", "vtype": "int", "text": "file descriptor"},
{"name": "buffer", "vtype": "Plain Buffer", "text": "buffer"},
{"name": "bufferOffset", "vtype": "uint", "text": "offset into the buffer"},
{"name": "maxRead", "vtype": "uint", "text": "max num bytes to read"},
{"name": "pos", "vtype": "uint", "text": "position in the file to read from (currently NOT supported)"}
],
"return": "number of bytes read",
"text": "Read from a file descriptor.",
"example": "
"
}
*/
static duk_ret_t js_fs_read(duk_context *ctx)
{
#ifdef FS_DEBUG_1
    printf("js_fs_read\n");
#endif
    //esp32_duktape_dump_value_stack(ctx);
    int fd = duk_require_int(ctx, 0);

    size_t writeOffset = duk_require_int(ctx, 2);

    size_t maxToRead = duk_require_int(ctx, 3);

    //size_t position = duk_require_int(ctx, 4);
    ssize_t sizeRead = 0;
    duk_size_t bufferSize;

    uint8_t *bufPtr = duk_require_buffer_data(ctx, 1, &bufferSize);

    if (bufPtr == NULL)
    {
#ifdef FS_DEBUG_1
        printf("Failed to get the buffer pointer\n");
#endif
    }
#ifdef FS_DEBUG_1
    printf("Buffer pointer size returned as %d\n", (int)bufferSize);
#endif

    // Length can't be <= 0.
    // FIX ... PERFORM CHECK HERE

    // Check that writeOffset is within range.  If the buffer is "buffer.length" bytes in size
    // then the writeOffset must be < buffer.length
    if (writeOffset >= bufferSize)
    {
#ifdef FS_DEBUG_1

        printf("Invalid writeOffset\n");
#endif
    }

    // Position can't be < 0
    // FIX ... PERFORM CHECK HERE

    // if length > buffer.length -> length = buffer.length
    if (maxToRead > bufferSize)
    {
        maxToRead = bufferSize;
    }

    // Read the data from the underlying file.
    sizeRead = read(fd, bufPtr + writeOffset, maxToRead);
    if (sizeRead < 0)
    {
#ifdef FS_DEBUG_1
        printf("js_fs_read: read() error: %d %s\n", errno, strerror(errno));
#endif
        sizeRead = 0;
    }

    duk_push_int(ctx, sizeRead); // Push the size onto the value stack.
#ifdef FS_DEBUG_1
    printf("js_fs_read: sizeRead: %d\n", (int)sizeRead);
#endif
    return 1;
}

/* jsondoc
{
"name": "unlink",
"args": [
{"name": "path", "vtype": "int", "text": "filepath"}
],
"text": "Unlink (delete) file.",
"example": "
"
}
*/
static duk_ret_t js_fs_unlink(duk_context *ctx)
{
    const char *path = duk_require_string(ctx, 0);
    unlink(path);
    return 0;
}

/* jsondoc
{
"name": "listDir",
"args": [
],
"return": "array of strings (e.g., ['filename', 'otherfilename'])",
"text": "List files in the filesystem.",
"example": "
"
}
*/
static duk_ret_t js_fs_spiffsDir(duk_context *ctx)
{
    duk_push_array(ctx); // Push a new empty array onto the stack.
    DIR *d;
    struct dirent *dir;

    d = opendir(BASE_PATH);
    if (d != NULL)
    {
        int i = 0;
        while ((dir = readdir(d)) != NULL)
        {
            // Skip if "." or ".."
            if (strcmp(dir->d_name, ".") == 0 ||
                strcmp(dir->d_name, "..") == 0)
            {
                continue;
            }
            duk_push_object(ctx);
            duk_push_string(ctx, dir->d_name);
            duk_put_prop_string(ctx, -2, "name");
            duk_put_prop_index(ctx, -2, i);
            i++;
        }
        closedir(d);
    }
    return 1;
}

/* jsondoc
{
"name": "fsSizeTotal",
"args": [
],
"return": "size in bytes of the filesystem",
"text": "Get total size of the flash filesystem.",
"example": "
"
}
*/
static int fs_size_total(duk_context *ctx)
{
    size_t total = 0;
    size_t used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        duk_push_number(ctx, -1);
        return 1;
    }
    duk_push_number(ctx, total);
    return 1;
}

/* jsondoc
{
"name": "fsSizeUsed",
"args": [
],
"return": "bytes used of the filesystem",
"text": "Get used number of bytes of the flash filesystem.",
"example": "
"
}
*/
static int fs_size_used(duk_context *ctx)
{
    size_t total = 0;
    size_t used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        duk_push_number(ctx, -1);
        return 1;
    }
    duk_push_number(ctx, used);
    return 1;
}

static duk_function_list_entry fs_funcs[] = {
    {"close", js_fs_close, 1},
    {"fstat", js_fs_fstat, 1},
    {"open", js_fs_open, 2},
    {"read", js_fs_read, 5},
    {"listDir", js_fs_spiffsDir, 0},
    {"stat", js_fs_stat, 1},
    {"unlink", js_fs_unlink, 1},
    {"write", js_fs_write, 4},
    {"fsSizeTotal", fs_size_total, 0},
    {"fsSizeUsed", fs_size_used, 0},
    {NULL, NULL, 0},
};

duk_ret_t duk_fs_register(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_push_object(ctx);

    duk_put_function_list(ctx, -1, fs_funcs);
    duk_put_prop_string(ctx, -2, "FileSystem");
    duk_pop(ctx);

    return 0;
}
