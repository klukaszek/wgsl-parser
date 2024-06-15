fips_begin_lib(wgsl_parser)
    fips_files(wgsl-parser.c wgsl-parser.h)
    target_link_libraries(wgsl_parser pcre)
fips_end_lib()

fips_begin_app(test cmdline)
    fips_files(test.c)
    fips_deps(wgsl_parser)
fips_end_app()
