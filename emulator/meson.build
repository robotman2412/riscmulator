
emulator_src = files(
    'src/rv_impl/base.c',
    'src/rv_impl/csr.c',
    'src/rv_impl/muldiv.c',
    'src/rv_cpu.c',
    'src/rv_csr.c',
    'src/rv_decompress.c',
    'src/rv_machine.c',
    'src/rv_privileged.c',
)

emulator_include = [
    include_directories('include'),
]

emulator_lib = library('riscmulator',
    emulator_src,
    include_directories: emulator_include,
    dependencies: dependency('threads'),
)
