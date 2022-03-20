include_directories(../examples/roms)

fips_begin_app(chips-test cmdline)
    fips_vs_warning_level(3)
    fips_files(
        chips-test.c 
        kbd-test.c
        mem-test.c
        ay38910-test.c
        z80ctc-test.c
        z80pio-test.c
        z80dasm-test.c
    )
fips_end_app()

fips_begin_app(z80-zex cmdline)
    fips_vs_warning_level(3)
    fips_files(z80-zex.c)
    fips_dir(roms)
    fipsutil_embed(zex-dump.yml zex-dump.h)
fips_end_app()

fips_begin_app(z80-int cmdline)
    fips_vs_warning_level(3)
    fips_files(z80-int.c)
fips_end_app()

fips_begin_app(z80-timing cmdline)
    fips_vs_warning_level(3)
    fips_files(z80-timing.c)
fips_end_app()

fips_begin_app(z80-fuse cmdline)
    fips_vs_warning_level(3)
    fips_files(z80-fuse.c)
    fips_dir(fuse)
    fips_generate(FROM fuse.yml TYPE fuse HEADER fuse.h)
fips_end_app()

fips_begin_app(z80-test cmdline)
    fips_vs_warning_level(3)
    fips_files(z80-test.c)
fips_end_app()
