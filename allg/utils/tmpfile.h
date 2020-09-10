#ifndef tmpfile_mne
#define tmpfile_mne

#include <stdio.h>
#include <message/message.h>

class TmpFile
{
    Message msg;
    char filename[256];
    FILE *fp;

public:
    TmpFile(const char *tmpl, int needopen = 0 );
    virtual ~TmpFile();

    char *get_name() { return filename; }
    FILE *get_fp() { return fp; }

    void close();

};

class TmpDir
{
    Message msg;
    char dirname[256];

public:
    TmpDir(const char *tmpl );
    virtual ~TmpDir();

    char *get_name() { return dirname; }

};

#endif /* tmpfile_mne */
