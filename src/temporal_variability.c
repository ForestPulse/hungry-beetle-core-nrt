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
#include "utils/image_io.h"
#include "utils/string.h"
#include "utils/stats.h"

typedef struct {
  int n_images;
  char **path_input;
  char path_output[STRLEN];
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -o output-image input-image(s)\n", exe);
  printf("\n");
  printf("  -o = output image\n");
  printf("\n");
  printf("  input-image(s) = one or more input images to compute temporal variability from\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 1;
  opterr = 0;

  while ((opt = getopt(argc, argv, "o:")) != -1){
    switch(opt){
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
    fprintf(stderr, "Not all '-' arguments received.\n");
    usage(argv[0], FAILURE);
  }

  if ((args->n_images = argc - optind) < 1){
    fprintf(stderr, "At least one input image must be provided.\n");
    usage(argv[0], FAILURE);
  }

  alloc_2D((void***)&args->path_input, args->n_images, STRLEN, sizeof(char));
  for (int i=0; i<args->n_images; i++){
    copy_string(args->path_input[i], STRLEN, argv[optind + i]);
    if (!fileexist(args->path_input[i])){
      fprintf(stderr, "Input file %s does not exist.\n", args->path_input[i]);
      usage(argv[0], FAILURE);
    }
  }
  
  if (fileexist(args->path_output)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output);
    usage(argv[0], FAILURE);
  }

  return;
}




int main ( int argc, char *argv[] ){
args_t args;
image_t *input;
image_t variability;

  parse_args(argc, argv, &args);

  GDALAllRegister();

  alloc((void**)&input, args.n_images, sizeof(image_t));

  for (int i=0; i<args.n_images; i++){

    read_image(args.path_input[i], NULL, &input[i]);

    if (i > 0) compare_images(&input[0], &input[i]);

  }

  copy_image(&input[0], &variability, 1, SHRT_MIN, args.path_output);

  for (int p=0; p<variability.nc; p++){

    variability.data[0][p] = variability.nodata;
    
    double mean = 0, var = 0, n = 0;
  
    for (int i=0; i<args.n_images; i++){

      if (input[i].data[0][p] == input[i].nodata) continue;

      // compute mean, variance
      n++;
      var_recurrence((double)input[i].data[0][p], &mean, &var, (double)n);
    }

    if (n > 0) variability.data[0][p] = (short)standdev(var, n);
  
  }

  write_image(&variability);

  for (int i=0; i<args.n_images; i++){
    free_image(&input[i]);
  }
  free((void*)input);
  free_image(&variability);
  free_2D((void**)args.path_input, args.n_images);
  
  GDALDestroy();

  exit(SUCCESS);
}
