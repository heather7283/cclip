#ifndef _GETOPT_H
#define _GETOPT_H

#ifdef __cplusplus
extern "C" {
#endif

int getopt(int, char * const [], const char *);
extern char *optarg;
extern int optind, opterr, optopt, optreset;

#ifdef __cplusplus
}
#endif

#endif
