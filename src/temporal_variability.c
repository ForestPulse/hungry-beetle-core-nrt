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
  char path_mask[STRLEN];
  char path_reference[STRLEN];
  char path_output[STRLEN];
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -j cpus -o output-image -x mask-image -r reference-period-image input-image(s)\n", exe);
  printf("\n");
  printf("  -j = number of CPUs to use\n");
  printf("\n");
  printf("  -o = output image\n");
  printf("  -x = mask image\n");
  printf("  -r = reference period image\n");
  printf("\n");
  printf("  input-image(s) = one or more input images to compute temporal variability from\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 4;
  opterr = 0;

  while ((opt = getopt(argc, argv, "j:o:r:x:")) != -1){
    switch(opt){
      case 'j':
        args->n_cpus = atoi(optarg);
        received_n++;
        break;
      case 'o':
        copy_string(args->path_output, STRLEN, optarg);
        received_n++;
        break;
      case 'r':
        copy_string(args->path_reference, STRLEN, optarg);
        received_n++;
        break;
      case 'x':
        copy_string(args->path_mask, STRLEN, optarg);
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

  if (!fileexist(args->path_reference)){
    fprintf(stderr, "Reference file %s does not exist.\n", args->path_reference);
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
date_t *dates = NULL;
image_t *input = NULL;
image_t mask;
image_t variability;
image_t reference;


  parse_args(argc, argv, &args);

  GDALAllRegister();

  read_image(args.path_mask, NULL, &mask);
  read_image(args.path_reference, NULL, &reference);
  compare_images(&mask, &reference);

  alloc((void**)&input, args.n_images, sizeof(image_t));
  alloc((void**)&dates, args.n_images, sizeof(date_t));

  for (int i=0; i<args.n_images; i++){
  
    char basename[STRLEN];
    basename_with_ext(args.path_input[i], basename, STRLEN);
    date_from_string(&dates[i], basename);
    
    read_image(args.path_input[i], NULL, &input[i]);
    compare_images(&mask, &input[i]);

    if (i > 0){
      if (dates[i].ce < dates[i-1].ce){
        fprintf(stderr, "Input images must be ordered by date (earliest to latest).\n");
        exit(FAILURE);
      }
    }

  }


  enum { start, end };
  int n_years = 2100; // should be enough, no?
  int **range = NULL;
  alloc_2D((void***)&range, n_years, 2, sizeof(int));
  for (int i=0; i<args.n_images; i++){
    if (range[dates[i].year][start] == 0) range[dates[i].year][start] = i;
    if (range[dates[i].year][end] < (i+1)) range[dates[i].year][end] = (i+1);
  }

  copy_image(&reference, &variability, 1, SHRT_MIN, args.path_output);

  
  omp_set_num_threads(args.n_cpus);

  #pragma omp parallel shared(input, mask, range, variability, reference) default(none)
  {

  #pragma omp for
  for (int p=0; p<variability.nc; p++){

    if (mask.data[0][p] == mask.nodata || mask.data[0][p] == 0) continue;

    variability.data[0][p] = variability.nodata;

    if (reference.data[0][p] == reference.nodata){
      continue;
    }
    
    double mean = 0, var = 0, n = 0;
  
    for (int i=range[reference.data[0][p]][start]; i<range[reference.data[0][p]][end]; i++){

      if (input[i].data[0][p] == input[i].nodata) continue;

      // compute mean, variance
      n++;
      var_recurrence((double)input[i].data[0][p], &mean, &var, (double)n);
    }

    if (n > 0) variability.data[0][p] = (short)standdev(var, n);
  
  }

  } // end omp parallel region

  write_image(&variability);

  for (int i=0; i<args.n_images; i++){
    free_image(&input[i]);
  }
  free((void*)input);
  free_image(&mask);
  free_image(&variability);
  free_image(&reference);
  free((void*)dates);
  free_2D((void**)range, n_years);
  free_2D((void**)args.path_input, args.n_images);
  
  GDALDestroy();

  exit(SUCCESS);
}
