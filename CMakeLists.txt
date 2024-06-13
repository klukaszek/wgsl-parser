#
# project: parse-wgpu
#
cmake_minimum_required(VERSION 3.21)
project(parse-wgsl)
# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

include_directories(assets)
fips_setup()
add_subdirectory(parser)
# add_subdirectory(src/...)
fips_finish()
