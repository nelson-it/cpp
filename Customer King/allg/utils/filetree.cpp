#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "filetree.h"

FileTree::FileTree(FileTree *root) :
    msg("FileTree")
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    h = INVALID_HANDLE_VALUE;
    act = this;
    this->tree_root = root;
#else
    not implemented
#endif
}

FileTree::FileTree() :
    msg("FileTree")
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    h = INVALID_HANDLE_VALUE;
    act = this;
    tree_root = this;
    last_act = this;
#else
    not implemented
#endif
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
    not implemented
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
    not implemented
#endif
}

void FileTree::cancel()
{
    if (act != this) act->cancel();

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if (h != INVALID_HANDLE_VALUE) FindClose(h);
    h = INVALID_HANDLE_VALUE;
    return;
#else
    not implemented
#endif
}
