
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_csr.h"

#include "string.h"

// Get the name of a CSR; returns `nullptr` if invalid.
char const *rv_csr_to_name(enum rv_csr csr) {
    switch (csr) {
#define RV_CSR_DEF(index, name)                                                                                        \
    case index: return #name;
#include "rv_defs/csr.h"
        default: return nullptr;
    }
}

// Get a CSR by name; returns 0 if not found.
enum rv_csr rv_csr_from_name(char const *name) {
#define RV_CSR_DEF(index, name_)                                                                                       \
    if (!strcmp(name, #name_)) {                                                                                       \
        return index;                                                                                                  \
    }
#include "rv_defs/csr.h"
    return 0;
}
