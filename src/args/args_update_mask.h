/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Argument parsing header for update_mask
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

#ifndef ARGS_UPDATE_MASK_H
#define ARGS_UPDATE_MASK_H

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
  char path_mask[STRLEN];
  char path_disturbance[STRLEN];
  char path_output[STRLEN];
} args_t;

void usage(char *exe, int exit_code);
void parse_args(int argc, char *argv[], args_t *args);

#ifdef __cplusplus
}
#endif

#endif
