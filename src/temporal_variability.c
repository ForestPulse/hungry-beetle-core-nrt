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
#include "utils/string.h"
#include "utils/stats.h"

typedef struct {
  int n_images;
  char **path_images;
  char path_output[STRLEN];
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -o output-image input-image(s)\n", exe);
  printf("\n");
  printf("  -o = output image\n");
  printf("\n");
  printf("  input-image(s) = one or more input images to compute temporal variability from\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 1;
  opterr = 0;

  while ((opt = getopt(argc, argv, "o:")) != -1){
    switch(opt){
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
    fprintf(stderr, "Not all '-' arguments received.\n");
    usage(argv[0], FAILURE);
  }

  if ((args->n_images = argc - optind) < 1){
    fprintf(stderr, "At least one input image must be provided.\n");
    usage(argv[0], FAILURE);
  }

  alloc_2D((void***)&args->path_images, args->n_images, STRLEN, sizeof(char));
  for (int i=0; i<args->n_images; i++){
    copy_string(args->path_images[i], STRLEN, argv[optind + i]);
    if (!fileexist(args->path_images[i])){
      fprintf(stderr, "Input file %s does not exist.\n", args->path_images[i]);
      usage(argv[0], FAILURE);
    }
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


  GDALDatasetH  fp_image;
  if ((fp_image = GDALOpen(args.path_images[0], GA_ReadOnly))== NULL){ 
    fprintf(stderr, "could not open %s\n", args.path_images[0]); exit(FAILURE);}

  int nx = GDALGetRasterXSize(fp_image);
  int ny = GDALGetRasterYSize(fp_image);
  int nc = nx*ny;

  char proj[STRLEN];
  copy_string(proj, STRLEN, GDALGetProjectionRef(fp_image));
  
  double geotran[6];
  GDALGetGeoTransform(fp_image, geotran);

  GDALClose(fp_image);


  short **images = NULL;
  short *nodata_images = NULL;
  alloc_2D((void***)&images, args.n_images, nc, sizeof(short));
  alloc((void**)&nodata_images, args.n_images, sizeof(short));

  for (int i=0; i<args.n_images; i++){

    if ((fp_image = GDALOpen(args.path_images[i], GA_ReadOnly))== NULL){ 
      fprintf(stderr, "could not open %s\n", args.path_images[i]); exit(FAILURE);}

    if (nx != GDALGetRasterXSize(fp_image)){
      fprintf(stderr, "dimension mismatch between %s and %s\n", args.path_images[0], args.path_images[i]); exit(FAILURE);}
    if (ny != GDALGetRasterYSize(fp_image)){
      fprintf(stderr, "dimension mismatch between %s and %s\n", args.path_images[0], args.path_images[i]); exit(FAILURE);}

    if (strcmp(proj, GDALGetProjectionRef(fp_image)) != 0){
      fprintf(stderr, "projection mismatch between %s and %s\n", args.path_images[0], args.path_images[i]); exit(FAILURE);}

    {
      double geotran_image[6];
      GDALGetGeoTransform(fp_image, geotran_image);
      for (int k=0; k<6; k++){
        if (geotran[k] != geotran_image[k]){
          fprintf(stderr, "geotransform mismatch between %s and %s\n", args.path_images[0], args.path_images[i]); exit(FAILURE);
        }
      }
    }

    {
      GDALRasterBandH band_image = GDALGetRasterBand(fp_image, 1);
      int has_nodata;
      nodata_images[i] = (short)GDALGetRasterNoDataValue(band_image, &has_nodata);
      if (!has_nodata){
        fprintf(stderr, "%s has no nodata value.\n", args.path_images[i]); 
        exit(FAILURE);
      }
      if (GDALRasterIO(band_image, GF_Read, 0, 0, 
        nx, ny, images[i], 
        nx, ny, GDT_Int16, 0, 0) == CE_Failure){
        printf("could not read band 1 from %s.\n", args.path_images[i]); exit(FAILURE);}
    }
  
    GDALClose(fp_image);

  }


  short nodata_variability = SHRT_MIN;
  short *variability = NULL;
  alloc((void**)&variability, nc, sizeof(short));

  for (int p=0; p<nc; p++){

    variability[p] = nodata_variability;
    
    double mean = 0, var = 0, n = 0;
  
    for (int i=0; i<args.n_images; i++){

      if (images[i][p] == nodata_images[i]) continue;

      // compute mean, variance
      n++;
      var_recurrence((double)images[i][p], &mean, &var, (double)n);

    }

    if (n > 0) variability[p] = (short)standdev(var, n);
  
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
    nx, ny, variability, 
    nx, ny, GDT_Int16, 0, 0) == CE_Failure){
    printf("Unable to write %s.\n", args.path_output); 
    exit(FAILURE);
  }

  GDALSetDescription(band_output, "Temporal Variability");
  GDALSetRasterNoDataValue(band_output, nodata_variability);

  GDALSetGeoTransform(fp_output, geotran);
  GDALSetProjection(fp_output,   proj);

  GDALClose(fp_output);

  free((void*)variability);
  free_2D((void**)images, args.n_images);
  free((void*)nodata_images);
  free_2D((void**)args.path_images, args.n_images);
  
  if (options != NULL) CSLDestroy(options);   
  GDALDestroy();

  exit(SUCCESS);
}
