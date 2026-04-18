
test_src = files(
    'src/branch.c',
    'src/decompress.c',
    'src/main.c',
    'src/mem.c',
    'src/multicpu.c',
    'src/op.c',
    'src/riscv_tests.c',
    'src/trap.c',
)

test_include = [
    include_directories('include'),
]

test_exe = executable('main',
    test_src,
    include_directories: test_include + emulator_include,
    link_with: emulator_lib,
)
