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
#include "args/args_reference_period.h"


int main ( int argc, char *argv[] ){
args_t args;
date_t *dates = NULL;
image_t *input = NULL;
image_t mask;
image_t input_reference_period;
image_t output_reference_period;
image_t input_coefficients;
image_t output_coefficients;


  parse_args(argc, argv, &args);

  GDALAllRegister();

  read_image(args.path_mask, NULL, &mask);
  read_image(args.path_input_coefficient, NULL, &input_coefficients);
  read_image(args.path_input_reference_period, NULL, &input_reference_period);

  compare_images(&mask, &input_coefficients);
  compare_images(&mask, &input_reference_period);

  alloc((void**)&input, args.n_images, sizeof(image_t));
  alloc((void**)&dates, args.n_images, sizeof(date_t));

  int i_break = -1;

  for (int i=0; i<args.n_images; i++){

    char basename[STRLEN];
    basename_with_ext(args.path_input[i], basename, STRLEN);
    date_from_string(&dates[i], basename);

    read_image(args.path_input[i], NULL, &input[i]);
    compare_images(&mask, &input[i]);

    if (dates[i].year == args.year && i_break < 0) i_break = i;

    if (dates[i].year > args.year){
      fprintf(stderr, "Input images must not include data from year %d or later.\n", args.year + 1);
      exit(FAILURE);
    }

    if (i > 0){
      if (dates[i].ce < dates[i-1].ce){
        fprintf(stderr, "Input images must be ordered by date (earliest to latest).\n");
        exit(FAILURE);
      }
    }

  }

  if (i_break < 0){
    fprintf(stderr, "No input image from year %d is given.\n", args.year);
    exit(FAILURE);
  }


  bool initial = false;
  int n_coef = number_of_coefficients(args.modes, args.trend);
  
  if (input_coefficients.nb == 1){
    initial = true;
    free_image(&input_coefficients);
    copy_image(&input[0], &input_coefficients, n_coef, SHRT_MIN, args.path_input_coefficient);  
  }
  
  copy_image(&input[0], &output_reference_period, 2, SHRT_MIN, args.path_output_reference_period);
  copy_image(&input[0], &output_coefficients, n_coef, SHRT_MIN, args.path_output_coefficient);
  
  // pre-compute terms for harmonic fitting
  float **terms;
  alloc_2D((void***)&terms, args.n_images, n_coef, sizeof(float));
  compute_harmonic_terms(dates, args.n_images, args.modes, args.trend, terms);
  
  
  omp_set_num_threads(args.n_cpus);
  
  int n_fit = 0, n_current_anomaly = 0, n_previous_anomaly = 0, n_pixels = 0;

  #pragma omp parallel shared(args, initial, dates, i_break, input, mask, terms, output_reference_period, output_coefficients, input_reference_period, input_coefficients, n_coef) reduction(+: n_fit, n_current_anomaly, n_previous_anomaly, n_pixels) default(none)
  {
    
    gsl_vector *coef = gsl_vector_alloc(n_coef);
    gsl_matrix *cov = gsl_matrix_alloc(n_coef, n_coef);
    gsl_vector *x_pred = gsl_vector_alloc(n_coef);
    gsl_set_error_handler_off();


  #pragma omp for
  for (int p=0; p<output_reference_period.nc; p++){
//if (p != 1837*output_reference_period.ny + 1385) continue;
    
    //printf("Processing pixel %d...\n", p);
    //printf("  determine if reference period will be extended until %d at index %d\n", periods[n_periods - 1][0], periods[n_periods - 1][1]);
    //printf("  fitting period of previous iterations ended in %d\n", input_reference_period.data[0][p]);

    // initialize images
    for (int b=0; b<output_coefficients.nb; b++) output_coefficients.data[b][p] = output_coefficients.nodata;
    for (int b=0; b<output_reference_period.nb; b++) output_reference_period.data[b][p] = output_reference_period.nodata;

    // check mask
    if (mask.data[0][p] == mask.nodata || mask.data[0][p] == 0) continue;

    n_pixels++;

    // we already ended the reference period in a previous iteration -> no need to fit again
    // if we are working in 2018, and the reference period already ended in 2016 or earlier, just copy previous results
    if (!initial && input_reference_period.data[0][p] < (args.year - 1)){
      // safety check (should not happen)
      if (input_reference_period.data[0][p] < 1900){
        printf("Warning: pixel %d has invalid reference period year - should not happen - %d.\n", p, input_reference_period.data[0][p]);
        continue;
      } 
      //printf("Pixel %d: reference period already ended in year %d, copy previous results.\n", p, input_reference_period.data[0][p]);
      for (int b=0; b<output_coefficients.nb; b++) output_coefficients.data[b][p] = input_coefficients.data[b][p];
      for (int b=0; b<output_reference_period.nb; b++) output_reference_period.data[b][p] = input_reference_period.data[b][p];
      n_previous_anomaly++;
      continue;
    }


    bool stable = true;

    // check for anomalies in the period after the previous reference period until the current year
    if (!initial){

      for (int i=i_break, anomaly_counter=0; i<args.n_images; i++){

        if (input[i].data[0][p] == input[i].nodata) continue;

        float y_pred = predict_harmonic_value(terms[i], &input_coefficients, p, n_coef, args.modes, args.trend);
        float residual = input[i].data[0][p] - y_pred;

        //printf("  Predicting date %d-%d-%d (index %d): observed = %d, predicted = %.2f, residual = %.2f\n",
        //  dates[i].year, dates[i].month, dates[i].day, i, input[i].data[0][p], y_pred, residual);

        if (args.threshold > 0 && residual > args.threshold){
          anomaly_counter++;
        } else if (args.threshold < 0 && residual < args.threshold){
          anomaly_counter++;
        } else {
          anomaly_counter = 0;
        }

        //printf("    anomaly counter = %d\n", anomaly_counter);

        // detected anomaly, stop extending reference period
        if (anomaly_counter >= args.confirmation_number){
          //printf("    -> detected anomaly. Stop the fitting period extension.\n");
          stable = false;
          for (int b=0; b<output_coefficients.nb; b++) output_coefficients.data[b][p] = input_coefficients.data[b][p];
          for (int b=0; b<output_reference_period.nb; b++) output_reference_period.data[b][p] = input_reference_period.data[b][p];
          n_current_anomaly++;
          break;
        }

      }

    }

    // if still stable or initial run, extend fitting period to the whole time frame
    if (stable || initial){

      int n_valid = 0;
      for (int i=0; i<args.n_images; i++){
        if (input[i].data[0][p] != input[i].nodata) n_valid++;
      }

      // not enough valid observations to fit the harmonic model
      if (n_valid > n_coef){

        // printf("  Fit a new model until year %d (index %d) with %d valid observations.\n", 
        //  periods[fit_period][0], periods[fit_period][1], n_valid);

        gsl_matrix *x = gsl_matrix_alloc(n_valid, n_coef);
        gsl_vector *y = gsl_vector_alloc(n_valid);

        for (int i=0, k=0; i<args.n_images; i++){

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
        double sd = irls_fit(x, y, coef, cov);

        // update coefficients image
        for (int b=0; b<n_coef; b++){
          //printf("Pixel %d, Coefficient %d: %.2f\n", p, b, gsl_vector_get(coef, b));
          output_coefficients.data[b][p] = (short)(gsl_vector_get(coef, b) * _COEF_SCALE_);
        }

        output_reference_period.data[0][p] = args.year; // extended until current year
        output_reference_period.data[1][p] = (short)sd; // extended until current year

        gsl_matrix_free(x);
        gsl_vector_free(y);

        n_fit++;

      }


    }

  }

  gsl_vector_free(coef);
  gsl_matrix_free(cov);
  gsl_vector_free(x_pred);
  gsl_set_error_handler(NULL);
  
  } // end omp parallel region

  printf("Fitted new models for %d out of %d pixels, i.e. %.2f%%.\n", n_fit, n_pixels, 100.0 * n_fit / n_pixels);
  printf("Stopped to extend the reference period for %d pixels, i.e. %.2f%%.\n", n_current_anomaly, 100.0 * n_current_anomaly / n_pixels);
  printf("Reference period already ended earlier for %d pixels, i.e. %.2f%%.\n", n_previous_anomaly, 100.0 * n_previous_anomaly / n_pixels);

  write_image(&output_reference_period);
  write_image(&output_coefficients);


  free_2D((void**)terms, args.n_images);
  for (int i=0; i<args.n_images; i++){
    free_image(&input[i]);
  }
  free((void*)input);
  free_image(&mask);
  free_image(&input_reference_period);
  free_image(&output_reference_period);
  free_image(&input_coefficients);
  free_image(&output_coefficients);
  free((void*)dates);
  free_2D((void**)args.path_input, args.n_images);
  
  GDALDestroy();

  exit(SUCCESS);
}
