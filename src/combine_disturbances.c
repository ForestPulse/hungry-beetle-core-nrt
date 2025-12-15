#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/** Geospatial Data Abstraction Library (GDAL) **/
#include "gdal.h"       // public (C callable) GDAL entry points
#include "cpl_conv.h"   // various convenience functions for CPL
#include "cpl_string.h" // various convenience functions for strings

/** OpenMP **/
#include <omp.h> // multi-platform shared memory multiprocessing

#include "utils/alloc.h"
#include "utils/const.h"
#include "utils/date.h"
#include "utils/dir.h"
#include "utils/image_io.h"
#include "utils/string.h"
#include "utils/stats.h"

typedef struct {
  int n_cpus;
  int n_images;
  char **path_input;
  char path_output[STRLEN];
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -j cpus -o output-image input-image(s)\n", exe);
  printf("\n");
  printf("  -j = number of CPUs to use\n");
  printf("\n");
  printf("  -o = output image\n");
  printf("\n");
  printf("  input-image(s) = one or more input images to compute temporal variability from\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 2;
  opterr = 0;

  while ((opt = getopt(argc, argv, "j:o:")) != -1){
    switch(opt){
      case 'j':
        args->n_cpus = atoi(optarg);
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

  if (args->n_cpus < 1){
    fprintf(stderr, "Number of CPUs must be at least 1.\n");
    usage(argv[0], FAILURE);
  }

  return;
}




int main ( int argc, char *argv[] ){
args_t args;
image_t *input = NULL;
image_t output;


  parse_args(argc, argv, &args);

  GDALAllRegister();

  
  alloc((void**)&input, args.n_images, sizeof(image_t));
  
  for (int i=0; i<args.n_images; i++){
    read_image(args.path_input[i], NULL, &input[i]);
    if (i > 0) compare_images(&input[0], &input[i]);
  }


  copy_image(&input[0], &output, input[0].nb, input[0].nodata, args.path_output);

  
  omp_set_num_threads(args.n_cpus);

  #pragma omp parallel shared(input, output, args) default(none)
  {

  #pragma omp for
  for (int p=0; p<output.nc; p++){

    output.data[0][p] = output.nodata;

    for (int i=0; i<args.n_images; i++){
      for (int b=0; b<input[i].nb; b++){
        if (input[i].data[b][p] != input[i].nodata && input[i].data[b][p] > 0) output.data[b][p] = input[i].data[b][p];
      }
    }

  }

  } // end omp parallel region

  write_image(&output);

  for (int i=0; i<args.n_images; i++){
    free_image(&input[i]);
  }
  free((void*)input);
  free_image(&output);
  free_2D((void**)args.path_input, args.n_images);
  
  GDALDestroy();

  exit(SUCCESS);
}
