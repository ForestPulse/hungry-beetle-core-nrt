/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This file parses command line arguments for temporal variability computation
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

#include "args_temporal_variability.h"

void usage(char *exe, int exit_code){
  printf("Usage: %s -j cpus -o output-image -x mask-image -r reference-period-image input-image(s)\n", exe);
  printf("\n");
  printf("  -j = number of CPUs to use\n");
  printf("\n");
  printf("  -o = output image\n");
  printf("  -x = mask image\n");
  printf("  -r = reference period image\n");
  printf("\n");
  printf("  input-image(s) = one or more input images to compute temporal variability from\n");
  printf("\n");
  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
  int opt, received_n = 0, expected_n = 4;
  opterr = 0;

  while ((opt = getopt(argc, argv, "j:o:r:x:")) != -1){
    switch(opt){
      case 'j':
        args->n_cpus = atoi(optarg);
        received_n++;
        break;
      case 'o':
        copy_string(args->path_output, STRLEN, optarg);
        received_n++;
        break;
      case 'r':
        copy_string(args->path_reference, STRLEN, optarg);
        received_n++;
        break;
      case 'x':
        copy_string(args->path_mask, STRLEN, optarg);
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
  
  if (fileexist(args->path_output)){
    fprintf(stderr, "Output file %s already exists.\n", args->path_output);
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_reference)){
    fprintf(stderr, "Reference file %s does not exist.\n", args->path_reference);
    usage(argv[0], FAILURE);
  }

  if (args->n_cpus < 1){
    fprintf(stderr, "Number of CPUs must be at least 1.\n");
    usage(argv[0], FAILURE);
  }

  return;
}
