#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "dbjoin.h"

const char *DbJoin::join_names[DbJoin::MAX_JOINTYPES] =
{
    "INNER",
    "LEFT",
    "RIGHT",
    "FULL"
};


const char *DbJoin::join_ops[DbJoin::MAX_JOINOPS] =
{
    "=",
    "!=",
    ">",
    ">=",
    "<",
    "<="
};

DbJoin::DbJoin()
       : msg("PgJoin")
{

}



