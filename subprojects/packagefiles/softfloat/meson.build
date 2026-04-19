
project('softfloat', 'c',
    default_options: ['c_std=c11'],
)

_src_dir  = meson.current_source_dir() / 'source'
_spec_dir = meson.current_source_dir() / 'source' / 'RISCV'

_src_all = run_command(
    'find', _src_dir, '-maxdepth', '1', '-name', '*.c',
    check: true,
).stdout().strip().split('\n')

_spec_src = run_command(
    'find', _spec_dir, '-maxdepth', '1', '-name', '*.c',
    check: true,
).stdout().strip().split('\n')

# These s_*M.c files are slow-path (SOFTFLOAT_SLOW_INT64) internal helpers.
# The official Linux-x86_64-GCC Makefile excludes them when SOFTFLOAT_FAST_INT64
# is set; including them causes undeclared-function errors in C11 mode.
_slow_path = [
    _src_dir / 's_addCarryM.c',           _src_dir / 's_addComplCarryM.c',
    _src_dir / 's_addExtF80M.c',           _src_dir / 's_addF128M.c',
    _src_dir / 's_addM.c',                 _src_dir / 's_compare128M.c',
    _src_dir / 's_compare96M.c',           _src_dir / 's_compareNonnormExtF80M.c',
    _src_dir / 's_invalidExtF80M.c',       _src_dir / 's_invalidF128M.c',
    _src_dir / 's_isNaNF128M.c',           _src_dir / 's_mul128MTo256M.c',
    _src_dir / 's_mul64To128M.c',          _src_dir / 's_mulAddF128M.c',
    _src_dir / 's_negXM.c',                _src_dir / 's_normExtF80SigM.c',
    _src_dir / 's_normRoundPackMToExtF80M.c', _src_dir / 's_normRoundPackMToF128M.c',
    _src_dir / 's_normSubnormalF128SigM.c', _src_dir / 's_remStepMBy32.c',
    _src_dir / 's_roundMToI64.c',          _src_dir / 's_roundMToUI64.c',
    _src_dir / 's_roundPackMToExtF80M.c',  _src_dir / 's_roundPackMToF128M.c',
    _src_dir / 's_shiftLeftM.c',           _src_dir / 's_shiftNormSigF128M.c',
    _src_dir / 's_shiftRightJamM.c',       _src_dir / 's_shiftRightM.c',
    _src_dir / 's_shortShiftLeft64To96M.c', _src_dir / 's_shortShiftLeftM.c',
    _src_dir / 's_shortShiftRightExtendM.c', _src_dir / 's_shortShiftRightJamM.c',
    _src_dir / 's_shortShiftRightM.c',     _src_dir / 's_sub1XM.c',
    _src_dir / 's_subM.c',                 _src_dir / 's_tryPropagateNaNExtF80M.c',
    _src_dir / 's_tryPropagateNaNF128M.c',
]

# Same pattern in RISCV/: the M variants of ext80 and f128 NaN helpers are
# slow-path and absent from the Makefile's OBJS_SPECIALIZE list.
_slow_path_riscv = [
    _spec_dir / 's_commonNaNToExtF80M.c', _spec_dir / 's_commonNaNToF128M.c',
    _spec_dir / 's_extF80MToCommonNaN.c',  _spec_dir / 's_f128MToCommonNaN.c',
    _spec_dir / 's_propagateNaNExtF80M.c', _spec_dir / 's_propagateNaNF128M.c',
]

_src = []
foreach f : _src_all
    if not (f in _slow_path)
        _src += [f]
    endif
endforeach

_spec_src_filt = []
foreach f : _spec_src
    if not (f in _slow_path_riscv)
        _spec_src_filt += [f]
    endif
endforeach

# Public includes: softfloat.h and the platform header.
softfloat_inc = [
    include_directories('source/include'),
    include_directories('build/Linux-x86_64-GCC'),
]

# Private: also expose RISCV/ so specialize.h is found during compilation.
_priv_inc = softfloat_inc + [include_directories('source/RISCV')]

softfloat_lib = static_library('softfloat',
    _src + _spec_src_filt,
    include_directories: _priv_inc,
    # The upstream Makefile uses -O2. platform.h defines INLINE=inline (C11
    # semantics), which relies on the compiler actually inlining things; at
    # -O0 the inline wrapper bodies are never emitted and linking fails.
    override_options: ['optimization=2'],
    c_args: [
        '-DSOFTFLOAT_FAST_INT64',
        '-DSOFTFLOAT_FAST_DIV32TO16',
        '-DSOFTFLOAT_FAST_DIV64TO32',
        '-DSOFTFLOAT_ROUND_ODD',
        # INLINE_LEVEL=5 with -O0 causes the always_inline primitives to be
        # emitted as unreferenceable static symbols; omitting it lets the
        # fallback .c files provide plain external definitions instead.
        # Must be in c_args (not add_project_arguments) so these flags arrive
        # after the parent project's add_global_arguments(-Werror=all).
        '-w',
        '-Wno-error',
    ],
)

softfloat_dep = declare_dependency(
    link_with: softfloat_lib,
    include_directories: softfloat_inc,
)
