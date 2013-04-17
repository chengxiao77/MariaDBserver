/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"


// Try to open an environment where the directory does not exist 
// Try when the dir exists but is not an initialized env
// Try when the dir exists and we do DB_CREATE: it should work.
// And after that the open should work without a DB_CREATE
//   However, in BDB, after doing an DB_ENV->open and then a close, no state has changed
//   One must actually create a DB I think...


#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

// ENVDIR is defined in the Makefile

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    DB_ENV *dbenv;
    int r;
    int do_private;

    for (do_private=0; do_private<2; do_private++) {
#ifdef USE_TDB
	if (do_private==0) continue; // See #208.
#else
	if (do_private==1) continue; // See #530.  BDB 4.6.21 segfaults if DB_PRIVATE is passed when no environment previously exists.
#endif
	int private_flags = do_private ? (DB_CREATE|DB_PRIVATE) : 0;
	
	r = system("rm -rf " ENVDIR);
	CKERR(r);
	r = db_env_create(&dbenv, 0);
	CKERR(r);
	r = dbenv->open(dbenv, ENVDIR, private_flags|DB_INIT_MPOOL, 0);
	assert(r==ENOENT);
	dbenv->close(dbenv,0); // free memory
	
	r = system("rm -rf " ENVDIR);
	CKERR(r);
	toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
	r = db_env_create(&dbenv, 0);
	CKERR(r);
	r = dbenv->open(dbenv, ENVDIR, private_flags|DB_INIT_MPOOL, 0);
#ifdef USE_TDB
	// TokuDB has no trouble opening an environment if the directory exists.
	CKERR(r);
	assert(r==0);
#else
	if (r!=ENOENT) printf("%s:%d %d: %s\n", __FILE__, __LINE__, r,db_strerror(r));
	assert(r==ENOENT);
#endif
	dbenv->close(dbenv,0); // free memory
    }

#ifndef USE_TDB
    // Now make sure that if we have a non-private DB that we can tell if it opened or not.
    DB *db;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&dbenv, 0);
    CKERR(r);
    r = dbenv->open(dbenv, ENVDIR, DB_CREATE|DB_INIT_MPOOL, 0);
    CKERR(r);
    r=db_create(&db, dbenv, 0);
    CKERR(r);
    db->close(db, 0);
    dbenv->close(dbenv,0); // free memory
    r = db_env_create(&dbenv, 0);
    CKERR(r);
    r = dbenv->open(dbenv, ENVDIR, DB_INIT_MPOOL, 0);
    CKERR(r);
    dbenv->close(dbenv,0); // free memory
#endif

    return 0;
}
