#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/** Geospatial Data Abstraction Library (GDAL) **/
#include "gdal.h"       // public (C callable) GDAL entry points
#include "cpl_conv.h"   // various convenience functions for CPL
#include "cpl_string.h" // various convenience functions for strings

/** OpenMP **/
#include <omp.h> // multi-platform shared memory multiprocessing

#include "utils/alloc.h"
#include "utils/const.h"
#include "utils/date.h"
#include "utils/dir.h"
#include "utils/image_io.h"
#include "utils/string.h"
#include "utils/stats.h"
#include "args/args_combine_disturbances.h"


int main ( int argc, char *argv[] ){
args_t args;
image_t *input = NULL;
image_t output;


  parse_args(argc, argv, &args);

  GDALAllRegister();

  
  alloc((void**)&input, args.n_images, sizeof(image_t));
  
  for (int i=0; i<args.n_images; i++){
    read_image(args.path_input[i], NULL, &input[i]);
    if (i > 0) compare_images(&input[0], &input[i]);
  }


  copy_image(&input[0], &output, input[0].nb, input[0].nodata, args.path_output);

  
  omp_set_num_threads(args.n_cpus);

  #pragma omp parallel shared(input, output, args) default(none)
  {

  #pragma omp for
  for (int p=0; p<output.nc; p++){

    output.data[0][p] = output.nodata;

    for (int i=0; i<args.n_images; i++){
      for (int b=0; b<input[i].nb; b++){
        if (input[i].data[b][p] != input[i].nodata && input[i].data[b][p] > 0) output.data[b][p] = input[i].data[b][p];
      }
    }

  }

  } // end omp parallel region

  write_image(&output);

  for (int i=0; i<args.n_images; i++){
    free_image(&input[i]);
  }
  free((void*)input);
  free_image(&output);
  free_2D((void**)args.path_input, args.n_images);
  
  GDALDestroy();

  exit(SUCCESS);
}
