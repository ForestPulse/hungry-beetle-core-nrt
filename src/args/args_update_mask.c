/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This file parses command line arguments for update_mask
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

#include "args_update_mask.h"

void usage(char *exe, int exit_code){
  printf("Usage: %s -d disturbance-image -x mask-image -o output-image\n", exe);
  printf("\n");
  printf("  -d = disturbance image\n");
  printf("  -x = mask image\n");
  printf("  -o = output image\n");
  printf("\n");
  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
  int opt, received_n = 0, expected_n = 3;
  opterr = 0;

  while ((opt = getopt(argc, argv, "d:x:o:")) != -1){
    switch(opt){
      case 'd':
        copy_string(args->path_disturbance, STRLEN, optarg);
        received_n++;
        break;
      case 'x':
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

  if (!fileexist(args->path_disturbance)){
    fprintf(stderr, "Disturbance file %s does not exist.\n", args->path_disturbance);
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
