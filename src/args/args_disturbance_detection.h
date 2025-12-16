/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Argument parsing header for disturbance_detection
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

#ifndef ARGS_DISTURBANCE_DETECTION_H
#define ARGS_DISTURBANCE_DETECTION_H

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
  char path_mask[STRLEN];
  char path_variability[STRLEN];
  char path_coefficients[STRLEN];
  char path_output[STRLEN];
  int modes;
  int trend;
  float threshold_variability;
  float threshold_residual;
  int confirmation_number;
} args_t;

void usage(char *exe, int exit_code);
void parse_args(int argc, char *argv[], args_t *args);

#ifdef __cplusplus
}
#endif

#endif
