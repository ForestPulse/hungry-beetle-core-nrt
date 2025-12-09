/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Harmonic model header
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#ifndef HARMONIC_H
#define HARMONIC_H

#include <stdio.h>    // core input and output functions
#include <stdlib.h>   // standard general utilities library
#include <string.h>   // string handling functions
#include <stdbool.h>  // boolean data type

#include "const.h"
#include "date.h"
#include "image_io.h"

/** GNU Scientific Library (GSL) **/
#include <gsl/gsl_multifit.h> // Linear Least Squares Fitting

#ifdef __cplusplus
extern "C" {
#endif

#define _COEF_SCALE_ 10.0f

int number_of_coefficients(int modes, int trend);
void compute_harmonic_terms(date_t *dates, int n_dates, int modes, int trend, float **terms);
float predict_harmonic_value(float *x, image_t *coefficients, int pixel, int n_coef, int modes, int trend);
int irls_fit(const gsl_matrix *X, const gsl_vector *y, gsl_vector *c, gsl_matrix *cov);

#ifdef __cplusplus
}
#endif

#endif

