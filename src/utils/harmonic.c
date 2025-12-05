/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This file contains functions for fitting harmonic models to time series
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#include "harmonic.h"



int irls_fit(const gsl_matrix *X, const gsl_vector *y, gsl_vector *c, gsl_matrix *cov){


  gsl_multifit_robust_workspace *work = 
    gsl_multifit_robust_alloc(gsl_multifit_robust_bisquare, X->size1, X->size2);

  //gsl_multifit_robust_maxiter(1000, work);

  int s = gsl_multifit_robust(X, y, c, cov, work);

  //if (s == GSL_EMAXITER) printf("max iter reached.\n");

  gsl_multifit_robust_free(work);

  return s;
}

