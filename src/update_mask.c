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
#include "args/args_update_mask.h"






int main ( int argc, char *argv[] ){
args_t args;
image_t disturbance;
image_t mask;
image_t output;


  parse_args(argc, argv, &args);

  GDALAllRegister();

  read_image(args.path_disturbance, NULL, &disturbance);
  read_image(args.path_mask, NULL, &mask);
  compare_images(&disturbance, &mask);
  
  copy_image(&disturbance, &output, 1, SHRT_MIN, args.path_output);

  for (int p=0; p<output.nc; p++){

    output.data[0][p] = mask.data[0][p];

    if (mask.data[0][p] == mask.nodata || mask.data[0][p] == 0 || 
        disturbance.data[0][p] == disturbance.nodata) continue;

    if (disturbance.data[0][p] > 0) output.data[0][p] = 0;
 
  }

  write_image(&output);

  free_image(&disturbance);
  free_image(&mask);
  free_image(&output);

  GDALDestroy();

  exit(SUCCESS);
}
