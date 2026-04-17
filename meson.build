
project('riscmulator', 'c')

include = [
    include_directories('include')
]

executable('main',
    'src/rv_impl/base.c',
    'src/main.c',
    'src/rv_cpu.c',
    'src/rv_csr.c',
    'src/rv_machine.c',
    c_args: ['-std=c23', '-Werror=all', '-Werror=implicit-fallthrough', '-Wextra'],
    include_directories: include
)
