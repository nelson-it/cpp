#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "filetree.h"

FileTree::FileTree(FileTree *root) :
msg("FileTree")
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    h = INVALID_HANDLE_VALUE;
#else
    hdir = NULL;
    entry = 0;
#endif

    act = this;
    this->tree_root = root;
}

FileTree::FileTree() :
            msg("FileTree")
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    h = INVALID_HANDLE_VALUE;
#else
    hdir = NULL;
    entry = 0;
#endif

    act = this;
    tree_root = this;
    last_act = this;
}

FileTree::~FileTree()
{
    cancel();
}

int FileTree::reset(std::string dir)
{
    cancel();

    this->dir = dir;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string str;

    str = this->uncroot;
    if (this->dir != "") str += "\\" + this->dir;
    str += "\\*";

    h = FindFirstFile(str.c_str(), &d);
    if (h == INVALID_HANDLE_VALUE)
    {
        msg.perror(E_RESET, "Konnte Ordner nicht öffnen");
        msg.line("<%s>", str.c_str());
        utils.perror();
        return 0;
    }

    this->name = d.cFileName;
    utils.setName(this->dir, this->name);
    tree_root->last_act = act = this;

    return 1;

#else
    entry = 0;
    struct stat buf;

    hdir = opendir((this->root + "/" + this->dir).c_str());
    if ( hdir != NULL )
    {
        entry = readdir(hdir);
        while ( entry && lstat(( this->root + "/" + this->dir + "/" + entry->d_name).c_str(), &buf ) != 0 )  entry = readdir(hdir);
        if (entry)
        {
            this->name = entry->d_name;
            utils.setName(this->dir, this->name);
            tree_root->last_act = act = this;
            return 1;
        }
    }

    msg.perror(E_RESET, "Konnte Ordner nicht öffnen");
    msg.line("<%s>", this->root.c_str());
    utils.perror();
    return 0;
#endif
}

int FileTree::next(int ignoredir)
{
    if (act != this)
    {
        int result;
        result = act->next(ignoredir);
        if (result) return result;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)

    BOOL result;
    if (h != INVALID_HANDLE_VALUE)
    {
        if (ignoredir == 0 && act == this && this->name != "." && this->name != ".." && utils.isDir())
        {
            act = new FileTree(tree_root);
            act->root = this->root;
            act->copyroot = this->copyroot;
            act->uncroot = this->uncroot;
            act->utils.setRoot(this->root);
            act->utils.setCopyRoot(this->copyroot);
            if (this->dir != "") act->dir = this->dir + "\\" + this->name;
            else act->dir = this->name;

            if (act->reset(act->dir)) return 1;
            act->cancel();
            delete act;
            act = this;
            tree_root->last_act = act;
        }
        else if (act != this)
        {
            delete act;
        }

        tree_root->last_act = act = this;

        result = FindNextFile(h, &d);
        if (result)
        {
            this->name = d.cFileName;
            utils.setName(this->dir, this->name);
            return 1;
        }
        else
        {
            cancel();
            return 0;
        }
    }
    else
    {
        return 0;
    }

#else
    struct stat buf;
    if ( entry )
    {
        if (ignoredir == 0 && act == this && this->name != "." && this->name != ".." && utils.isDir())
        {
            act = new FileTree(tree_root);
            act->root = this->root;
            act->copyroot = this->copyroot;
            act->utils.setRoot(this->root);
            act->utils.setCopyRoot(this->copyroot);
            if (this->dir != "") act->dir = this->dir + "/" + this->name;
            else act->dir = this->name;

            if (act->reset(act->dir)) return 1;
            act->cancel();
            delete act;
            act = this;
            tree_root->last_act = act;
        }
        else if (act != this)
        {
            delete act;
        }

        tree_root->last_act = act = this;

        if ( hdir != NULL )
        {
            entry = readdir(hdir);
            while ( entry && lstat(( this->root + "/" + this->dir + "/" + entry->d_name).c_str(), &buf ) != 0 )  entry = readdir(hdir);

            if (entry)
            {
                this->name = entry->d_name;
                utils.setName(this->dir, this->name);
               return 1;
            }
            else
            {
                cancel();
                return 0;
            }
        }
        else
            return 0;
    }
    else
    {
        return 0;
    }

#endif
return 0;

}

void FileTree::cancel()
{
    if (act != this) act->cancel();

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if (h != INVALID_HANDLE_VALUE) FindClose(h);
    h = INVALID_HANDLE_VALUE;
    return;
#else
    if ( hdir != NULL ) closedir(hdir);
    hdir = NULL;
    entry = 0;

#endif
}
