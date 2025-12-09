#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/** Geospatial Data Abstraction Library (GDAL) **/
#include "gdal.h"       // public (C callable) GDAL entry points
#include "cpl_conv.h"   // various convenience functions for CPL
#include "cpl_string.h" // various convenience functions for strings


#include "utils/alloc.h"
#include "utils/const.h"
#include "utils/dir.h"
#include "utils/quality.h"
#include "utils/image_io.h"
#include "utils/string.h"


typedef struct {
  char path_mask[STRLEN];
  char path_disturbance[STRLEN];
  char path_output[STRLEN];
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -d disturbance-image -x mask-image -o output-image\n", exe);
  printf("\n");
  printf("  -d = disturbance image\n");
  printf("  -x = mask image\n");
  printf("  -o = output image\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 3;
  opterr = 0;

  while ((opt = getopt(argc, argv, "d:x:o:")) != -1){
    switch(opt){
      case 'd':
        copy_string(args->path_disturbance, STRLEN, optarg);
        received_n++;
        break;
      case 'x':
        copy_string(args->path_mask, STRLEN, optarg);
        received_n++;
        break;
      case 'o':
        copy_string(args->path_output, STRLEN, optarg);
        received_n++;
        break;
      case '?':
        if (isprint(optopt)){
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        }
        usage(argv[0], FAILURE);
      default:
        fprintf(stderr, "Error parsing arguments.\n");
        usage(argv[0], FAILURE);
    }
  }

  if (received_n != expected_n){
    fprintf(stderr, "Not all arguments received.\n");
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_disturbance)){
    fprintf(stderr, "Disturbance file %s does not exist.\n", args->path_disturbance);
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_mask)){
    fprintf(stderr, "Mask file %s does not exist.\n", args->path_mask);
    usage(argv[0], FAILURE);
  }
  
  if (fileexist(args->path_output)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output);
    usage(argv[0], FAILURE);
  }

  return;
}


int main ( int argc, char *argv[] ){
args_t args;
image_t disturbance;
image_t mask;
image_t output;


  parse_args(argc, argv, &args);

  GDALAllRegister();

  read_image(args.path_disturbance, NULL, &disturbance);
  read_image(args.path_mask, NULL, &mask);
  compare_images(&disturbance, &mask);
  
  copy_image(&disturbance, &output, 1, SHRT_MIN, args.path_output);

  for (int p=0; p<output.nc; p++){

    output.data[0][p] = mask.data[0][p];

    if (mask.data[0][p] == mask.nodata || mask.data[0][p] == 0 || 
        disturbance.data[0][p] == disturbance.nodata) continue;

    if (disturbance.data[0][p] > 0) output.data[0][p] = 0;
 
  }

  write_image(&output);

  free_image(&disturbance);
  free_image(&mask);
  free_image(&output);

  GDALDestroy();

  exit(SUCCESS);
}
