/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This file parses command line arguments for reference period computation
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#include "args_reference_period.h"


void usage(char *exe, int exit_code){

  printf("Usage: %s -j cpus -x mask-image \n", exe);
  printf("          -p input-reference-image -r output-reference-period-image\n");
  printf("          -i input-coefficient-image -c output-coefficient-image\n");
  printf("          -m modes -t trend -e year -s threshold -n confirmation-number input-image(s)\n");
  printf("\n");
  printf("  -j = number of CPUs to use\n");
  printf("\n");
  printf("  -x = mask image\n");
  printf("  -p = input reference period image (e.g., previous_reference_period.tif)\n");
  printf("  -r = output reference period image (e.g., reference_period.tif)\n");
  printf("  -i = input coefficient image (e.g., previous_coefficients.tif)\n");
  printf("  -c = output coefficient image (e.g., coefficient.tif)\n");
  printf("\n");
  printf("  -m = number of modes for fitting the harmonic model (1-3)\n");
  printf("  -t = use trend coefficient when fitting the harmonic model? (0 = no, 1 = yes)\n");
  printf("  -y = latest year to fit reference period to (e.g., 2020)\n");
  printf("  -s = threshold for detecting change (e.g., 500)\n");
  printf("  -n = confirmation number for detecting change (e.g., 3)\n");
  printf("\n");
  printf("  input-image(s) = input images to compute reference period from\n");
  printf("                   images must be ordered by date (earliest to latest)\n");
  printf("                   no image from this year should be included!\n");
  printf("\n");

  exit(exit_code);
  return;
}


void parse_args(int argc, char *argv[], args_t *args){
int opt, received_n = 0, expected_n = 11;
  opterr = 0;

  while ((opt = getopt(argc, argv, "j:x:p:r:i:c:m:t:y:s:n:")) != -1){
    switch(opt){
      case 'j':
        args->n_cpus = atoi(optarg);
        received_n++;
        break;
      case 'x':
        copy_string(args->path_mask, STRLEN, optarg);
        received_n++;
        break;
      case 'p':
        copy_string(args->path_input_reference_period, STRLEN, optarg);
        received_n++;
        break;
      case 'r':
        copy_string(args->path_output_reference_period, STRLEN, optarg);
        received_n++;
        break;
      case 'i':
        copy_string(args->path_input_coefficient, STRLEN, optarg);
        received_n++;
        break;
      case 'c':
        copy_string(args->path_output_coefficient, STRLEN, optarg);
        received_n++;
        break;
      case 'm':
        args->modes = atoi(optarg);
        received_n++;
        break;
      case 't':
        args->trend = atoi(optarg);
        received_n++;
        break;
      case 'y':
        args->year = atoi(optarg);
        received_n++;
        break;
      case 's':
        args->threshold = atoi(optarg);
        received_n++;
        break;
      case 'n':
        args->confirmation_number = atoi(optarg);
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

  alloc_2D((void***)&args->path_input, args->n_images, STRLEN, sizeof(char));
  for (int i=0; i<args->n_images; i++){
    copy_string(args->path_input[i], STRLEN, argv[optind + i]);
    if (!fileexist(args->path_input[i])){
      fprintf(stderr, "Input file %s does not exist.\n", args->path_input[i]);
      usage(argv[0], FAILURE);
    }
  }
  
  if (!fileexist(args->path_input_coefficient)){
    fprintf(stderr, "Input coefficient file %s does not exist.\n", args->path_input_coefficient);
    usage(argv[0], FAILURE);
  }

  if (fileexist(args->path_output_coefficient)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output_coefficient);
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_input_reference_period)){
    fprintf(stderr, "Input reference period file %s does not exist.\n", args->path_input_reference_period);
    usage(argv[0], FAILURE);
  }

  if (fileexist(args->path_output_reference_period)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output_reference_period);
    usage(argv[0], FAILURE);
  }

  if (args->n_cpus < 1){
    fprintf(stderr, "Number of CPUs must be at least 1.\n");
    usage(argv[0], FAILURE);
  }

  if (args->modes != 1 && args->modes != 2 && args->modes != 3){
    fprintf(stderr, "modes must be between 1, 2, or 3.\n");
    usage(argv[0], FAILURE);
  }

  if (args->trend != 0 && args->trend != 1){
    fprintf(stderr, "trend must be 0 (no) or 1 (yes).\n");
    usage(argv[0], FAILURE);
  }

  if (args->confirmation_number < 1){
    fprintf(stderr, "confirmation number must be at least 1.\n");
    usage(argv[0], FAILURE);
  }

  if (args->threshold == 0){
    fprintf(stderr, "threshold must be non-zero.\n");
    usage(argv[0], FAILURE);
  }

  if (args->year < 1970 || args->year > 2100){
    fprintf(stderr, "year must be between 1970 and 2100. Or even better a reasonable year\n");
    usage(argv[0], FAILURE);
  }

  return;
}


