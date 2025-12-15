#!/bin/bash

tile="X0055_Y0053"
cube_dir="/nvme2/ahsoka/gi-sds/testdata"
mask_dir="/data/ahsoka/eocp/borkenkaefer/forst"
mask="forest.tif"
out_dir="$HOME/temp/beetle-with-reverse-2"

bin_dir="src/temp/bin"

set -e

# boa and qai must be in the same order and match one-to-one

# compute the indices for 2015 and 2016 without reference period masking
# this is just because it is too complicated to do in the bash loop
for this_year in {2015..2017}; do

  echo "Processing year ${this_year}..."
  prev_year=$((this_year - 1))
  cp ${mask_dir}/${tile}/${mask} ${out_dir}/mask_${this_year}.tif

  cp ${mask_dir}/${tile}/${mask} ${out_dir}/reference_period_${prev_year}.tif
  cp ${mask_dir}/${tile}/${mask} ${out_dir}/coefficients_${prev_year}.tif

  boa_files=$(ls ${cube_dir}/${tile}/${this_year}*SEN2[ABC]*BOA.tif | tr '\n' ' ')
  qai_files=$(ls ${cube_dir}/${tile}/${this_year}*SEN2[ABC]*QAI.tif | tr '\n' ' ')

  parallel --eta --link \
    ${bin_dir}/spectral_index -r {1} -q {2} \
    -x ${out_dir}/mask_${this_year}.tif \
    -o ${out_dir}/{1/.}_CREM.tif \
    ::: $boa_files ::: $qai_files

done


for this_year in {2018..2025}; do

  before_prev_year=$((this_year - 2))
  prev_year=$((this_year - 1))

  echo "Processing year ${this_year}..."

  # Generate reference period and coefficients from previous year's data
  time ${bin_dir}/reference_period -j 64 \
    -p ${out_dir}/reference_period_${before_prev_year}.tif \
    -r ${out_dir}/reference_period_${prev_year}.tif \
    -i ${out_dir}/coefficients_${before_prev_year}.tif \
    -c ${out_dir}/coefficients_${prev_year}.tif \
    -x ${out_dir}/mask_${prev_year}.tif \
    -m 3 -t 0 -e 2017 -y ${prev_year} -s 200 -n 3\
    ${out_dir}/*_CREM.tif

  # Compute temporal variability from previous year's data
  time ${bin_dir}/temporal_variability -j 64 \
    -o ${out_dir}/variability_${prev_year}.tif \
    -r ${out_dir}/reference_period_${prev_year}.tif \
    -x ${out_dir}/mask_${prev_year}.tif \
    ${out_dir}/*_CREM.tif

  # Now compute the indices for the current year
  boa_files=$(ls ${cube_dir}/${tile}/${this_year}*SEN2[ABC]*BOA.tif | tr '\n' ' ')
  qai_files=$(ls ${cube_dir}/${tile}/${this_year}*SEN2[ABC]*QAI.tif | tr '\n' ' ')

  time parallel --eta --link \
    ${bin_dir}/spectral_index -r {1} -q {2} \
    -x ${out_dir}/mask_${prev_year}.tif \
    -o ${out_dir}/{1/.}_CREM.tif \
    ::: $boa_files ::: $qai_files

  # Finally, detect disturbances for the current year
  time ${bin_dir}/disturbance_detection -j 64 \
    -c ${out_dir}/coefficients_${prev_year}.tif \
    -s ${out_dir}/variability_${prev_year}.tif \
    -x ${out_dir}/mask_${prev_year}.tif \
    -o ${out_dir}/disturbance_${this_year}.tif \
    -m 3 -t 0 -d 3 -r 500 -n 3 \
    ${out_dir}/${this_year}*_CREM.tif

  time ${bin_dir}/update_mask \
    -d ${out_dir}/disturbance_${this_year}.tif \
    -x ${out_dir}/mask_${prev_year}.tif \
    -o ${out_dir}/mask_${this_year}.tif

done



${bin_dir}/combine_disturbances ${out_dir}/disturbance_20*.tif -o ${out_dir}/disturbances.tif -j 16

