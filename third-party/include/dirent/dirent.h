/*
 * Defines and structures used to implement the
 * functionality standard in direct.h for:
 *
 * opendir(), readdir(), closedir() and rewinddir().
 *
 * 06/17/2000 by Mike Haaland <mhaaland@hypertech.com>
 */
#ifndef _DIRENT_H_
#define _DIRENT_H_
#if !defined(__GNUC__) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ >= 4)	// GCC supports "pragma once" correctly since 3.4
#pragma once
#endif

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
//#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#include <sys/types.h>

#if !defined(__GNUC__)
/* Convienience macros used with stat structures */
#define S_ISDIR(x) ((x) & _S_IFDIR)
#define S_ISREG(x) ((x) & _S_IFREG)
#endif

/* Standard directory name entry returned by readdir() */
struct dirent {
  unsigned int d_namlen;
  char d_name[MAX_PATH];
};

/* Structure to keep track of the current directory status, one per opendir() */
typedef struct my_dir {
    HANDLE           handle;
    WIN32_FIND_DATAA findFileData;
    BOOLEAN          firstTime;
    char             pattern[MAX_PATH];
    struct dirent    entry;
} DIR;

/* function prototypes */
int		        closedir(DIR *dirp);
DIR *		    opendir(const char *dirname);
struct dirent *	readdir(DIR *dirp);
void		    rewinddir(DIR *dirp);

#endif
#endif

