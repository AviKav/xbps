/*-
 * Copyright (c) 2013 Juan Romero Pardines.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <xbps.h>
#include "defs.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-pkgdb [OPTIONS] [PKGNAME...]\n\n"
	    "OPTIONS\n"
	    " -a --all                               Process all packages\n"
	    " -C --config <file>                     Full path to configuration file\n"
	    " -d --debug                             Debug mode shown to stderr\n"
	    " -h --help                              Print usage help\n"
	    " -m --mode <auto|manual|hold|unhold>    Change PKGNAME to this mode\n"
	    " -r --rootdir <dir>                     Full path to rootdir\n"
	    " -u --update                            Update pkgdb to the latest format\n"
	    " -v --verbose                           Verbose messages\n"
	    " -V --version                           Show XBPS version\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int
change_pkg_mode(struct xbps_handle *xhp, const char *pkgname, const char *mode)
{
	xbps_dictionary_t pkgd;

	pkgd = xbps_pkgdb_get_pkg(xhp, pkgname);
	if (pkgd == NULL)
		return errno;

	if (strcmp(mode, "auto") == 0)
		xbps_dictionary_set_bool(pkgd, "automatic-install", true);
	else if (strcmp(mode, "manual") == 0)
		xbps_dictionary_set_bool(pkgd, "automatic-install", false);
	else if (strcmp(mode, "hold") == 0)
		xbps_dictionary_set_bool(pkgd, "hold", true);
	else if (strcmp(mode, "unhold") == 0)
		xbps_dictionary_remove(pkgd, "hold");
	else
		usage(true);

	return xbps_pkgdb_update(xhp, true);
}

int
main(int argc, char **argv)
{
	const char *shortopts = "aC:dhm:r:uVv";
	const struct option longopts[] = {
		{ "all", no_argument, NULL, 'a' },
		{ "config", required_argument, NULL, 'C' },
		{ "debug", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ "mode", required_argument, NULL, 'm' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "update", no_argument, NULL, 'u' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	const char *conffile = NULL, *rootdir = NULL, *instmode = NULL;
	int c, i, rv, flags = 0;
	bool update_format = false, all = false;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'a':
			all = true;
			break;
		case 'C':
			conffile = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'm':
			instmode = optarg;
			break;
		case 'r':
			rootdir = optarg;
			break;
		case 'u':
			update_format = true;
			break;
		case 'v':
			flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage(true);
			/* NOTREACHED */
		}
	}
	if (!update_format && !all && (argc == optind))
		usage(true);

	memset(&xh, 0, sizeof(xh));
	if (rootdir)
		strncpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
	xh.conffile = conffile;
	xh.flags = flags;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	if (update_format)
		convert_pkgdb_format(&xh);
	else if (instmode) {
		if (argc == optind) {
			fprintf(stderr,
			    "xbps-pkgdb: missing PKGNAME argument\n");
			exit(EXIT_FAILURE);
		}
		for (i = optind; i < argc; i++) {
			rv = change_pkg_mode(&xh, argv[i], instmode);
			if (rv != 0) {
				fprintf(stderr, "xbps-pkgdb: failed to "
				    "change to %s mode to %s: %s\n",
				    instmode, argv[i], strerror(rv));
				exit(EXIT_FAILURE);
			}
		}
	} else if (all) {
		rv = check_pkg_integrity_all(&xh);
	} else {
		for (i = optind; i < argc; i++) {
			rv = check_pkg_integrity(&xh, NULL, argv[i]);
			if (rv != 0)
				fprintf(stderr, "Failed to check "
				    "`%s': %s\n", argv[i], strerror(rv));
		}
	}

	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
