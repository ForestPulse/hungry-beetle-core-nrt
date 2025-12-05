# Libraries

GDAL=-I/usr/include/gdal -L/usr/lib -Wl,-rpath=/usr/lib

# Linked libs

LDGDAL=-lgdal

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


### DIRECTORIES

DMAIN=src
DUTILS=src/utils
DTMP=src/temp
DMOD=src/temp/modules
DBIN=src/temp/bin
DMISC=misc
DINSTALL=$(HOME)/bin

### TARGETS

all: temp exe
utils: alloc date dir image_io quality stats string
exe: disturbance_detection spectral_index
.PHONY: temp all install install_ clean check

### TEMP

temp:
	mkdir -p $(DTMP) $(DMOD) $(DBIN)


### UTILS COMPILE UNITS

alloc: temp $(DUTILS)/alloc.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/alloc.c -o $(DMOD)/alloc.o

date: temp $(DUTILS)/date.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/date.c -o $(DMOD)/date.o

dir: temp $(DUTILS)/dir.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/dir.c -o $(DMOD)/dir.o

quality: temp $(DUTILS)/quality.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/quality.c -o $(DMOD)/quality.o

image_io: temp $(DUTILS)/image_io.c
	$(GCC) $(CFLAGS) $(GDAL) -c $(DUTILS)/image_io.c -o $(DMOD)/image_io.o

stats: temp $(DUTILS)/stats.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/stats.c -o $(DMOD)/stats.o

string: temp $(DUTILS)/string.c
	$(GCC) $(CFLAGS) -c $(DUTILS)/string.c -o $(DMOD)/string.o


### EXECUTABLES

disturbance_detection: temp utils $(DMAIN)/disturbance_detection.c
	$(GCC) $(CFLAGS) $(GDAL) -o $(DBIN)/disturbance_detection $(DMAIN)/disturbance_detection.c $(DMOD)/*.o $(LDGDAL) -lm

spectral_index: temp utils $(DMAIN)/spectral_index.c
	$(GCC) $(CFLAGS) $(GDAL) -o $(DBIN)/spectral_index $(DMAIN)/spectral_index.c $(DMOD)/*.o $(LDGDAL) -lm

temporal_variability: temp utils $(DMAIN)/temporal_variability.c
	$(GCC) $(CFLAGS) $(GDAL) -o $(DBIN)/temporal_variability $(DMAIN)/temporal_variability.c $(DMOD)/*.o $(LDGDAL) -lm

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
