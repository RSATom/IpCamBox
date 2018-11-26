#pragma once

#include <memory>

#include <libpq-fe.h>


struct PGFree
{
    void operator() (PGresult* result)
        { PQclear(result); }
    void operator() (PGconn* conn)
        { PQfinish(conn); }
};

typedef
    std::unique_ptr<
        PGresult,
        PGFree> PGresultPtr;
typedef
    std::unique_ptr<
        PGconn,
        PGFree> PGconnPtr;
