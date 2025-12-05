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


// bands to read
static const int BAND_NUMBERS[] = { 8, 9, 10 };
static const float WAVELENGTHS[] = { 0.864, 1.609, 2.202 };
#define N_BANDS (sizeof(BAND_NUMBERS)/sizeof(BAND_NUMBERS[0]))

typedef struct {
  char path_reflectance[STRLEN];
  char path_quality[STRLEN];
  char path_mask[STRLEN];
  char path_output[STRLEN];
  char index[STRLEN];
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -r reflectance-image -q quality-image -m mask-image -o output-image\n", exe);
  printf("\n");
  printf("  -r = reflectance image, FORCE BOA image, either Sentinel-2 or Landsat\n");
  printf("  -q = quality image, FORCE QAI image\n");
  printf("  -m = mask image\n");
  printf("  -o = output image\n");
  printf("\n");
  printf("  The spectral index to compute is currently fixed to continuum-removed SWIR1.\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 4;
  opterr = 0;

  while ((opt = getopt(argc, argv, "r:q:m:o:")) != -1){
    switch(opt){
      case 'r':
        copy_string(args->path_reflectance, STRLEN, optarg);
        received_n++;
        break;
      case 'q':
        copy_string(args->path_quality, STRLEN, optarg);
        received_n++;
        break;
      case 'm':
        copy_string(args->path_mask, STRLEN, optarg);
        received_n++;
        break;
      case 'o':
        copy_string(args->path_output, STRLEN, optarg);
        received_n++;
        break;
      case '?':
        if (isprint(optopt)){
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        }
        usage(argv[0], FAILURE);
      default:
        fprintf(stderr, "Error parsing arguments.\n");
        usage(argv[0], FAILURE);
    }
  }

  if (received_n != expected_n){
    fprintf(stderr, "Not all arguments received.\n");
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_reflectance)){
    fprintf(stderr, "Reflectance file %s does not exist.\n", args->path_reflectance);
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_quality)){
    fprintf(stderr, "Quality file %s does not exist.\n", args->path_quality);
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_mask)){
    fprintf(stderr, "Mask file %s does not exist.\n", args->path_mask);
    usage(argv[0], FAILURE);
  }
  
  if (fileexist(args->path_output)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output);
    usage(argv[0], FAILURE);
  }

  return;
}


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
