fips_begin_lib(roms)
    fipsutil_embed(zx-roms.yml zx-roms.h)
    fips_files(dummy.c)
fips_end_lib()
