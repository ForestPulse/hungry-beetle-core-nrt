/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This file contains functions for fitting harmonic models to time series
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#include "harmonic.h"


int number_of_coefficients(int modes, int trend){

  int n_coef = 1 + modes * 2 + (trend ? 1 : 0);
  
  // at least have offset and uni-modal frequency
  if (n_coef < 3){
    printf("not enough coefficients for harmonic fitting. "
      "Something is wrong. Abort.\n");
      return FAILURE;
  }

  return n_coef;
}

void compute_harmonic_terms(date_t *dates, int n_dates, int modes, int trend, float **terms){


  for (int i=0; i<n_dates; i++){

    int coef = 0;

    terms[i][coef++] = 1.0; // intercept

    if (trend){
      terms[i][coef++] = (float)(dates[i].ce); // trend
    }

    // uni-modal frequency
    if (modes >= 1){
      terms[i][coef++] = cos(2 * M_PI / 365 * dates[i].ce);
      terms[i][coef++] = sin(2 * M_PI / 365 * dates[i].ce);
    }

    // bi-modal frequency
    if (modes >= 2){
      terms[i][coef++] = cos(4 * M_PI / 365 * dates[i].ce);
      terms[i][coef++] = sin(4 * M_PI / 365 * dates[i].ce);
    }

    // tri-modal frequency
    if (modes >= 3){
      terms[i][coef++] = cos(6 * M_PI / 365 * dates[i].ce);
      terms[i][coef++] = sin(6 * M_PI / 365 * dates[i].ce);
    }

  }

  return;
}


float predict_harmonic_value(float *x, image_t *coefficients, int pixel, int n_coef, int modes, int trend){

  int coef = 0;

  // offset
  float y_pred = x[coef++];

  // trend
  if (trend){
    y_pred += x[coef] * coefficients->data[coef][pixel] / _COEF_SCALE_; coef++;
  } 

  // uni-modal frequency
  if (modes >= 1){
    y_pred += x[coef] * coefficients->data[coef][pixel] / _COEF_SCALE_; coef++;
    y_pred += x[coef] * coefficients->data[coef][pixel] / _COEF_SCALE_; coef++;
  }

  // bi-modal frequency
  if (modes >= 2){
    y_pred += x[coef] * coefficients->data[coef][pixel] / _COEF_SCALE_; coef++;
    y_pred += x[coef] * coefficients->data[coef][pixel] / _COEF_SCALE_; coef++;
  }

  // tri-modal frequency
  if (modes >= 3){
    y_pred += x[coef] * coefficients->data[coef][pixel] / _COEF_SCALE_; coef++;
    y_pred += x[coef] * coefficients->data[coef][pixel] / _COEF_SCALE_; coef++;
  }

  return y_pred;
}


int irls_fit(const gsl_matrix *X, const gsl_vector *y, gsl_vector *c, gsl_matrix *cov){

  gsl_multifit_robust_workspace *work = 
    gsl_multifit_robust_alloc(gsl_multifit_robust_bisquare, X->size1, X->size2);

  //gsl_multifit_robust_maxiter(1000, work);

  int s = gsl_multifit_robust(X, y, c, cov, work);

  //if (s == GSL_EMAXITER) printf("max iter reached.\n");

  gsl_multifit_robust_free(work);

  return s;
}

