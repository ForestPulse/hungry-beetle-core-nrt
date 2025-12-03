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
#include "utils/string.h"

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


  parse_args(argc, argv, &args);

  GDALAllRegister();
  
  GDALDatasetH  fp_reflectance;
  GDALDatasetH  fp_quality;
  GDALDatasetH  fp_mask;
  
  if ((fp_reflectance = GDALOpen(args.path_reflectance, GA_ReadOnly))== NULL){ 
    fprintf(stderr, "could not open %s\n", args.path_reflectance); exit(FAILURE);}

  if ((fp_quality = GDALOpen(args.path_quality, GA_ReadOnly))== NULL){ 
    fprintf(stderr, "could not open %s\n", args.path_quality); exit(FAILURE);}

  if ((fp_mask = GDALOpen(args.path_mask, GA_ReadOnly))== NULL){ 
    fprintf(stderr, "could not open %s\n", args.path_mask); exit(FAILURE);}

  int nx = GDALGetRasterXSize(fp_reflectance);
  int ny = GDALGetRasterYSize(fp_reflectance);
  int nc = nx*ny;

  if (nx != GDALGetRasterXSize(fp_quality)){
    fprintf(stderr, "dimension mismatch between %s and %s\n", args.path_reflectance, args.path_quality); exit(FAILURE);}
  if (ny != GDALGetRasterYSize(fp_quality)){
    fprintf(stderr, "dimension mismatch between %s and %s\n", args.path_reflectance, args.path_quality); exit(FAILURE);}

  if (nx != GDALGetRasterXSize(fp_mask)){
    fprintf(stderr, "dimension mismatch between %s and %s\n", args.path_reflectance, args.path_mask); exit(FAILURE);}
  if (ny != GDALGetRasterYSize(fp_mask)){
    fprintf(stderr, "dimension mismatch between %s and %s\n", args.path_reflectance, args.path_mask); exit(FAILURE);}
  
  char proj[STRLEN];
  copy_string(proj, STRLEN, GDALGetProjectionRef(fp_reflectance));

  if (strcmp(proj, GDALGetProjectionRef(fp_quality)) != 0){
    fprintf(stderr, "projection mismatch between %s and %s\n", args.path_reflectance, args.path_quality); exit(FAILURE);}
  if (strcmp(proj, GDALGetProjectionRef(fp_mask)) != 0){
    fprintf(stderr, "projection mismatch between %s and %s\n", args.path_reflectance, args.path_mask); exit(FAILURE);}

  double geotran[6];
  GDALGetGeoTransform(fp_reflectance, geotran);

  {
    double geotran_quality[6];
    GDALGetGeoTransform(fp_quality, geotran_quality);
    for (int k=0; k<6; k++){
      if (geotran[k] != geotran_quality[k]){
        fprintf(stderr, "geotransform mismatch between %s and %s\n", args.path_reflectance, args.path_quality); exit(FAILURE);
      }
    }
  }

  {
    double geotran_mask[6];
    GDALGetGeoTransform(fp_mask, geotran_mask);
    for (int k=0; k<6; k++){
      if (geotran[k] != geotran_mask[k]){
        fprintf(stderr, "geotransform mismatch between %s and %s\n", args.path_reflectance, args.path_mask); exit(FAILURE);
      }
    }
  }
  
  int nb = GDALGetRasterCount(fp_reflectance);
  int bands[3];
  float wavelength[3];
  
  if (nb == 10){ // Sentinel-2
    bands[0] = 8; // Red
    bands[1] = 9; // NIR
    bands[2] = 10; // SWIR1
    wavelength[0] = 0.864;
    wavelength[1] = 1.609;
    wavelength[2] = 2.202;
  } else if (nb == 6){ // Landsat
    bands[0] = 4; // Red
    bands[1] = 5; // NIR
    bands[2] = 6; // SWIR1
    wavelength[0] = 0.865;
    wavelength[1] = 1.614;
    wavelength[2] = 2.202;
  } else {
    fprintf(stderr, "Unexpected number of bands (%d) in %s\n", nb, args.path_reflectance);
    exit(FAILURE);
  }
  
  // all good so far, match and integrity confirmed
  
  short nodata_reflectance;
  short **reflectance = NULL;
  alloc_2D((void***)&reflectance, 3, nc, sizeof(short));
  
  for (int b=0; b<3; b++){
    GDALRasterBandH band_reflectance = GDALGetRasterBand(fp_reflectance, bands[b]);
    int has_nodata;
    nodata_reflectance = (short)GDALGetRasterNoDataValue(band_reflectance, &has_nodata);
    if (!has_nodata){
      fprintf(stderr, "%s has no nodata value.\n", args.path_reflectance); 
      exit(FAILURE);
    }
    if (GDALRasterIO(band_reflectance, GF_Read, 0, 0, 
      nx, ny, reflectance[b], 
      nx, ny, GDT_Int16, 0, 0) == CE_Failure){
      printf("could not read band %d from %s.\n", bands[b], args.path_reflectance); exit(FAILURE);}
  }

  GDALClose(fp_reflectance);


  short nodata_quality;
  short *quality = NULL;
  alloc((void**)&quality, nc, sizeof(short));
  
  {
    GDALRasterBandH band_quality = GDALGetRasterBand(fp_quality, 1);
    int has_nodata;
    nodata_quality = (short)GDALGetRasterNoDataValue(band_quality, &has_nodata);
    if (!has_nodata){
      fprintf(stderr, "%s has no nodata value.\n", args.path_quality); 
      exit(FAILURE);
    }
    if (GDALRasterIO(band_quality, GF_Read, 0, 0, 
      nx, ny, quality, 
      nx, ny, GDT_Int16, 0, 0) == CE_Failure){
      printf("could not read band 1 from %s.\n", args.path_quality); exit(FAILURE);}
  }
  
  GDALClose(fp_quality);

  
  char nodata_mask;
  char *mask = NULL;
  alloc((void**)&mask, nc, sizeof(char));
  
  {
    GDALRasterBandH band_mask = GDALGetRasterBand(fp_mask, 1);
    int has_nodata;
    nodata_mask = (char)GDALGetRasterNoDataValue(band_mask, &has_nodata);
    if (!has_nodata){
      fprintf(stderr, "%s has no nodata value.\n", args.path_mask); 
      exit(FAILURE);
    }
    if (GDALRasterIO(band_mask, GF_Read, 0, 0, 
      nx, ny, mask, 
      nx, ny, GDT_Byte, 0, 0) == CE_Failure){
      printf("could not read band 1 from %s.\n", args.path_mask); exit(FAILURE);}
  }
  
  GDALClose(fp_mask);


  short nodata_index = SHRT_MIN;
  short *index = NULL;
  alloc((void**)&index, nc, sizeof(short));

  for (int p=0; p<nc; p++){

    if (quality[p] == nodata_quality ||
        reflectance[0][p] == nodata_reflectance ||
        reflectance[1][p] == nodata_reflectance ||
        reflectance[2][p] == nodata_reflectance ||
        mask[p] == nodata_mask ||
        mask[p] == 0 ||
        !use_this_pixel(quality[p])){
      index[p] = nodata_index;
      continue;
    }

    float interpolated = 
      (reflectance[0][p] * (wavelength[2] - wavelength[1]) + 
       reflectance[2][p] * (wavelength[1] - wavelength[0])) / 
      (wavelength[2] - wavelength[0]);

    index[p] = (short)(reflectance[1][p] - interpolated);
  
  }

  GDALDatasetH fp_output = NULL;
  GDALRasterBandH band_output = NULL;
  GDALDriverH driver = NULL;
  char **options = NULL;

  if ((driver = GDALGetDriverByName("GTiff")) == NULL){
    printf("%s driver not found\n", "GTiff"); exit(FAILURE);}

  options = CSLSetNameValue(options, "COMPRESS", "ZSTD");
  options = CSLSetNameValue(options, "PREDICTOR", "2");
  options = CSLSetNameValue(options, "INTERLEAVE", "BAND");
  options = CSLSetNameValue(options, "BIGTIFF", "YES");
  options = CSLSetNameValue(options, "TILED", "YES");
  options = CSLSetNameValue(options, "BLOCKXSIZE", "256");
  options = CSLSetNameValue(options, "BLOCKYSIZE", "256");

  if ((fp_output = GDALCreate(driver, args.path_output, nx, ny, 1, GDT_Int16, options)) == NULL){
    printf("Error creating file %s.\n", args.path_output); exit(FAILURE);}

  band_output = GDALGetRasterBand(fp_output, 1);
  if (GDALRasterIO(band_output, GF_Write, 0, 0, 
    nx, ny, index, 
    nx, ny, GDT_Int16, 0, 0) == CE_Failure){
    printf("Unable to write %s.\n", args.path_output); 
    exit(FAILURE);
  }

  GDALSetDescription(band_output, "Index: Continuum-Removed SWIR1");
  GDALSetRasterNoDataValue(band_output, nodata_index);

  GDALSetGeoTransform(fp_output, geotran);
  GDALSetProjection(fp_output,   proj);

  GDALClose(fp_output);



  free((void*)index);
  free((void*)quality);
  free((void*)mask);
  free_2D((void**)reflectance, 3);
  if (options != NULL) CSLDestroy(options);   

  GDALDestroy();

  exit(SUCCESS);
}
