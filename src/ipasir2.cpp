/**
 * @file ipasir2cadical.cc
 * @author Markus Iser (markus.iser@kit.edu)
 * @brief Wrap CaDiCaL solver into IPASIR 2 solver
 * @version 0.1
 * @date 2022-11-17
 */

#include "ipasir2.h"
#include "ccadical.h"

#include <cassert>

#include <cstring>
#include <iostream>
#include <vector>

using std::string;

#include "config.hpp"
#include "options.hpp"


// Configuration

ipasir2_errorcode ipasir2_options(void* S, ipasir2_option const** result) {
    // ipasir2_options() can be called concurrently. Initializing option_defs with the
    // result of a function call is thread-safe - hence the initialization code is wrapped
    // in a lambda:
    static std::vector<ipasir2_option> const option_defs = []() {
        int n_extra = 2;

        std::vector<ipasir2_option> extra;
        extra.resize(n_extra);

        extra[0] = { "ipasir.limits.decisions", -1, INT32_MAX, IPASIR2_S_INPUT, 0, 0,
                        (void const*) +[] (void* solver, ipasir2_option const* opt, int64_t value) { ccadical_limit((CCaDiCaL*)solver, "decisions", value); } };
        extra[1] = { "ipasir.limits.conflicts", -1, INT32_MAX, IPASIR2_S_INPUT, 0, 0,
                        (void const*) +[] (void* solver, ipasir2_option const* opt, int64_t value) { ccadical_limit((CCaDiCaL*)solver, "conflicts", value); } };

        std::vector<ipasir2_option> solver_options;
        solver_options.resize(CaDiCaL::number_of_options + n_extra + 1);

        int i = 0;
        for (; i < n_extra; ++i) {
            solver_options[i] = extra[i];
        }

        for (CaDiCaL::Option* option = CaDiCaL::Options::begin(); option != CaDiCaL::Options::end(); ++option) {
            if (option->optimizable) {
                solver_options[i].name = option->name;
                solver_options[i].min = option->lo;
                solver_options[i].max = option->hi;
                solver_options[i].max_state = IPASIR2_S_CONFIG; // TODO: figure out eligible states and pick better default. being conservative for now.
                solver_options[i].tunable = option->optimizable;
                solver_options[i].indexed = 0;
                solver_options[i].handle = (void const*) +[] (CCaDiCaL* solver, ipasir2_option const* opt, int64_t value) { ccadical_set_option(solver, opt->name, value); };
                ++i;
            }
        }

        solver_options[i] = { nullptr, 0, 0, IPASIR2_S_CONFIG, 0, 0, (void const*)nullptr};

        return solver_options;
    }();

    *result = option_defs.data();
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_set_option(void* solver, ipasir2_option const* option, int64_t value, int64_t index) {
    if (option != nullptr && option->handle != nullptr) {
        if (value < option->min || value > option->max) {
            return IPASIR2_E_INVALID_ARGUMENT;
        } // todo: check state
        ((void (*) (void*, ipasir2_option const*, int64_t))option->handle)((CCaDiCaL*)solver, option, value);
        return IPASIR2_E_OK;
    }
    else {
        return IPASIR2_E_INVALID_ARGUMENT;
    }
}


// Basic IPASIR

ipasir2_errorcode ipasir2_signature(char const** result) {
    *result = ccadical_signature();
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_init(void** result) {
    *result = ccadical_init();
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_release(void* solver) {
    ccadical_release((CCaDiCaL*)solver);
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_add(void* solver, int32_t const* clause, int32_t len, int32_t forgettable) {
    for (int i = 0; i < len; ++i) {
        ccadical_add((CCaDiCaL*)solver, clause[i]);
    }
    ccadical_add((CCaDiCaL*)solver, 0);
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_solve(void* solver, int* result, int32_t const* assumps, int32_t len) {
    for (int i = 0; i < len; ++i) {
        ccadical_assume((CCaDiCaL*)solver, assumps[i]);
    }
    *result = ccadical_solve((CCaDiCaL*)solver);
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_val(void* solver, int32_t lit, int32_t* result) {
    *result = ccadical_val((CCaDiCaL*)solver, lit);
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_failed(void* solver, int32_t lit, int* result) {
    *result = ccadical_failed((CCaDiCaL*)solver, lit);
    return IPASIR2_E_OK;
}


// Callbacks

ipasir2_errorcode ipasir2_set_terminate(void* solver, void* data, 
        int (*callback)(void* data)) {
    ccadical_set_terminate((CCaDiCaL*)solver, data, callback);
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_set_export(void* solver, void* data, int32_t max_length,
        void (*callback)(void* data, int32_t const* clause)) {
    if (max_length < 0) {
        return IPASIR2_E_UNSUPPORTED_ARGUMENT;
    }
    ccadical_set_learn((CCaDiCaL*)solver, data, max_length, callback);
    return IPASIR2_E_OK;
}

ipasir2_errorcode ipasir2_set_import(void* solver, void* data, void (*callback)(void* data)) {
    return IPASIR2_E_UNSUPPORTED;
}

ipasir2_errorcode ipasir2_set_fixed(void* solver, void* data, void (*callback)(void* data, int32_t fixed)) {
    return IPASIR2_E_UNSUPPORTED;
}
