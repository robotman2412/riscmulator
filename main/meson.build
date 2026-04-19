
main_src = files(
    'src/main.c',
)

main_include = [
    include_directories('include'),
]

main_exe = executable('main',
    main_src,
    include_directories: main_include + emulator_include,
    link_with: emulator_lib,
    dependencies: [softfloat_dep],
)
