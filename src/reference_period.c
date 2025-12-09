#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/** Geospatial Data Abstraction Library (GDAL) **/
#include "gdal.h"       // public (C callable) GDAL entry points
#include "cpl_conv.h"   // various convenience functions for CPL
#include "cpl_string.h" // various convenience functions for strings

/** GNU Scientific Library (GSL) **/
#include <gsl/gsl_multifit.h> // Linear Least Squares Fitting

/** OpenMP **/
#include <omp.h> // multi-platform shared memory multiprocessing

#include "utils/alloc.h"
#include "utils/const.h"
#include "utils/date.h"
#include "utils/dir.h"
#include "utils/harmonic.h"
#include "utils/image_io.h"
#include "utils/string.h"
#include "utils/stats.h"

typedef struct {
  int n_cpus;
  int n_images;
  char **path_input;
  char path_mask[STRLEN];
  char path_output_reference_period[STRLEN];
  char path_output_coefficient[STRLEN];
  int modes;
  int trend;
  int fit_end;
  int threshold;
  int confirmation_number;
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -j cpus -x mask-image -r output-reference-period-image -c output-coefficient-image\n", exe);
  printf("          -m modes -t trend -e end-fitting -s threshold -n confirmation-number input-image(s)\n");
  printf("\n");
  printf("  -j = number of CPUs to use\n");
  printf("\n");
  printf("  -x = mask image\n");
  printf("  -r = output reference period image (e.g., reference_period.tif)\n");
  printf("  -c = output coefficient image (e.g., coefficient.tif)\n");
  printf("\n");
  printf("  -m = number of modes for fitting the harmonic model (1-3)\n");
  printf("  -t = use trend coefficient when fitting the harmonic model? (0 = no, 1 = yes)\n");
  printf("  -e = end year for initial fitting (e.g., 2017)\n");
  printf("  -s = threshold for detecting change (e.g., 500)\n");
  printf("  -n = confirmation number for detecting change (e.g., 3)\n");
  printf("\n");
  printf("  input-image(s) = input images to compute reference period from\n");
  printf("                   images must be ordered by date (earliest to latest)\n");
  printf("                   no image from this year should be included!\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 9;
  opterr = 0;

  while ((opt = getopt(argc, argv, "j:x:r:c:m:t:e:s:n:")) != -1){
    switch(opt){
      case 'j':
        args->n_cpus = atoi(optarg);
        received_n++;
        break;
      case 'x':
        copy_string(args->path_mask, STRLEN, optarg);
        received_n++;
        break;
      case 'r':
        copy_string(args->path_output_reference_period, STRLEN, optarg);
        received_n++;
        break;
      case 'c':
        copy_string(args->path_output_coefficient, STRLEN, optarg);
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
      case 'e':
        args->fit_end = atoi(optarg);
        received_n++;
        break;
      case 's':
        args->threshold = atoi(optarg);
        received_n++;
        break;
      case 'n':
        args->confirmation_number = atoi(optarg);
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
  
  if (fileexist(args->path_output_reference_period)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output_reference_period);
    usage(argv[0], FAILURE);
  }

  if (fileexist(args->path_output_coefficient)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output_coefficient);
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

  if (args->confirmation_number < 1){
    fprintf(stderr, "confirmation number must be at least 1.\n");
    usage(argv[0], FAILURE);
  }

  if (args->threshold == 0){
    fprintf(stderr, "threshold must be non-zero.\n");
    usage(argv[0], FAILURE);
  }

  if (args->fit_end < 1970 || args->fit_end > 2100){
    fprintf(stderr, "end fitting year must be between 1970 and 2100. Or even better a reasonable year\n");
    usage(argv[0], FAILURE);
  }

  return;
}




int main ( int argc, char *argv[] ){
args_t args;
date_t *dates = NULL;
image_t *input = NULL;
image_t mask;
image_t reference_period;
image_t coefficients;


  parse_args(argc, argv, &args);

  GDALAllRegister();

  read_image(args.path_mask, NULL, &mask);

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

  int n_periods = dates[args.n_images - 1].year - args.fit_end + 1; // including 1st and next year
  int **periods = NULL;
  alloc_2D((void***)&periods, n_periods, 2, sizeof(int));
  for (int i=1, period=0; i<args.n_images; i++){
    if (dates[i].year > dates[i-1].year && dates[i].year > args.fit_end){
      periods[period][0] = dates[i].year-1;
      periods[period][1] = i;
      period++;
    }
  }
  periods[n_periods - 1][0] = dates[args.n_images - 1].year;
  periods[n_periods - 1][1] = args.n_images;

  /** Example
  9 fitting periods in time series
  Fitting period 0 ends in year 2017 below index 59
  Fitting period 1 ends in year 2018 below index 119
  Fitting period 2 ends in year 2019 below index 170
  Fitting period 3 ends in year 2020 below index 236
  Fitting period 4 ends in year 2021 below index 280
  Fitting period 5 ends in year 2022 below index 339
  Fitting period 6 ends in year 2023 below index 385
  Fitting period 7 ends in year 2024 below index 419
  Fitting period 8 ends in year 2025 below index 473
  **/

  printf("%d fitting periods in time series\n", n_periods);
  for (int period=0; period<n_periods; period++){
    printf("Fitting period %d ends in year %d below index %d\n", period, periods[period][0], periods[period][1]);
  }

  
  int n_coef = number_of_coefficients(args.modes, args.trend);
  
  copy_image(&input[0], &reference_period, 1, SHRT_MIN, args.path_output_reference_period);
  copy_image(&input[0], &coefficients, n_coef, SHRT_MIN, args.path_output_coefficient);
  
  // pre-compute terms for harmonic fitting
  float **terms;
  alloc_2D((void***)&terms, args.n_images, n_coef, sizeof(float));
  compute_harmonic_terms(dates, args.n_images, args.modes, args.trend, terms);
  
  gsl_set_error_handler_off();

  omp_set_num_threads(args.n_cpus);

  #pragma omp parallel shared(args, dates, periods, n_periods, input, mask, terms, reference_period, coefficients, n_coef) default(none)
  {

  gsl_vector *c = gsl_vector_alloc(n_coef);
  gsl_matrix *cov = gsl_matrix_alloc(n_coef, n_coef);
  gsl_vector *x_pred = gsl_vector_alloc(n_coef);


  #pragma omp for
  for (int p=0; p<reference_period.nc; p++){
//if (p != 503971) continue;
    // initialize images
    for (int b=0; b<coefficients.nb; b++) coefficients.data[b][p] = coefficients.nodata;
    reference_period.data[0][p] = reference_period.nodata;

    if (mask.data[0][p] == mask.nodata || mask.data[0][p] == 0) continue;

    bool all_nodata = true;
    for (int i=0; i<args.n_images; i++){
      if (input[i].data[0][p] != input[i].nodata){
        all_nodata = false;
        break;
      }
    }
    if (all_nodata) continue;


    // reset iterators
    int fit_period = 0;

    while (fit_period < n_periods){

      int n_valid = 0;

      // pre-screen number of valid observations in fitting period
      for (int i=0; i<periods[fit_period][1]; i++){
        if (input[i].data[0][p] != input[i].nodata) n_valid++;
      }
      //printf("n_valid: %d\n", n_valid);

      if (n_valid < n_coef){
        fit_period++;
        continue;
      }


      gsl_matrix *x = gsl_matrix_alloc(n_valid, n_coef);
      gsl_vector *y = gsl_vector_alloc(n_valid);


      for (int i=0, k=0; i<periods[fit_period][1]; i++){

        if (input[i].data[0][p] == input[i].nodata) continue;

        // explanatory variables
        for (int coef=0; coef<n_coef; coef++){
          gsl_matrix_set(x, k, coef, terms[i][coef]);
        }

        // response variable
        gsl_vector_set(y, k, input[i].data[0][p]);
        k++;

      }

      // Iteratively Reweighted Least Squares (IRLS)
      irls_fit(x, y, c, cov);

      // reached the end of the time series, no data left to predict and compare
      if (fit_period == (n_periods-1)) break; 

      int predict_period = fit_period + 1;
      bool stable = true;

      // predict and check for disturbance in subsequent fitting periods
      for (int e=periods[fit_period][1], dist_ctr=0; e<args.n_images; e++){

        if (input[e].data[0][p] == input[e].nodata) continue;

        if (e >= periods[predict_period][1]) predict_period++;
 
        // explanatory variables
        for (int coef=0; coef<n_coef; coef++){
          gsl_vector_set(x_pred, coef, terms[e][coef]);
        }

        // predict value and compute residual
        double y_pred, y_err;
        gsl_multifit_robust_est(x_pred, c, cov, &y_pred, &y_err);
        double residual = input[e].data[0][p] - y_pred;

        //printf("  Predicting date %d-%d-%d (index %d): observed = %d, predicted = %.2f, residual = %.2f\n",
        //  dates[e].year, dates[e].month, dates[e].day, e, input[e].data[0][p], y_pred, residual);
        //printf("  Fitting period %d, predicting period %d\n", fit_period, predict_period);

        if (args.threshold > 0 && residual > args.threshold){
          dist_ctr++;
        } else if (args.threshold < 0 && residual < args.threshold){
          dist_ctr++;
        } else {
          dist_ctr = 0;
        }

        //printf("    anomaly counter = %d\n", dist_ctr);

        if (dist_ctr >= args.confirmation_number){
          //printf("    -> detected anomaly. Stop the fitting period extension.\n");
          stable = false;
          break;
        }

      }

      gsl_matrix_free(x);
      gsl_vector_free(y);

      if (stable){
        // if still stable, extend fitting period to whole time frame, then fit once more
        fit_period = n_periods - 1;
      } else if (predict_period == (fit_period + 1)){
        // if instability detected in the first subsequent fitting period, stop the fitting process
        break;
      } else {
        // otherwise, fit again up to the fitting period before instability detected
        fit_period = predict_period - 1;
      }

    }


    // save coefficients and reference period
    reference_period.data[0][p] = periods[fit_period][0]; // last stable year
    for (int b=0; b<n_coef; b++){
      //printf("Pixel %d, Coefficient %d: %.2f\n", p, b, gsl_vector_get(c, b));
      coefficients.data[b][p] = (short)(gsl_vector_get(c, b) * _COEF_SCALE_);
    }

  }

  gsl_vector_free(c);
  gsl_matrix_free(cov);
  gsl_vector_free(x_pred);
  gsl_set_error_handler(NULL);
  
  } // end omp parallel region

  write_image(&reference_period);
  write_image(&coefficients);


  free_2D((void**)terms, args.n_images);
  for (int i=0; i<args.n_images; i++){
    free_image(&input[i]);
  }
  free((void*)input);
  free_image(&mask);
  free_image(&reference_period);
  free_image(&coefficients);
  free((void*)dates);
  free_2D((void**)args.path_input, args.n_images);
  
  GDALDestroy();

  exit(SUCCESS);
}
