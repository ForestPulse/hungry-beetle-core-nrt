/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Statistics header
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#ifndef STATS_H
#define STATS_H

#include <stdio.h>   // core input and output functions
#include <stdlib.h>  // standard general utilities library
#include <stdbool.h> // boolean data type
#include <math.h>    // common mathematical functions

#include "const.h"
#include "alloc.h"


#ifdef __cplusplus
extern "C" {
#endif

void covar_recurrence(double   x, double   y, double *mx, double *my, double *vx, double *vy, double *cv, double n);
void cov_recurrence(double   x, double   y, double *mx, double *my, double *cv, double n);
void kurt_recurrence(double   x,    double *mx, double *vx,    double *sx,double *kx, double n);
void skew_recurrence(double   x,    double *mx, double *vx,    double *sx,double n);
void var_recurrence(double   x, double *mx, double *vx, double n);
double kurtosis(double var, double kurt, double n);
double skewness(double var, double skew, double n);
double variance(double var, double n);
double standdev(double var, double n);
double covariance(double cov, double n);

#ifdef __cplusplus
}
#endif

#endif

