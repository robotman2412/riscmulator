
test_src = files(
    'src/jal.c',
    'src/main.c',
)

test_include = [
    include_directories('include'),
]

test_exe = executable('main',
    test_src,
    include_directories: test_include + emulator_include,
    link_with: emulator_lib,
)
