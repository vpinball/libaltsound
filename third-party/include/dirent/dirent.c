/*
 * Implementation of the standard functions in direct.h
 *
 * opendir(), readdir(), closedir() and rewinddir().
 *
 * 06/17/2000 by Mike Haaland <mhaaland@hypertech.com>
 */

#include "dirent.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

/** open a directory and return a structure to be used in
 *  subsequent readdir() and closedir() calls. Each call gets its
 *  own structure, so listings can be nested, and the current
 *  directory is left alone.
 *
 *  returns NULL on error.
 */
DIR * opendir(const char *dirname) {
    DIR *dir;
    size_t length;

    if (dirname == NULL || *dirname == '\0')
        return NULL;

    /* the search pattern is the directory followed by "\*" */
    length = strlen(dirname);
    if (length + 3 > MAX_PATH)
        return NULL;

    dir = (DIR *)calloc(1, sizeof(DIR));
    if (dir == NULL)
        return NULL;

    memcpy(dir->pattern, dirname, length);
    if (dirname[length - 1] != '/' && dirname[length - 1] != '\\')
        dir->pattern[length++] = '\\';
    dir->pattern[length++] = '*';
    dir->pattern[length] = '\0';

    /* fail like POSIX opendir() when the directory doesn't exist */
    dir->handle = FindFirstFileA(dir->pattern, &dir->findFileData);
    if (INVALID_HANDLE_VALUE == dir->handle)
    {
        free(dir);
        return NULL;
    }
    dir->firstTime = TRUE;

    return dir;
}

/** Close the directory and free its structure - return 0 if success */
int	closedir(DIR *dirp) {
    int result = 1;

    if (dirp == NULL)
        return -1;

    if (dirp->handle != INVALID_HANDLE_VALUE)
        result = FindClose(dirp->handle);

    free(dirp);

    return (result == 0) ? -1 : 0;
}

/** get the next entry in the directory */
struct dirent *	readdir(DIR *dirp) {
    if (dirp == NULL)
        return NULL;

    if (TRUE == dirp->firstTime)
    {
        /** opendir() already read the first entry */
        dirp->firstTime = FALSE;
        if (INVALID_HANDLE_VALUE == dirp->handle)
            return NULL;
    }
    else
    {
        if (INVALID_HANDLE_VALUE == dirp->handle || 0 == FindNextFileA(dirp->handle, &dirp->findFileData))
            return NULL;
    }

    /* we have a valid FIND_FILE_DATA, copy the filename */
    memset(&dirp->entry, '\0', sizeof(struct dirent));

    strncpy(dirp->entry.d_name, dirp->findFileData.cFileName, MAX_PATH - 1);
    dirp->entry.d_namlen = (unsigned int)strlen(dirp->entry.d_name);

    return &dirp->entry;
}

/** reopen the directory */
void rewinddir(DIR *dirp) {
    if (dirp == NULL)
        return;

    if (dirp->handle != INVALID_HANDLE_VALUE)
        FindClose(dirp->handle);

    dirp->handle = FindFirstFileA(dirp->pattern, &dirp->findFileData);
    dirp->firstTime = TRUE;
}

#endif
