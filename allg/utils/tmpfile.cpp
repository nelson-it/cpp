#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#define DIRSEP   "\\"
#define unlink DeleteFile
#include <winsock2.h>
#include <windows.h>
#include <sys/unistd.h>
#else
#include <unistd.h>
#define DIRSEP   "/"
#endif

#include "tmpfile.h"

TmpFile::TmpFile(const char *tmpl, int needopen) :
        msg("TmpFile")
{
    *filename = '\0';
    fp = NULL;
    buffer = NULL;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( getenv ("TEMP") != NULL)
    {
        strncpy(filename, getenv("TEMP"), sizeof(filename) -1 );
        strncat(filename, "\\HttpReportXXXXXX", sizeof(filename) - strlen(filename) - 1);
    }
    _mktemp_s(filename, strlen(filename) + 1);
    filename[sizeof(filename) - 1] = '\0';
#else
    int fd;
    if (getenv("TMPDIR") != NULL)
    {
        strncpy(filename, getenv("TMPDIR"), sizeof(filename) - 1);
        strncat(filename, "/", sizeof(filename) - strlen(filename) - 1);
    }
    else if (getenv("TMP") != NULL)
    {
        strncpy(filename, getenv("TMP"), sizeof(filename) - 1);
        strncat(filename, "/", sizeof(filename) - strlen(filename) - 1);
    }
    else
    {
        strcpy(filename, "/tmp/");
    }
    strncat(filename, tmpl, sizeof(filename) - strlen(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';

    fd = mkstemp(filename);
    if (fd != -1)
    {
        ::close(fd);
        unlink(filename);
    }
    else
    {
        msg.perror(1, "kann temporäres für <%s> nicht öffnen", tmpl);
    }
#endif

    if (needopen)
    {
        fp = fopen(filename, "wb+");
    }

}

TmpFile::~TmpFile()
{
    close();
    unlink(filename);
}

void TmpFile::close()
{
    if (fp != NULL) ::fclose(fp);
    if (buffer != NULL) delete[] buffer;
    fp = NULL;
}

std::string TmpFile::get_content()
{
    int length;

    if ( fp == NULL )
        fp = fopen(filename, "rb+");

    if ( fp == NULL ) return "";

    fseek (fp, 0, SEEK_END);
    length = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    if ( buffer ) delete buffer;
    buffer = new char[length + 1];

    if (buffer)
    {
        fread (buffer, 1, length, fp);
        buffer[length] = '\0';
        return buffer;
    }
    return "";
}


TmpDir::TmpDir(const char *tmpl) :
                msg("TmpDir")
{
    *dirname = '\0';

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( getenv ("TEMP") != NULL)
    {
        strncpy(dirname, getenv("TEMP"), sizeof(dirname) -1 );
        strncat(dirname, "\\HttpReportXXXXXX", sizeof(dirname) - strlen(dirname) - 1);
    }
    _mktemp_s(dirname, strlen(dirname) + 1);
    dirname[sizeof(dirname) - 1] = '\0';
    mkdir(dirname);
#else
    if (getenv("TMPDIR") != NULL)
    {
        strncpy(dirname, getenv("TMPDIR"), sizeof(dirname) - 1);
        strncat(dirname, "/", sizeof(dirname) - strlen(dirname) - 1);
    }
    else if (getenv("TMP") != NULL)
    {
        strncpy(dirname, getenv("TMP"), sizeof(dirname) - 1);
        strncat(dirname, "/", sizeof(dirname) - strlen(dirname) - 1);
    }
    else
    {
        strcpy(dirname, "/tmp/");
    }
    strncat(dirname, tmpl, sizeof(dirname) - strlen(dirname) - 1);
    dirname[sizeof(dirname) - 1] = '\0';
    mkdtemp(dirname);
#endif

}

TmpDir::~TmpDir()
{
    rmdir(dirname);
}
