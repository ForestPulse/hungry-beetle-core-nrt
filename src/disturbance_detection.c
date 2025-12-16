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
#include "args/args_disturbance_detection.h"


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

  int n_pixels = 0, n_alert = 0, n_reversed = 0, n_detected = 0;

  #pragma omp parallel shared(args, dates, input, mask, variability, coefficients, disturbance, n_coef, terms) reduction(+: n_pixels, n_alert, n_reversed, n_detected) default(none)
  {

  #pragma omp for
  for (int p=0; p<disturbance.nc; p++){
//if (p != 1837*disturbance.ny + 1385) continue;

    if (mask.data[0][p] == mask.nodata || mask.data[0][p] == 0) continue;

    if (variability.data[1][p] == variability.nodata) continue;
    if (coefficients.data[1][p] == coefficients.nodata) continue;

    n_pixels++;

    //printf("pixel %d is valid, proceed:\n", p);
    //printf("  Variability: %.2f\n", (float)variability.data[0][p]);
    //for (int b=0; b<coefficients.nb; b++){
    //  printf("  Coefficient %d: %.2f\n", b, (float)coefficients.data[b][p] / _COEF_SCALE_);
    //}
    //printf("  Number of images: %d\n", args.n_images);

    int alert_number = 0, candidate = 0;
    int revert_number = 0;
    bool confirmed = false;

    for (int i=0; i<args.n_images; i++){

      if (input[i].data[0][p] == input[i].nodata) continue;

      // predict value and compute residual
      float y_pred = predict_harmonic_value(terms[i], &coefficients, p, n_coef, args.modes, args.trend);
      float residual = input[i].data[0][p] - y_pred;

      //printf("Pixel %d, Date %d-%03d, ce %d, index %d: Observed = %.2f, Predicted = %.2f, Residual = %.2f\n", 
      //  p, dates[i].year, dates[i].doy, dates[i].ce, i, (float)input[i].data[0][p], y_pred, residual);

      if (!confirmed){
        // not yet confirmed, check and potentially raise alert
        //printf("  Not yet confirmed.\n");
        if (
          args.threshold_residual > 0 && 
          residual > args.threshold_residual &&
          residual > (args.threshold_variability * variability.data[1][p])){
          alert_number++;
          //printf(" -> alert %d raised. Residual: %f, variability: %d\n", alert_number, residual, variability.data[1][p]);
        } else if (
          args.threshold_residual < 0 && 
          residual < args.threshold_residual &&
          residual < (args.threshold_variability * variability.data[1][p])){
          alert_number++;
          //printf(" -> alert %d raised. Residual: %f, variability: %d\n", alert_number, residual, variability.data[1][p]);
        } else {
          alert_number = 0;
        }

        if (alert_number == 1) candidate = i;
        if (alert_number == args.confirmation_number){
          confirmed = true;
          n_alert++;
          //break;
        }
        //printf("  candidate: %d, Alert counter: %d, revert counter: %d\n", candidate, alert_number, revert_number);
      } else {
        // already confirmed, check for reversion
        //printf("  Already confirmed.\n");
        if (
          args.threshold_residual > 0 && 
          residual < (args.threshold_residual / 2)){
          revert_number++;
        } else if (
          args.threshold_residual < 0 && 
          residual > (args.threshold_residual / 2)){
          revert_number++;
        } else {
          revert_number = 0;
        }

        if (revert_number == args.confirmation_number){
          // disturbance reverted
          confirmed = false;
          n_reversed++;
          alert_number = 0;
          revert_number = 0;
        }
        //printf("  candidate: %d, Alert counter: %d, revert counter: %d\n", candidate, alert_number, revert_number);
      }

    }

    if (!confirmed) continue;

    n_detected++;

    disturbance.data[0][p] = dates[candidate].ce - 1970*365;
    disturbance.data[1][p] = dates[candidate].year;
    disturbance.data[2][p] = dates[candidate].doy;    

  }

   } // end omp parallel

  printf("Alerts were produced for %d out of %d pixels, i.e. %.2f%%.\n", n_alert, n_pixels, 100.0 * n_alert / n_pixels);
  printf("Alerts were reversed for %d out of %d pixels, i.e. %.2f%%.\n", n_reversed, n_pixels, 100.0 * n_reversed / n_pixels);
  printf("Disturbances were detected for %d out of %d pixels, i.e. %.2f%%.\n", n_detected, n_pixels, 100.0 * n_detected / n_pixels);

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

