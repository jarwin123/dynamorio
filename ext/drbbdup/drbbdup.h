/* **********************************************************
 * Copyright (c) 2020 Google, Inc.   All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _DRBBDUP_H_
#define _DRBBDUP_H_ 1

#include "drmgr.h"
#include <stdint.h>

/**
 * @file drbbdup.h
 * @brief Header for DynamoRIO Basic Block Duplicator Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drbbdup Basic Block Duplicator
 */
/*@{*/ /* begin doxygen group */

/** Success code for each drbbdup operation. */
typedef enum {
    DRBBDUP_SUCCESS,                       /**< Operation succeeded. */
    DRBBDUP_ERROR_INVALID_PARAMETER,       /**< Operation failed: invalid parameter. */
    DRBBDUP_ERROR_CASE_ALREADY_REGISTERED, /**< Operation failed: already registered. */
    DRBBDUP_ERROR_CASE_LIMIT_REACHED,      /**< Operation failed: case limit reached. */
    DRBBDUP_ERROR_ALREADY_INITIALISED,     /**< DRBBDUP can only be initialised once. */
    DRBBDUP_ERROR,                         /**< Operation failed. */
} drbbdup_status_t;

/***************************************************************************
 * User call-back functions
 */

/**
 * Set up initial information related to managing copies of a new basic block \p bb.
 * A pointer-sized value indicating the default case encoding is returned.
 * The boolean value written to \p enable_dups specifies whether code duplication should
 * be done for this particular basic block. If false, the basic block is always executed
 * under the default case and no duplications are made. The user data \p user_data is
 * that supplied to drbbdup_init().
 *
 * Use drbbdup_register_case_value(), passing \p drbbdup_ctx, to register other case
 * encodings.
 *
 * @return the default case encoding.
 */
typedef uintptr_t (*drbbdup_set_up_bb_dups_t)(void *drbbdup_ctx, void *drcontext,
                                              void *tag, instrlist_t *bb,
                                              IN bool *enable_dups, void *user_data);

/**
 * Conducts an analysis of the original basic block. The call-back is not called for
 * each case, but once for the overall fragment. Therefore, computationally expensive
 * analysis that only needs to be done once per fragment can be implemented by this
 * call-back and made available to all cases of the basic block. The function should
 * store the analysis result in \p orig_analysis_data. The  user data \p user_data is
 * that supplied to drbbdup_init().
 *
 * The user can use thread allocation for storing the analysis result.
 *
 * The analysis data is destroyed via a #drbbdup_destroy_orig_analysis_t function.
 *
 * @return whether successful or an error code on failure.
 */
typedef void (*drbbdup_analyze_orig_t)(void *drcontext, void *tag, instrlist_t *bb,
                                       void *user_data, IN void **orig_analysis_data);

/**
 * Destroys analysis data \p orig_analysis_data.
 *
 * The function is not invoked by drbbdup if \p orig_analysis_data was set
 * to NULL by the the #drbbdup_analyze_orig_t function.
 *
 */
typedef void (*drbbdup_destroy_orig_analysis_t)(void *drcontext, void *user_data,
                                                void *orig_analysis_data);

/**
 * Conducts an analysis on a basic block with respect to a case with encoding \p encoding.
 * The result of the analysis needs to be stored in \p case_analysis_data.
 *
 * The user data \p user_data is that supplied to drbbdup_init(). Analysis data
 * \p orig_analysis_data that was conducted on the original bb is also provided.
 *
 * The user can use thread allocation for storing the analysis result.
 *
 * The analysis data is destroyed via a #drbbdup_analyze_case_t function.
 */
typedef void (*drbbdup_analyze_case_t)(void *drcontext, void *tag, instrlist_t *bb,
                                       uintptr_t encoding, void *user_data,
                                       void *orig_analysis_data,
                                       IN void **case_analysis_data);

/**
 * Destroys analysis data \p case_analysis_data for the case with encoding \p encoding.
 *
 * The function is not invoked by drbbdup if \p case_analysis_data was set to NULL by the
 * #drbbdup_analyze_case_t function.
 *
 * The  user data \p user_data is that supplied to drbbdup_init(). Analysis data
 * \p orig_analysis_data that was conducted on the original bb is also provided.
 *
 * \note The user should not destroy orig_analysis_data.
 */
typedef void (*drbbdup_destroy_case_analysis_t)(void *drcontext, uintptr_t encoding,
                                                void *user_data, void *orig_analysis_data,
                                                void *case_analysis_data);

/**
 * Inserts code responsible for encoding the current runtime
 * case. The function should store the resulting pointer-sized encoding to the memory
 * destination operand obtained via drbbdup_get_encoding_opnd(). If the user has
 * implemented the encoder via a clean call, drbbdup_set_encoding() should be
 * used instead.
 *
 * The user data \p user_data is that supplied to drbbdup_init(). Analysis data
 * \p orig_analysis_data that was conducted on the original bb is also provided.
 */
typedef void (*drbbdup_insert_encode_t)(void *drcontext, void *tag, instrlist_t *bb,
                                        instr_t *where, void *user_data,
                                        void *orig_analysis_data);

