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
  int n_images;
  char **path_input;
  char path_output_reference_period[STRLEN];
  char path_output_coefficient[STRLEN];
  int modes;
  int trend;
  int current;
  int fit_end;
  int threshold;
  int confirmation_number;
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -r output-reference-period-image -c output-coefficient-image input-image(s)\n", exe);
  printf("          -m modes -t trend -e end-fitting -s threshold -n confirmation-number -y current-year\n");
  printf("\n");
  printf("  -r = output reference period image (e.g., reference_period.tif)\n");
  printf("  -c = output coefficient image (e.g., coefficient.tif)\n");
  printf("  -m = number of modes for fitting the harmonic model (1-3)\n");
  printf("  -t = use trend coefficient when fitting the harmonic model? (0 = no, 1 = yes)\n");
  printf("  -e = end year for initial fitting (e.g., 2017)\n");
  printf("  -s = threshold for detecting change (e.g., 500)\n");
  printf("  -n = confirmation number for detecting change (e.g., 3)\n");
  printf("  -y = current year (e.g., 2025)\n");
  printf("\n");
  printf("  input-image(s) = input images to compute reference period from\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 8;
  opterr = 0;

  while ((opt = getopt(argc, argv, "r:c:m:t:e:s:n:y:")) != -1){
    switch(opt){
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
      case 'y':
        args->current = atoi(optarg);
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

  if (args->current < 1970 || args->current > 2100){
    fprintf(stderr, "current year must be between 1970 and 2100. Or even better a reasonable year\n");
    usage(argv[0], FAILURE);
  }

  return;
}




int main ( int argc, char *argv[] ){
args_t args;
date_t *dates = NULL;
image_t *input = NULL;
image_t reference_period;
image_t coefficients;

  parse_args(argc, argv, &args);

  GDALAllRegister();

  alloc((void**)&input, args.n_images, sizeof(image_t));
  alloc((void**)&dates, args.n_images, sizeof(date_t));

  for (int i=0; i<args.n_images; i++){

    char basename[STRLEN];
    basename_with_ext(args.path_input[i], basename, STRLEN);
    date_from_string(&dates[i], basename);

    read_image(args.path_input[i], NULL, &input[i]);

    if (i > 0){
      compare_images(&input[0], &input[i]);
      if (dates[i].ce < dates[i-1].ce){
        fprintf(stderr, "Input images must be ordered by date (earliest to latest).\n");
        exit(FAILURE);
      }
    }

  }


  int n_coef = 1 + args.modes * 2 + (args.trend ? 1 : 0);

  // at least have offset and uni-modal frequency
  if (n_coef < 3){
    printf("not enough coefficients for harmonic fitting. "
      "Something is wrong. Abort.\n");
      return FAILURE;
  }
  
  gsl_set_error_handler_off();
  
  
  copy_image(&input[0], &reference_period, 1, SHRT_MIN, args.path_output_reference_period);
  copy_image(&input[0], &coefficients, n_coef, SHRT_MIN, args.path_output_coefficient);


  omp_set_num_threads(64);
  //omp_set_num_threads(omp_get_thread_limit());

  #pragma omp parallel shared(args, dates, input, reference_period, coefficients, n_coef) default(none)
  {

  gsl_vector *c = gsl_vector_alloc(n_coef);
  gsl_matrix *cov = gsl_matrix_alloc(n_coef, n_coef);
  gsl_vector *x_pred = gsl_vector_alloc(n_coef);

  #pragma omp for
  for (int p=0; p<reference_period.nc; p++){

    // initialize images
    for (int b=0; b<coefficients.nb; b++) coefficients.data[b][p] = coefficients.nodata;
    reference_period.data[0][p] = reference_period.nodata;

    bool all_nodata = true;
    for (int i=0; i<args.n_images; i++){
      if (input[i].data[0][p] != input[i].nodata){
        all_nodata = false;
        break;
      }
    }
    if (all_nodata) continue;

    // reset iterators
    int local_end = args.fit_end;
    bool stable = true;

    while (local_end < (args.current - 1) && stable){

      int i_end = 0;
      int n_valid = 0;

      // pre-screen and determine fitting range
      for (int i=i_end; i<args.n_images; i++){

        if (dates[i].year > local_end){
          i_end = i;
          break;
        } else if (input[i].data[0][p] == input[i].nodata){
          continue;
        } else {
          n_valid++;
        }

        if (i == args.n_images - 1) i_end = args.n_images;

      }

      //printf("Pixel %d: fitting period until %d (index %d), n_valid = %d\n", p, local_end, i_end, n_valid);

    //printf("n_valid: %d\n", n_valid);

      if (n_valid < n_coef){
        local_end++;
        continue;
      }


      gsl_matrix *x = gsl_matrix_alloc(n_valid, n_coef);
      gsl_vector *y = gsl_vector_alloc(n_valid);


      for (int i=0, k=0; i<i_end; i++){

        if (input[i].data[0][p] == input[i].nodata) continue;

        int i_coef = 0;

        // offset
        gsl_matrix_set(x, k, i_coef++, 1.0);

        // trend
        if (args.trend) gsl_matrix_set(x, k, i_coef++, input[i].data[0][p]);

        // uni-modal frequency
        if (args.modes >= 1){
          gsl_matrix_set(x, k, i_coef++, cos(2 * M_PI / 365 * dates[i].ce));
          gsl_matrix_set(x, k, i_coef++, sin(2 * M_PI / 365 * dates[i].ce));
        }

        // bi-modal frequency
        if (args.modes >= 2){
          gsl_matrix_set(x, k, i_coef++, cos(4 * M_PI / 365 * dates[i].ce));
          gsl_matrix_set(x, k, i_coef++, sin(4 * M_PI / 365 * dates[i].ce));
        }

        // tri-modal frequency
        if (args.modes >= 3){
          gsl_matrix_set(x, k, i_coef++, cos(6 * M_PI / 365 * dates[i].ce));
          gsl_matrix_set(x, k, i_coef++, sin(6 * M_PI / 365 * dates[i].ce));
        }

        // response variable
        gsl_vector_set(y, k, input[i].data[0][p]);
        k++;

      }

      // Iteratively Reweighted Least Squares (IRLS)
      irls_fit(x, y, c, cov);

      for (int e=i_end, dist_ctr=0; e<args.n_images; e++){

        // only predict for dates within one year after fitting period
        if (dates[e].year > (local_end + 1) ) break;

        if (input[e].data[0][p] == input[e].nodata) continue;

        int i_coef = 0;

        // offset
        gsl_vector_set(x_pred, i_coef++, 1.0);

        // trend
        if (args.trend) gsl_vector_set(x_pred, i_coef++, dates[e].ce);

        // uni-modal frequency
        if (args.modes >= 1){
          gsl_vector_set(x_pred, i_coef++, cos(2 * M_PI / 365 * dates[e].ce));
          gsl_vector_set(x_pred, i_coef++, sin(2 * M_PI / 365 * dates[e].ce));
        }

        // bi-modal frequency
        if (args.modes >= 2){
          gsl_vector_set(x_pred, i_coef++, cos(4 * M_PI / 365 * dates[e].ce));
          gsl_vector_set(x_pred, i_coef++, sin(4 * M_PI / 365 * dates[e].ce));
        }

        // tri-modal frequency
        if (args.modes >= 3){
          gsl_vector_set(x_pred, i_coef++, cos(6 * M_PI / 365 * dates[e].ce));
          gsl_vector_set(x_pred, i_coef++, sin(6 * M_PI / 365 * dates[e].ce));
        }

        double y_pred, y_err;
        gsl_multifit_robust_est(x_pred, c, cov, &y_pred, &y_err);
        double residual = input[e].data[0][p] - y_pred;

        //printf("  Predicting date %d-%d-%d (index %d): observed = %d, predicted = %.2f, residual = %.2f\n",
        //  dates[e].year, dates[e].month, dates[e].day, e, input[e].data[0][p], y_pred, residual);

        if (args.threshold > 0 && residual > args.threshold){
          dist_ctr++;
        } else if (args.threshold < 0 && residual < args.threshold){
          dist_ctr++;
        } else {
          dist_ctr = 0;
        }

        //printf("    disturbance counter = %d\n", dist_ctr);

        if (dist_ctr >= args.confirmation_number){
          //printf("    -> detected disturbance. Stopping fitting period extension.\n");
          stable = false;
          break;
        }

      }

      gsl_matrix_free (x);
      gsl_vector_free (y);

      //printf("    stil stable, adding one year to fitting period.\n");
      local_end++;

    }


    // save coefficients and reference period
    reference_period.data[0][p] = local_end;
    for (int b=0; b<n_coef; b++){
      coefficients.data[b][p] = (short)gsl_vector_get(c, b);
    }

  }

  gsl_vector_free (c);
  gsl_matrix_free (cov);
  gsl_vector_free (x_pred);
  gsl_set_error_handler(NULL);
  
  } // end omp parallel region

  write_image(&reference_period);
  write_image(&coefficients);

  
  for (int i=0; i<args.n_images; i++){
    free_image(&input[i]);
  }
  free((void*)input);
  free_image(&reference_period);
  free_image(&coefficients);
  free((void*)dates);
  free_2D((void**)args.path_input, args.n_images);
  
  GDALDestroy();

  exit(SUCCESS);
}
