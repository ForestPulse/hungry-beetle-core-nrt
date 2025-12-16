# Libraries

GDAL_INCLUDES = $(shell gdal-config --cflags)
GDAL_LIBS = $(shell gdal-config --libs)
GDAL_FLAGS = -Wl,-rpath=/usr/lib

GSL_INCLUDES = $(shell gsl-config --cflags)
GSL_LIBS = $(shell gsl-config --libs)
GSL_FLAGS = -Wl,-rpath=/usr/lib/x86_64-linux-gnu -DHAVE_INLINE=1 -DGSL_RANGE_CHECK=0

### EXECUTABLES TO BE CHECKED PRE-COMPILATION

EXE_PRE = gcc g++

OK := $(foreach exec,$(EXE_PRE),\
        $(if $(shell which $(exec)),OK,$(error "No $(exec) in PATH, install dependencies!")))

### EXECUTABLES TO BE CHECKED POST-INSTALL

EXE_POST = disturbance_detection

### COMPILER

GCC=gcc
GPP=g++
G11=g++ -std=c++11

CFLAGS=-O3 -Wall -fopenmp
#CFLAGS=-g -Wall -fopenmp

INCLUDES=$(GDAL_INCLUDES) $(GSL_INCLUDES)
FLAGS=$(CFLAGS) $(GDAL_FLAGS) $(GSL_FLAGS)
LIBS=$(GDAL_LIBS) $(GSL_LIBS) -lm

### DIRECTORIES

DMAIN=src
DUTILS=src/utils
DTMP=src/temp
DMOD=src/temp/modules
DARG=src/temp/modules/args
DBIN=src/temp/bin
DMISC=misc
DINSTALL=$(HOME)/bin

### TARGETS

all: temp exe
utils: alloc date dir harmonic image_io quality stats string
args: args_spectral_index args_reference_period args_disturbance_detection args_temporal_variability args_combine_disturbances args_update_mask
exe: spectral_index temporal_variability reference_period disturbance_detection update_mask combine_disturbances
.PHONY: temp all install install_ clean check

### TEMP

temp:
	mkdir -p $(DTMP) $(DMOD) $(DARG) $(DBIN)


### UTILS COMPILE UNITS

alloc: temp $(DUTILS)/alloc.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/alloc.c -o $(DMOD)/alloc.o

date: temp $(DUTILS)/date.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/date.c -o $(DMOD)/date.o

dir: temp $(DUTILS)/dir.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/dir.c -o $(DMOD)/dir.o

harmonic: temp $(DUTILS)/harmonic.c
	$(GCC) $(CFLAGS) $(GDAL_INCLUDES) $(GDAL_FLAGS) -c $(DUTILS)/harmonic.c -o $(DMOD)/harmonic.o

quality: temp $(DUTILS)/quality.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/quality.c -o $(DMOD)/quality.o

image_io: temp $(DUTILS)/image_io.c
	$(GCC) $(CFLAGS) $(GDAL_INCLUDES) $(GDAL_FLAGS) -c $(DUTILS)/image_io.c -o $(DMOD)/image_io.o

stats: temp $(DUTILS)/stats.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/stats.c -o $(DMOD)/stats.o

string: temp $(DUTILS)/string.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/string.c -o $(DMOD)/string.o


### ARG PARSING UNITS
args_reference_period: temp $(DMAIN)/args/args_reference_period.c
	$(GCC) $(CFLAGS) -c $(DMAIN)/args/args_reference_period.c -o $(DARG)/args_reference_period.o

args_disturbance_detection: temp $(DMAIN)/args/args_disturbance_detection.c
	$(GCC) $(CFLAGS) -c $(DMAIN)/args/args_disturbance_detection.c -o $(DARG)/args_disturbance_detection.o

args_spectral_index: temp $(DMAIN)/args/args_spectral_index.c
	$(GCC) $(CFLAGS) -c $(DMAIN)/args/args_spectral_index.c -o $(DARG)/args_spectral_index.o

args_temporal_variability: temp $(DMAIN)/args/args_temporal_variability.c
	$(GCC) $(CFLAGS) -c $(DMAIN)/args/args_temporal_variability.c -o $(DARG)/args_temporal_variability.o

args_combine_disturbances: temp $(DMAIN)/args/args_combine_disturbances.c
	$(GCC) $(CFLAGS) -c $(DMAIN)/args/args_combine_disturbances.c -o $(DARG)/args_combine_disturbances.o

args_update_mask: temp $(DMAIN)/args/args_update_mask.c
	$(GCC) $(CFLAGS) -c $(DMAIN)/args/args_update_mask.c -o $(DARG)/args_update_mask.o


### EXECUTABLES


disturbance_detection: temp utils args_disturbance_detection $(DMAIN)/disturbance_detection.c
	$(GCC) $(FLAGS) $(INCLUDES) -o $(DBIN)/disturbance_detection $(DMAIN)/disturbance_detection.c $(DMOD)/*.o $(DARG)/args_disturbance_detection.o $(LIBS)

spectral_index: temp utils args_spectral_index $(DMAIN)/spectral_index.c
	$(GCC) $(FLAGS) $(INCLUDES) -o $(DBIN)/spectral_index $(DMAIN)/spectral_index.c $(DMOD)/*.o $(DARG)/args_spectral_index.o $(LIBS)

temporal_variability: temp utils args_temporal_variability $(DMAIN)/temporal_variability.c
	$(GCC) $(FLAGS) $(INCLUDES) -o $(DBIN)/temporal_variability $(DMAIN)/temporal_variability.c $(DMOD)/*.o $(DARG)/args_temporal_variability.o $(LIBS)

reference_period: temp utils args_reference_period $(DMAIN)/reference_period.c
	$(GCC) $(FLAGS) $(INCLUDES) -o $(DBIN)/reference_period $(DMAIN)/reference_period.c $(DMOD)/*.o $(DARG)/args_reference_period.o $(LIBS)

update_mask: temp utils args_update_mask $(DMAIN)/update_mask.c
	$(GCC) $(FLAGS) $(INCLUDES) -o $(DBIN)/update_mask $(DMAIN)/update_mask.c $(DMOD)/*.o $(DARG)/args_update_mask.o $(LIBS)

combine_disturbances: temp utils args_combine_disturbances $(DMAIN)/combine_disturbances.c
	$(GCC) $(FLAGS) $(INCLUDES) -o $(DBIN)/combine_disturbances $(DMAIN)/combine_disturbances.c $(DMOD)/*.o $(DARG)/args_combine_disturbances.o $(LIBS)

### MISC

install_:
	cp -a $(DMISC)/* $(DBIN)
	chmod 0755 $(DBIN)/*
	cp -a $(DBIN)/. $(DINSTALL)

clean:
	rm -rf $(DTMP)

check:
	$(foreach exec,$(EXE_POST),\
      $(if $(shell which $(DINSTALL)/$(exec)), \
	    $(info $(exec) installed), \
		$(error $(exec) was not installed properly!))) 

install: install_ clean check
