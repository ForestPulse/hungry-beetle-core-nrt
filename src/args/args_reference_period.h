/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Argument parsing header
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#ifndef ARGS_REFERENCE_PERIOD_H
#define ARGS_REFERENCE_PERIOD_H

#include <stdio.h>   // core input and output functions
#include <stdlib.h>  // standard general utilities library
#include <unistd.h>   // essential POSIX functions and constants
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
  char path_mask[STRLEN];
  char path_input_reference_period[STRLEN];
  char path_output_reference_period[STRLEN];
  char path_input_coefficient[STRLEN];
  char path_output_coefficient[STRLEN];
  int modes;
  int trend;
  int year;
  int threshold;
  int confirmation_number;
} args_t;

void usage(char *exe, int exit_code);
void parse_args(int argc, char *argv[], args_t *args);

#ifdef __cplusplus
}
#endif

#endif