/**
 * A user-defined call-back function that is invoked to instrument an instruction \p
 * instr. The inserted code must be placed at \p where.
 *
 * Instrumentation must be driven according to the passed case encoding \p encoding.
 *
 * The user data \p user_data is that supplied to drbbdup_init(). Analysis data
 * \p orig_analysis_data and \p case_analysis_data are also provided.
 *
 */
typedef void (*drbbdup_instrument_instr_t)(void *drcontext, void *tag, instrlist_t *bb,
                                           instr_t *instr, instr_t *where,
                                           uintptr_t encoding, void *user_data,
                                           void *orig_analysis_data,
                                           void *case_analysis_data);

/***************************************************************************
 * INIT
 */

/**
 * Specifies the options when initialising drbbdup. \p set_up_bb_dups, \p insert_encode
 * and \p instrument_instr cannot be NULL, while \p dup_limit must be greater than zero.
 */
typedef struct {
    /** Set this to the size of this structure. */
    size_t struct_size;
    /**
     * A user-defined call-back function that sets up how to duplicate a basic block.
     * Cannot be NULL.
     */
    drbbdup_set_up_bb_dups_t set_up_bb_dups;
    /**
     * A user-defined call-back function that inserts code to encode the runtime case.
     * The resulting encoding is used by the dispatcher to direct control to the
     * appropriate basic block. Cannot be NULL.
     */
    drbbdup_insert_encode_t insert_encode;
    /**
     * A user-defined call-back function that conducts an analysis of the original basic
     * block.
     */
    drbbdup_analyze_orig_t analyze_orig;
    /**
     * A user-defined call-back function that destroys analysis data of the original basic
     * block.
     */
    drbbdup_destroy_orig_analysis_t destroy_orig_analysis;
    /**
     * A user-defined call-back function that analyzes a basic block for a particular
     * case.
     */
    drbbdup_analyze_case_t analyze_case;
    /**
     * A user-defined call-back function that destroys analysis data for a particular
     * case.
     */
    drbbdup_destroy_case_analysis_t destroy_case_analysis;
    /**
     * A user-defined call-back function that instruments an instruction with respect to a
     * particular case.
     */
    drbbdup_instrument_instr_t instrument_instr;
    /**
     * User-data made available to user-defined call-back functions that drbbdup invokes
     * to manage basic block duplication.
     */
    void *user_data;
    /**
     * The maximum number of cases, excluding the default case, that can be associated
     * with a basic block. Once the limit is reached and an unhandled case is encountered,
     * control is directed to the default case.
     */
    ushort dup_limit;
} drbbdup_options_t;

/**
 * Priorities of drmgr instrumentation passes used by drbbdup. Users
 * can perform app2app manipulations prior to duplication
 * by ordering such changes before #DRMGR_PRIORITY_DRBBDUP.
 */
enum {
    /** Priority of drbbdup. */
    DRMGR_PRIORITY_DRBBDUP = -1500
};

/**
 * Name of drbbdup priorities for analysis and insert steps.
 */
#define DRMGR_PRIORITY_NAME_DRBBDUP "drbbdup"

DR_EXPORT
/**
 * Initialises the drbbdup extension. Must be called before use of any other routines.
 *
 * It cannot be called multiple times as duplication management is specific to a single
 * use-case where only one default case encoding is associated with each basic block.
 *
 * The \p ops_in parameter is a set of options which dictate how drbbdup manages
 * basic block copies.
 *
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_init(drbbdup_options_t *ops_in);

DR_EXPORT
/**
 * Cleans up the drbbdup extension.
 *
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_exit(void);

/***************************************************************************
 * ENCODING
 */

DR_EXPORT
/**
 * Registers a non-default case encoding \p encoding. The function should only be called
 * by a #drbbdup_set_up_bb_dups_t call-back function which provides \p drbbdup_ctx.
 *
 * The same encoding cannot be registered more than once.
 *
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_register_case_encoding(void *drbbdup_ctx, uintptr_t encoding);

DR_EXPORT
/**
 * Sets the runtime case encoding \p encoding.
 *
 * Must be called from a clean call inserted via a drbbdup_insert_encode_t call-back
 * function.
 */
drbbdup_status_t
drbbdup_set_encoding(uintptr_t encoding);

DR_EXPORT
/**
 * Retrieves a memory destination operand which should be used to set the runtime case
 * encoding.
 *
 * Must be called from code stemming from a drbbdup_insert_encode_t call-back function.
 *
 * @return a destination operand that refers to a memory location where the encoding
 * should be stored.
 */
opnd_t
drbbdup_get_encoding_opnd();

DR_EXPORT
/**
 * Indicates whether the instruction \p instr is the first instruction of
 * the currently considered basic block copy. The result is returned in \p is_start.
 *
 * Must be called via a #drbbdup_instrument_instr_t call-back function.
 *
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_is_first_instr(void *drcontext, instr_t *instr, OUT bool *is_start);

DR_EXPORT
/**
 * Indicates whether the instruction \p instr is the last instruction of the currently
 * considered basic block copy. The result is returned in \p is_last.
 *
 * Must be called via a #drbbdup_instrument_instr_t call-back function.
 *
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_is_last_instr(void *drcontext, instr_t *instr, OUT bool *is_last);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRBBDUP_H_ */
