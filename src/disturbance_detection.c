#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/** Geospatial Data Abstraction Library (GDAL) **/
#include "gdal.h"       // public (C callable) GDAL entry points
#include "cpl_conv.h"   // various convenience functions for CPL
#include "cpl_string.h" // various convenience functions for strings

/** OpenMP **/
#include <omp.h> // multi-platform shared memory multiprocessing

#include "utils/const.h"
#include "utils/alloc.h"
#include "utils/date.h"
#include "utils/dir.h"
#include "utils/harmonic.h"
#include "utils/image_io.h"
#include "utils/string.h"


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

void usage(char *exe, int exit_code){


  printf("Usage: %s -j cpus -c coefficient-image -s variability-image -x mask-image -o output-image\n", exe);
  printf("          -m modes -t trend -d threshold_variability -r threshold_residual -n confirmation-number\n");
  printf("          input-image(s)\n");
  printf("\n");
  printf("  -j = number of CPUs to use\n");
  printf("\n");
  printf("  -x = mask image\n");
  printf("  -c = path to coefficients\n");
  printf("  -s = path to statistics\n");
  printf("  -o = output file (.tif)\n");
  printf("\n");  
  printf("  -m = number of modes for fitting the harmonic model (1-3)\n");
  printf("  -t = use trend coefficient when fitting the harmonic model? (0 = no, 1 = yes)\n");
  printf("  -d = standard deviation threshold\n");
  printf("  -r = minimum residuum threshold\n");
  printf("  -n = number of consecutive observations to detect disturbance event\n");
  printf("\n");
  printf("  input-image(s) = input images to compute disturbances from\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 10;

  opterr = 0;

  while ((opt = getopt(argc, argv, "j:c:s:o:m:t:d:r:n:x:")) != -1){
    switch(opt){
      case 'j':
        args->n_cpus = atoi(optarg);
        received_n++;
        break;
      case 'c':
        copy_string(args->path_coefficients, STRLEN, optarg); 
        received_n++;
        break;
      case 's':
        copy_string(args->path_variability, STRLEN, optarg);
        received_n++;
        break;
      case 'o':
          copy_string(args->path_output, STRLEN, optarg);
          received_n++;
          break;
      case 'm':
        args->modes = atoi(optarg);
        received_n++;
        break;
      case 't':
        args->trend = atoi(optarg);
        received_n++;
        break;
      case 'd':
        args->threshold_variability = atof(optarg);
        received_n++;
        break;
      case 'r':
        args->threshold_residual = atof(optarg);
        received_n++;
        break;
      case 'n':
        args->confirmation_number = atoi(optarg);
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
    fprintf(stderr, "Not all arguments received.\n");
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
  
  if (!fileexist(args->path_coefficients)){
    fprintf(stderr, "Coefficient file %s does not exist.\n", args->path_coefficients);
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_variability)){
    fprintf(stderr, "Variability file %s does not exist.\n", args->path_variability);
    usage(argv[0], FAILURE);
  }

  if (fileexist(args->path_output)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output);
    usage(argv[0], FAILURE);
  }

  if (args->n_cpus < 1){
    fprintf(stderr, "Number of CPUs must be at least 1.\n");
    usage(argv[0], FAILURE);
  }
  
  if (args->modes != 1 && args->modes != 2 && args->modes != 3){
    fprintf(stderr, "modes must be between 1, 2, or 3.\n");
    usage(argv[0], FAILURE);
  }

  if (args->trend != 0 && args->trend != 1){
    fprintf(stderr, "trend must be 0 (no) or 1 (yes).\n");
    usage(argv[0], FAILURE);
  }
  
  if (args->threshold_variability == 0){
    fprintf(stderr, "variabiloity threshold must be non-zero.\n");
    usage(argv[0], FAILURE);
  }
  if (args->threshold_residual == 0){
    fprintf(stderr, "residual threshold must be non-zero.\n");
    usage(argv[0], FAILURE);
  }
  
  if (args->confirmation_number < 1){
    fprintf(stderr, "confirmation number must be at least 1.\n");
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
image_t coefficients;
image_t disturbance;


  parse_args(argc, argv, &args);

  GDALAllRegister();


  read_image(args.path_mask, NULL, &mask);
  read_image(args.path_coefficients, NULL, &coefficients);
  read_image(args.path_variability, NULL, &variability);
  compare_images(&mask, &coefficients);
  compare_images(&mask, &variability);

  alloc((void**)&input, args.n_images, sizeof(image_t));
  alloc((void**)&dates, args.n_images, sizeof(date_t));

  for (int i=0; i<args.n_images; i++){

    char basename[STRLEN];
    basename_with_ext(args.path_input[i], basename, STRLEN);
    date_from_string(&dates[i], basename);

    read_image(args.path_input[i], NULL, &input[i]);
    compare_images(&coefficients, &input[i]);

    if (i > 0){
      if (dates[i].ce < dates[i-1].ce){
        fprintf(stderr, "Input images must be ordered by date (earliest to latest).\n");
        exit(FAILURE);
      }
      if (dates[i].year != dates[i-1].year){
        fprintf(stderr, "Input images should be from the same year.\n");
        exit(FAILURE);
      }
    }

  }


  copy_image(&variability, &disturbance, 3, SHRT_MIN, args.path_output);

  
  // pre-compute terms for harmonic fitting
  int n_coef = number_of_coefficients(args.modes, args.trend);
  if (n_coef != coefficients.nb){
    fprintf(stderr, "Number of coefficients in coefficient image does not match the number required by modes and trend settings.\n");
    exit(FAILURE);
  }

  float **terms;
  alloc_2D((void***)&terms, args.n_images, n_coef, sizeof(float));
  compute_harmonic_terms(dates, args.n_images, args.modes, args.trend, terms);

  omp_set_num_threads(args.n_cpus);

  #pragma omp parallel shared(args, dates, input, mask, variability, coefficients, disturbance, n_coef, terms) default(none)
  {

  for (int p=0; p<disturbance.nc; p++){

    if (mask.data[0][p] == mask.nodata || mask.data[0][p] == 0) continue;

    if (variability.data[0][p] == variability.nodata) continue;
    if (coefficients.data[0][p] == coefficients.nodata) continue;


    int number = 0, candidate = 0;
    bool confirmed = false;

    for (int i=0; i<args.n_images; i++){

      if (input[i].data[0][p] == input[i].nodata) continue;

      // predict value and compute residual
      float y_pred = predict_harmonic_value(terms[i], &coefficients, p, n_coef, args.modes, args.trend);
      float residual = input[i].data[0][p] - y_pred;


      if (
        args.threshold_residual > 0 && 
        residual > args.threshold_residual &&
        residual > (args.threshold_variability * variability.data[0][p])){
        number++;
      } else if (
        args.threshold_residual < 0 && 
        residual < args.threshold_residual &&
        residual < (args.threshold_variability * variability.data[0][p])){
        number++;
      } else {
        number = 0;
      }

      if (number == 1) candidate = i;
      if (number == args.confirmation_number){
          confirmed = true;
          break;
      }

    }

    if (!confirmed) continue;

    disturbance.data[0][p] = dates[candidate].ce - 1970*365;
    disturbance.data[1][p] = dates[candidate].year;
    disturbance.data[2][p] = dates[candidate].doy;    

  }

   } // end omp parallel

  write_image(&disturbance);

  
  for (int i=0; i<args.n_images; i++){
    free_image(&input[i]);
  }
  free((void*)input);
  free_image(&mask);
  free_image(&variability);
  free_image(&coefficients);
  free((void*)dates);
  free_2D((void**)terms, args.n_images);
  free_2D((void**)args.path_input, args.n_images);
  
  GDALDestroy();

  return SUCCESS; 
}

