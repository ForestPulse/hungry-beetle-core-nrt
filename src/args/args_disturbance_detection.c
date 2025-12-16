/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This file parses command line arguments for disturbance detection
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

#include "args_disturbance_detection.h"

void usage(char *exe, int exit_code){
  printf("Usage: %s -j cpus -c coefficient-image -s variability-image -x mask-image -o output-image\n", exe);
  printf("          -m modes -t trend -d threshold_variability -r threshold_residual -n confirmation-number\n");
  printf("          input-image(s)\n");
  printf("\n");
  printf("  -j = number of CPUs to use\n");
  printf("\n");
  printf("  -x = mask image\n");
  printf("  -c = path to coefficients\n");
  printf("  -s = path to statistics\n");
  printf("  -o = output file (.tif)\n");
  printf("\n");  
  printf("  -m = number of modes for fitting the harmonic model (1-3)\n");
  printf("  -t = use trend coefficient when fitting the harmonic model? (0 = no, 1 = yes)\n");
  printf("  -d = standard deviation threshold\n");
  printf("  -r = minimum residuum threshold\n");
  printf("  -n = number of consecutive observations to detect disturbance event\n");
  printf("\n");
  printf("  input-image(s) = input images to compute disturbances from\n");
  printf("\n");
  exit(exit_code);
  return;
}

void parse_args(int argc, char *argv[], args_t *args){
  int opt, received_n = 0, expected_n = 10;
  opterr = 0;

  while ((opt = getopt(argc, argv, "j:c:s:o:m:t:d:r:n:x:")) != -1){
    switch(opt){
      case 'j':
        args->n_cpus = atoi(optarg);
        received_n++;
        break;
      case 'c':
        copy_string(args->path_coefficients, STRLEN, optarg); 
        received_n++;
        break;
      case 's':
        copy_string(args->path_variability, STRLEN, optarg);
        received_n++;
        break;
      case 'o':
        copy_string(args->path_output, STRLEN, optarg);
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
      case 'd':
        args->threshold_variability = atof(optarg);
        received_n++;
        break;
      case 'r':
        args->threshold_residual = atof(optarg);
        received_n++;
        break;
      case 'n':
        args->confirmation_number = atoi(optarg);
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
  
  if (!fileexist(args->path_coefficients)){
    fprintf(stderr, "Coefficient file %s does not exist.\n", args->path_coefficients);
    usage(argv[0], FAILURE);
  }

  if (!fileexist(args->path_variability)){
    fprintf(stderr, "Variability file %s does not exist.\n", args->path_variability);
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
  
  if (args->modes != 1 && args->modes != 2 && args->modes != 3){
    fprintf(stderr, "modes must be between 1, 2, or 3.\n");
    usage(argv[0], FAILURE);
  }

  if (args->trend != 0 && args->trend != 1){
    fprintf(stderr, "trend must be 0 (no) or 1 (yes).\n");
    usage(argv[0], FAILURE);
  }
  
  if (args->threshold_variability == 0){
    fprintf(stderr, "variabiloity threshold must be non-zero.\n");
    usage(argv[0], FAILURE);
  }
  if (args->threshold_residual == 0){
    fprintf(stderr, "residual threshold must be non-zero.\n");
    usage(argv[0], FAILURE);
  }
  
  if (args->confirmation_number < 1){
    fprintf(stderr, "confirmation number must be at least 1.\n");
    usage(argv[0], FAILURE);
  }

  return;
}
