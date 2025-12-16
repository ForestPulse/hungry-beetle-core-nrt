#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/** Geospatial Data Abstraction Library (GDAL) **/
#include "gdal.h"       // public (C callable) GDAL entry points
#include "cpl_conv.h"   // various convenience functions for CPL
#include "cpl_string.h" // various convenience functions for strings


#include "utils/alloc.h"
#include "utils/const.h"
#include "utils/dir.h"
#include "utils/quality.h"
#include "utils/image_io.h"
#include "utils/string.h"
#include "args/args_spectral_index.h"


// bands to read
static const int BAND_NUMBERS[] = { 8, 9, 10 };
static const float WAVELENGTHS[] = { 0.864, 1.609, 2.202 };
#define N_BANDS (sizeof(BAND_NUMBERS)/sizeof(BAND_NUMBERS[0]))



int main ( int argc, char *argv[] ){
args_t args;
image_t reflectance;
image_t quality;
image_t mask;
image_t index;
bandlist_t bands;

  parse_args(argc, argv, &args);

  GDALAllRegister();

  bands.n = N_BANDS;
  bands.number = (int*)BAND_NUMBERS;
  bands.wavelengths = (float*)WAVELENGTHS;

  read_image(args.path_reflectance, &bands, &reflectance);
  read_image(args.path_quality, NULL, &quality);
  read_image(args.path_mask, NULL, &mask);

  compare_images(&reflectance, &quality);
  compare_images(&reflectance, &mask);

  copy_image(&reflectance, &index, 1, SHRT_MIN, args.path_output);

  for (int p=0; p<index.nc; p++){

    if (quality.data[0][p] == quality.nodata ||
        reflectance.data[0][p] == reflectance.nodata ||
        reflectance.data[1][p] == reflectance.nodata ||
        reflectance.data[2][p] == reflectance.nodata ||
        mask.data[0][p] == mask.nodata ||
        mask.data[0][p] == 0 ||
        !use_this_pixel(quality.data[0][p])){
      index.data[0][p] = index.nodata;
      continue;
    }

    float interpolated = 
      (reflectance.data[0][p] * (bands.wavelengths[2] - bands.wavelengths[1]) + 
       reflectance.data[2][p] * (bands.wavelengths[1] - bands.wavelengths[0])) / 
      (bands.wavelengths[2] - bands.wavelengths[0]);

    index.data[0][p] = (short)(reflectance.data[1][p] - interpolated);
  
  }

  write_image(&index);

  free_image(&reflectance);
  free_image(&quality);
  free_image(&mask);
  free_image(&index);

  GDALDestroy();

  exit(SUCCESS);
}
