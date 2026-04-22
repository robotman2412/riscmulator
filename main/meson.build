
main_src = files(
    'src/main.c',
)

main_exe = executable('main',
    main_src,
    include_directories: emulator_include,
    link_with: emulator_lib,
    dependencies: [softfloat_dep],
)
