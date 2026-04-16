
project('riscmulator', 'c')

include = [
    include_directories('include')
]

executable('main',
    'src/main.c',
    c_args: ['-std=c23', '-Werror=all', '-Wextra'],
    include_directories: include
)
