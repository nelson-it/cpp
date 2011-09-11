#ifndef dbcursor_mne
#define dbcursor_mne

#include "dbconnect.h"

class DbCursor 
{
public:
     virtual ~DbCursor() {};

     virtual void open( char *stm) = 0;
     virtual void close() = 0;
     virtual int eof() = 0;

     virtual void end() = 0;
     virtual void start() = 0;

     virtual DbConnect::ResultVec next() = 0;
     virtual DbConnect::ResultVec* p_next() = 0;

};


#endif /* dbcursor_mne */

