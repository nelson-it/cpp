#ifndef filetree_mne
#define filetree_mne

#include <string>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#undef WINVER
#define WINVER 0x0500
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <message/message.h>
#include "fileutils.h"

class FileTree
{
    Message msg;
    enum ERROR_TYPES
    {
       E_OK,
       E_RESET
    };

    std::string root;
    std::string dir;
    std::string name;

    std::string copyroot;

    #if defined(__MINGW32__) || defined(__CYGWIN__)
    HANDLE h;
    WIN32_FIND_DATA d;
    std::string uncroot;
    #else
    DIR *hdir;
    struct dirent *entry;
    #endif

    FileTree *tree_root;
    FileTree *last_act;
    FileTree *act;

    FileUtils utils;

public:
    FileTree();
    FileTree(FileTree *root);
    virtual ~FileTree();

    int  reset(std::string dir = "");
    int  next(int ignoredir = 0);
    void cancel();

    void setRoot(std::string root)
    {
      this->root = root;
      this->dir = "";

#if defined(__MINGW32__) || defined(__CYGWIN__)
      if ( this->root[0] == '\\' && this->root[1] == '\\' )
	  this->uncroot = "\\\\?\\UNC" + this->root.substr(1);
      else
	  this->uncroot = "\\\\?\\" + this->root;
#endif
      utils.setRoot(root);
      utils.setCopyRoot(root);
    }

    void setCopyRoot(std::string copyroot)
    {
        this->copyroot = copyroot;
	utils.setCopyRoot(copyroot);
    }


    std::string getRoot() { return this->root; }
    std::string getCopyRoot() { return this->copyroot; }
    std::string getDir()  { return tree_root->last_act->dir;  }
    std::string getName() { return tree_root->last_act->name; }
    std::string getFullname()
    {
        if ( tree_root->last_act->dir == "" )
	    return tree_root->last_act->name;
	else
#if defined(__MINGW32__) || defined(__CYGWIN__)
	    return tree_root->last_act->dir + "\\" + tree_root->last_act->name;
#else
        return tree_root->last_act->dir + "/" + tree_root->last_act->name;
#endif
    }
    time_t getCreatetime(int werror = 1)
        { return tree_root->last_act->utils.getCreatetime(werror); }
    time_t getAccesstime(int werror = 1)
        { return tree_root->last_act->utils.getAccesstime(werror); }
    time_t getModifytime(int werror = 1)
        { return tree_root->last_act->utils.getModifytime(werror); }

    int isDir( int werror = 1 )
        { return tree_root->last_act->utils.isDir(werror);}
    int exists( int werror = 1)
        { return tree_root->last_act->utils.exists(werror); }

    FileUtils *p_getUtils()
        { return &tree_root->last_act->utils; }

};

#endif /* filetree_mne */
