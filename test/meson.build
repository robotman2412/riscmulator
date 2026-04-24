
test_src = files(
    'src/atomics.c',
    'src/branch.c',
    'src/decompress.c',
    'src/main.c',
    'src/mem.c',
    'src/mmio.c',
    'src/multicpu.c',
    'src/op.c',
    'src/clint.c',
    'src/plic.c',
    'src/paging.c',
    'src/pmp.c',
    'src/riscv_tests.c',
    'src/softfloat.c',
    'src/trap.c',
)

test_include = [
    include_directories('include'),
]

test_exe = executable('main',
    test_src,
    include_directories: test_include + emulator_include,
    link_with: emulator_lib,
    dependencies: [softfloat_dep],
)
