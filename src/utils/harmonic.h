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

/** GNU Scientific Library (GSL) **/
#include <gsl/gsl_multifit.h> // Linear Least Squares Fitting

#ifdef __cplusplus
extern "C" {
#endif


int irls_fit(const gsl_matrix *X, const gsl_vector *y, gsl_vector *c, gsl_matrix *cov);

#ifdef __cplusplus
}
#endif

#endif

