fips_begin_lib(wgsl_parser)
    fips_files(wgsl-parser.c wgsl-parser.h slre.c slre.h)
fips_end_lib()

fips_begin_app(test cmdline)
    fips_files(test.c)
    fips_deps(wgsl_parser)
fips_end_app()
