
project('riscmulator', 'c',
    default_options: [
        'c_std=c23',
        'buildtype=debugoptimized',
    ],
)
add_global_arguments('-Werror=all', language: 'c')
add_global_arguments('-Werror=implicit-fallthrough', language: 'c')
add_global_arguments('-Werror=extra', language: 'c')

subdir('emulator')
subdir('main')
subdir('test')
