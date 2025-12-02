#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/** Geospatial Data Abstraction Library (GDAL) **/
#include "gdal.h"       // public (C callable) GDAL entry points
#include "cpl_conv.h"   // various convenience functions for CPL
#include "cpl_string.h" // various convenience functions for strings


#include "utils/const.h"
#include "utils/alloc.h"
#include "utils/dir.h"
#include "utils/string.h"


typedef struct {
  char path_reflectance[STRLEN];
  char path_quality[STRLEN];
  char path_output[STRLEN];
  int n_cpus;
  char index[STRLEN];
} args_t;


void usage(char *exe, int exit_code){

  printf("Usage: %s -j cpus -r reflectance-image -q quality-image -o output-image\n", exe);
  printf("\n");
  printf("  -j = number of CPUs to use\n");
  printf("  -r = reflectance image, FORCE BOA image, either Sentinel-2 or Landsat\n");
  printf("  -q = quality image, FORCE QAI image\n");
  printf("  -o = output image\n");
  printf("\n");

  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 4;
  opterr = 0;

  while ((opt = getopt(argc, argv, "j:r:q:o:")) != -1){
    switch(opt){
      case 'j':
        args->n_cpus = atoi(optarg);
        received_n++;
        break;
      case 'r':
        copy_string(args->path_reflectance, STRLEN, optarg);
        received_n++;
        break;
      case 'q':
        copy_string(args->path_quality, STRLEN, optarg);
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

  if (fileexist(args->path_output)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output);
    usage(argv[0], FAILURE);
  }

  if (args->n_cpus < 1){
    fprintf(stderr, "Number of CPUs must be at least 1.\n");
    usage(argv[0], FAILURE);
  }

  return;
}




int main ( int argc, char *argv[] ){
args_t args;


  parse_args(argc, argv, &args);

  GDALAllRegister();





  exit(SUCCESS);
}
