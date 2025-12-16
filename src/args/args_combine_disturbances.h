/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Argument parsing header for combine_disturbances
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

#ifndef ARGS_COMBINE_DISTURBANCES_H
#define ARGS_COMBINE_DISTURBANCES_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "../utils/alloc.h"
#include "../utils/const.h"
#include "../utils/dir.h"
#include "../utils/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int n_cpus;
  int n_images;
  char **path_input;
  char path_output[STRLEN];
} args_t;

void usage(char *exe, int exit_code);
void parse_args(int argc, char *argv[], args_t *args);

#ifdef __cplusplus
}
#endif

#endif
