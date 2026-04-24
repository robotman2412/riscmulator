
emulator_src = files(
    'src/rv_impl/base.c',
    'src/rv_impl/atomics.c',
    'src/rv_impl/float.c',
    'src/rv_impl/muldiv.c',
    'src/rv_cpu.c',
    'src/rv_csr.c',
    'src/rv_clint.c',
    'src/rv_plic.c',
    'src/rv_pmp.c',
    'src/rv_decompress.c',
    'src/rv_machine.c',
    'src/rv_paging.c',
    'src/rv_privileged.c',
)

emulator_include = [
    include_directories('include'),
]

softfloat_dep = dependency('softfloat')
libm_dep = meson.get_compiler('c').find_library('m', required: true)

emulator_lib = library('riscmulator',
    emulator_src,
    include_directories: emulator_include,
    dependencies: [dependency('threads'), softfloat_dep, libm_dep],
    c_args: [ '-fno-fast-math' ],
)
