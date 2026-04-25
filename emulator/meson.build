
emulator_src = files(
    'src/cpu/rv_impl/base.c',
    'src/cpu/rv_impl/atomics.c',
    'src/cpu/rv_impl/float.c',
    'src/cpu/rv_impl/muldiv.c',
    'src/cpu/rv_cpu.c',
    'src/cpu/rv_csr.c',
    'src/cpu/rv_pmp.c',
    'src/cpu/rv_decompress.c',
    'src/cpu/rv_paging.c',
    'src/cpu/rv_privileged.c',
    'src/device/rv_clint.c',
    'src/device/rv_plic.c',
    'src/emu_machine.c',
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
