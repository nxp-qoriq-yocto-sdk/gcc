/*
 Type Widening:
  
 Locals and temporaries having signed integral types, whose address has
 not been taken, are not volatile qualified, and having type precision
 less than that of type long are widened to type long (with any other
 qualifiers retained).
 
   Copyright (C) 2011
   Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "tm_p.h"
#include "gimple.h"
#include "basic-block.h"
#include "tree-iterator.h"
#include "tree-inline.h"
#include "langhooks.h"
#include "tree-pretty-print.h"
#include "gimple-pretty-print.h"
#include "langhooks.h"
#include "tree-flow.h"
#include "cgraph.h"
#include "timevar.h"
#include "hashtab.h"
#include "flags.h"
#include "function.h"
#include "output.h"
#include "ggc.h"
#include "tree-dump.h"
#include "tree-pass.h"
#include "diagnostic-core.h"
#include "target.h"
#include "pointer-set.h"
#include "splay-tree.h"
#include "vec.h"
#include "gimple.h"
#include "tree-pass.h"

#include "langhooks-def.h"
#include "expr.h"

#include "except.h"
#include "value-prof.h"
#include "pointer-set.h"

/* define TW_FINALIZE_STMTS to 1, if you want to run the widening
 * pass just after gimplification - over the sequence of statements.
 */
#define TW_FINALIZE_STMTS 1

#define TW_DEBUG 0
#if TW_DEBUG

#define TWDBG_STMT(stmt) fprintf (stderr, "%s: ", __FUNCTION__); \
                         debug_gimple_stmt (stmt);

#define TWDBG_TREE(tree)            \
{                                   \
  fprintf (stderr, "%s:\n", #tree); \
  debug_tree (tree);                \
  fprintf (stderr, "\n");           \
}

#define TWDBG_MSG(fmt) \
fprintf (stderr, "%s: ", __FUNCTION__); \
fprintf (stderr, fmt)

#define TWDBG_MSG1(fmt, msg) \
fprintf (stderr, "%s: ", __FUNCTION__); \
fprintf (stderr, fmt, msg)

#else
#define TWDBG_STMT(stmt)
#define TWDBG_TREE(tree)
#define TWDBG_MSG(fmt)
#define TWDBG_MSG1(fmt, msg)
#endif

#if TW_DEBUG
static void tw_dump_candidate_list (void);
static bool tw_debug_candidate (const void *t, void **candidacy, void *data);
#endif
static void tw_init (void);
static void tw_reset (void);
static long tw_candidate (tree node);
static long tw_candidate_const (tree node);
static long *tw_log_candidate (tree node);
static long tw_candidacy_valid (tree node);
static void tw_candidacy (tree node, long value);
static long tw_in_candidate_list (tree node);
static tree tw_widen_constant (tree node);
static tree tw_widen_variable (tree node);
#ifdef TW_FINALIZE_STMTS
static long tw_fn_has_openmp (gimple_seq stmts);
#endif
static void tw_log_parms (tree fndecl);
#ifdef TW_FINALIZE_STMTS
static void tw_log_vars (gimple_seq stmts);
#endif
static void tw_log_local_decls (void);
#ifdef TW_FINALIZE_STMTS
static unsigned int tw_finalize_stmts (void);
#endif
static unsigned int tw_finalize_bbs (void);
static long tw_gimple_in_seq (gimple_seq stmts, long widen);
static long tw_gimple_in_bb (basic_block bb, long widen);
static long tw_switch (gimple stmt, long widen);
static long tw_gimple_stmt (gimple stmt, long widen);
static long tw_gimple_assign (gimple stmt, long widen);
static long tw_gimple_assign_single (gimple stmt, long widen);
static long tw_gimple_assign_unary (gimple stmt, long widen);
static long tw_gimple_assign_binary (gimple stmt, long widen);
static long tw_gimple_assign_ternary (gimple stmt, long widen);
static bool is_formatted_IO_fn (tree decl);
static long tw_gimple_call (gimple stmt, long widen);
static long tw_gimple_comparison (gimple stmt, long widen);
static long tw_gimple_switch (gimple stmt, long widen);
static long tw_gimple_return (gimple stmt, long widen);
static long tw_gimple_asm (gimple stmt, long widen);
static long tw_gimple_debug (gimple stmt, long widen);

static struct pointer_map_t *tw_candidate_list;

#if TW_DEBUG
static void
tw_dump_candidate_list (void)
{
  TWDBG_MSG ("Dumping candidate list:\n"); 
  pointer_map_traverse (tw_candidate_list, tw_debug_candidate, NULL);
  TWDBG_MSG ("Done dumping candidate list\n"); 
}

static
bool tw_debug_candidate (const void *t, void **candidacy, void *data)
{
  debug_tree (t);
  fprintf(stderr, "candidacy: %ld\n data (ignore): %p", *((long *) candidacy), data);
  return true;
}
#endif

static void
tw_init (void)
{
  gcc_assert (tw_candidate_list == NULL);
  tw_candidate_list = pointer_map_create ();
}

static void
tw_reset (void)
{
  if (tw_candidate_list)
    {
      pointer_map_destroy (tw_candidate_list);
      tw_candidate_list = NULL;
    }
}

/* gcc.dg/torture/pr43879_[12].c
 * Initialized statics should not be widened:
 *
 * void bar(int c)
 * {
 *  static int x = 1; // if widened, x gets initialized to (2^32)
 *  if (c != x) __builtin_abort();
 *   x--;
 * }
 *
 *  int main()
 *  {
 *   int c = 1;
 *   bar (1);
 *   return 0;
 *  }
 *
 * Likely, the initial value is laid out/translated to RTL way before
 * the rest of the code is translated to GIMPLE; so when we widen the
 * type, it's already too late.
 */

/* tw_candidate() has no way to tell if it was passed a local variable
 * (or not) - so make sure it is passed local variables or parameters only.
 */
static long
tw_candidate (tree node)
{
  long rv = 0;

  if (!node || TREE_TYPE (node) == error_mark_node)
    return 0;

  if (node && TREE_TYPE (node) != error_mark_node &&
      ((TREE_CODE (node) == VAR_DECL &&
        /* See note: Initialized statics should not be widened. */
        (!TREE_STATIC (node) || !DECL_INITIAL (node))) ||
       TREE_CODE (node) == PARM_DECL ||
       TREE_CODE (node) == DEBUG_EXPR_DECL) &&
      !TYPE_VOLATILE (TREE_TYPE (node)) &&
      !TREE_ADDRESSABLE (node) &&
      !POINTER_TYPE_P (TREE_TYPE (node)) &&
      INTEGRAL_TYPE_P (TREE_TYPE (node)) &&
      !TYPE_UNSIGNED (TREE_TYPE (node))  &&
      (TYPE_PRECISION (TREE_TYPE (node)) < TYPE_PRECISION (long_integer_type_node)))
    rv = 1;
  return rv;
}

static long
tw_candidate_const (tree node)
{
  long rv = 0;

  if (node && TREE_TYPE (node) != error_mark_node &&
      INTEGRAL_TYPE_P (TREE_TYPE (node)) &&
      TREE_CONSTANT (node) &&
      (TYPE_PRECISION (TREE_TYPE (node)) < TYPE_PRECISION (long_integer_type_node)))
    rv = 1;
  return rv;
}

static long *
tw_log_candidate (tree node)
{
  long *pval = NULL;

  if (tw_candidate_list && node && TREE_TYPE (node) != error_mark_node)
    {
      pval = (long *) pointer_map_contains (tw_candidate_list, node);
      if (!pval)
        {
          pval = (long *) pointer_map_insert (tw_candidate_list, node);
          *pval = 1;
          TWDBG_MSG ("Logged variable:\n");
          TWDBG_TREE (node);
        }
    }
  return pval; 
}

static long
tw_candidacy_valid (tree node)
{
  long rval = 0;
  long *pval = NULL;

  if (tw_candidate_list && node && TREE_TYPE (node) != error_mark_node)
    pval = (long *) pointer_map_contains (tw_candidate_list, node); 
  if (pval)
    rval = *pval ? 1 : 0;
  return rval;
}

static void
tw_candidacy (tree node, long value)
{
  long *pval;

  if (tw_candidate_list && node)
    {
      pval = (long *) pointer_map_contains (tw_candidate_list, node);
      if (pval)
        {
          *pval = value;
#if TW_DEBUG
          fprintf (stderr, "Setting candidacy of node:\n");
          TWDBG_TREE (node);
          fprintf (stderr, "to: %ld\n", value);
#endif
        }
    }
}

static long
tw_in_candidate_list (tree node)
{
  long *pval;
  long rval = 0;

  if (tw_candidate_list && node && TREE_TYPE (node) != error_mark_node)
    {
     pval = (long *) pointer_map_contains (tw_candidate_list, node);
     rval = pval ? 1 : 0;
    }
  return rval;
}

static tree
tw_widen_constant (tree node)
{
  if (node && tw_candidate_const (node))
    node = build_int_cst (long_integer_type_node, TREE_INT_CST_LOW (node));

  return node;
}

static tree
tw_widen_variable (tree node)
{
  if (node && tw_candidacy_valid (node))
  {
    TWDBG_MSG ("Widening:\n");
    TWDBG_TREE(node);

    TREE_TYPE (node) = build_qualified_type (long_integer_type_node,
                                             TYPE_QUALS (TREE_TYPE (node)));

    if (TREE_CODE (node) != DEBUG_EXPR_DECL)
      relayout_decl (node);
  }
  return node;
}

#ifdef TW_FINALIZE_STMTS
static long
tw_fn_has_openmp (gimple_seq stmts)
{
  gimple_stmt_iterator ittr;
  long found_openmp = 0;

  for (ittr = gsi_start (stmts); !gsi_end_p (ittr) && !found_openmp; gsi_next (&ittr))
    {
      gimple stmt = gsi_stmt (ittr);

      switch (gimple_code (stmt))
        {
	case GIMPLE_BIND:
	  found_openmp = tw_fn_has_openmp (gimple_bind_body (stmt));
	  break;

	case GIMPLE_TRY:
	  found_openmp = tw_fn_has_openmp (gimple_try_eval (stmt));
	  found_openmp = tw_fn_has_openmp (gimple_try_cleanup (stmt));
	  break;

	case GIMPLE_EH_FILTER:
	  found_openmp = tw_fn_has_openmp (gimple_eh_filter_failure (stmt));
	  break;

	case GIMPLE_CATCH:
	  found_openmp = tw_fn_has_openmp (gimple_catch_handler (stmt));
	  break;

	default:
          switch (gimple_code (stmt))
            {
            CASE_GIMPLE_OMP:
            found_openmp = 1;
            break;
            default:
            break;
            }
	}
    }
  return found_openmp;  
}
#endif

/* Originally, we implemented type widening over the emitted GIMPLE
 * sequence. Later on, we discovered that we needed to wait till
 * after OpenMP expansion, so we implemented type widening over the
 * CFG-BB form.
 */
#ifdef TW_FINALIZE_STMTS
struct gimple_opt_pass pass_widen_types_stmts =
{
 {
  GIMPLE_PASS,
  "tw-stmts",				/* name */
  NULL,					/* gate */
  tw_finalize_stmts,			/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_NONE,				/* tv_id */
  PROP_gimple_any, 			/* properties_required */
  0,			                /* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  0                                     /* todo_flags_finish */
 }
};
#endif

struct gimple_opt_pass pass_widen_types_bbs =
{
 {
  GIMPLE_PASS,
  "tw-bbs",				/* name */
  NULL,					/* gate */
  tw_finalize_bbs,			/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_NONE,				/* tv_id */
  PROP_gimple_any, 			/* properties_required */
  0,			                /* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  0                                     /* todo_flags_finish */
 }
};

static
void
tw_log_parms (tree fndecl)
{
  tree parm;

  if (!fndecl)
    return;
  for (parm = DECL_ARGUMENTS (fndecl); parm; parm = DECL_CHAIN (parm))
    if (tw_candidate (parm))
      tw_log_candidate (parm);
  return;
}

#ifdef TW_FINALIZE_STMTS
static
void
tw_log_vars (gimple_seq stmts)
{
  gimple_stmt_iterator ittr;
  tree vars, vindex;

  if (!stmts)
    return;

  gcc_assert (tw_candidate_list != NULL);

  for (ittr = gsi_start (stmts); !gsi_end_p (ittr); gsi_next (&ittr))
    {
      gimple stmt = gsi_stmt (ittr);

      switch (gimple_code (stmt))
        {
	case GIMPLE_BIND:
          vars = gimple_bind_vars (stmt);
          for (vindex = vars; vindex; vindex = DECL_CHAIN (vindex))
            if (tw_candidate (vindex))
              tw_log_candidate (vindex);
          tw_log_vars (gimple_bind_body (stmt));
	  break;

	case GIMPLE_TRY:
	  tw_log_vars (gimple_try_eval (stmt));
	  tw_log_vars (gimple_try_cleanup (stmt));
	  break;

	case GIMPLE_EH_FILTER:
	  tw_log_vars (gimple_eh_filter_failure (stmt));
	  break;

	case GIMPLE_CATCH:
	  tw_log_vars (gimple_catch_handler (stmt));
	  break;

	default:
          break;
	}
    }

  return;
}
#endif

static
void
tw_log_local_decls (void)
{
  tree decl;
  unsigned ix;

  FOR_EACH_LOCAL_DECL (cfun, ix, decl)
    {
      TWDBG_MSG ("Testing decl:\n");
      TWDBG_TREE (decl);
      if (tw_candidate (decl))
        tw_log_candidate (decl);
    }
}

/* Widen types. tw_finalize_stmts () can be run anytime immediately after
 * gimplification but before the CFG pass (see comment * accompanying
 * gimple_body ()).
 *
 * After gimplification has occurred, the emitted GIMPLE is
 * scanned to check if these variables are only used among
 * themselves (with the exception of being cast to unsigned long);
 * invalidating the candidacy of any variable that is used with
 * another outside this set (and so on recursively). The variables
 * that remain after this process all occur in operations with other
 * such candidate variables, (or with constants) - the type of all
 * such residual candidate variables (and of constants that appear
 * with these in operations) is changed to long (along with the
 * original accompannying qualifiers on the type).
 *
 * void
 * init_optimization_passes (void)
 *
 * p = &all_lowering_passes;
 * NEXT_PASS (pass_widen_types_stmts);
 * NEXT_PASS (pass_warn_unused_result);
 */
#ifdef TW_FINALIZE_STMTS
static
unsigned int
tw_finalize_stmts (void)
{
  long iv = 0;
  gimple_seq stmts;
  tree fndecl = current_function_decl; 

  if (strcmp (lang_hooks.name, "GNU C") != 0 ||
      seen_error () ||
      !flag_strict_overflow ||
      !flag_widen_types)
    {
      TWDBG_MSG ("Skipping: Language not C or seen error or -fno-strict-overflow or -fno-widen-types\n");
      return 0;
    }

  /* gcc.dg/pr23518.c execution test */ 
  if (flag_wrapv)
    {
      TWDBG_MSG ("Skipping: -fwrapv specified.\n");
      return 0;
    }

  if (debug_info_level == DINFO_LEVEL_NONE)
    {
      /* PS: That we cannot call relayout_decl () on DEBUG_EXPR_DECL is an
      * issue: Debug information is generated after lowering from tree to 
      * GIMPLE; unless we widen before debug information is generated, the
      * debug information will record pre-widening information - and that
      * cannot be changed because relayout_decl () cannot be invoked on 
      * DEBUG_EXPR_DECL. expand_debug_locations () during cfgexpand will
      * fail gcc_assert ()'s on the DEBUG_INSN's since e.g. the modes will
      * not agree, etc. So if we are compiling -g, we ought to run the 
      * pass_widen_types_stmts.
      *
      * In short: pass_widen_types_stmts runs iff we're generating debug
      *           information.
      */
      TWDBG_MSG ("Skipping: Debug level none.\n");
      return 0;
    }
  gcc_assert (debug_info_level != DINFO_LEVEL_NONE);

  if (!fndecl)
    {
      TWDBG_MSG ("Skipping: !fndecl.\n");
      return 0;
    }

  TWDBG_MSG ("Widening function:\n");
  TWDBG_TREE (fndecl);

  stmts = gimple_body (fndecl);

  if (!stmts)
    {
      TWDBG_MSG ("Skipping: !stmts.\n");
      return 0;
    }

  if (tw_fn_has_openmp (stmts))
    {
      TWDBG_MSG ("Skipping: OpenMP stmts found.\n");
      return 0;
    }

  /* Assume for now that we do not need to check for nested functions:
   * (cgraph_get_node (fndecl) && cgraph_get_node (fndecl)->nested != NULL) ||
   * TREE_CODE (DECL_CONTEXT (fndecl)) != TRANSLATION_UNIT_DECL ||
   * Well, turns out that a nested function is processed only if it is 
   * actually invoked from within the function, so we are in good shape.
   */

  tw_init ();
  tw_log_parms (fndecl);
  tw_log_vars (stmts);
#if TW_DEBUG
  tw_dump_candidate_list ();
#endif

  do
    {
      iv = tw_gimple_in_seq (stmts, 0);
    } while (iv);
  tw_gimple_in_seq (stmts, 1);
  verify_gimple_in_seq (stmts);

  tw_reset ();

  return 0;
}
#endif

static
unsigned int
tw_finalize_bbs (void)
{
  long iv = 0;
  basic_block bb;
  tree fndecl;

  if (strcmp (lang_hooks.name, "GNU C") != 0 ||
      seen_error () ||
      !flag_strict_overflow ||
      !flag_widen_types)
    {
      TWDBG_MSG ("Skipping: Language not C or seen error or -fno-strict-overflow or -fno-widen-types\n");
      return 0;
    }
 
  /* gcc.dg/pr23518.c execution test */ 
  if (flag_wrapv)
    {
      TWDBG_MSG ("Skipping: -fwrapv specified.\n");
      return 0;
    }

  if (debug_info_level != DINFO_LEVEL_NONE && flag_openmp)
    {
      /* Cannot run this pass as the debug information has already
       * been recorded; If we type widen now, it'll lead to assert
       * failures during RTL expansion in expandcfg.c since the
       * debug information would all be prewidening and would
       * mismatch with the postwidening information for the variables
       * that got widened (but whoose debug information was already
       * generated). This is all because we cannot call relayout_decl ()
       * on DEBUG_EXPR_DECL's - see note elsewhere.
       */
      TWDBG_MSG ("Skipping: Non-zero debug level and -fopenmp specified.\n");
      return 0;
    }

  if (!cfun || !(fndecl = cfun->decl) || !(cfun->cfg))
    {
      TWDBG_MSG ("Skipping: !cfun or !fndecl or !(cfun->cfg).\n");
      return 0;
    }

  TWDBG_MSG ("Widening function:\n");
  TWDBG_TREE (fndecl);

  /* Assume for now that we do not need to check for nested functions:
   * (cgraph_get_node (fndecl) && cgraph_get_node (fndecl)->nested != NULL) ||
   * TREE_CODE (DECL_CONTEXT (fndecl)) != TRANSLATION_UNIT_DECL ||
   * Well, turns out that a nested function is processed only if it is 
   * actually invoked from within the function, so we are in good shape.
   */

  tw_init ();
  tw_log_parms (fndecl);
  tw_log_local_decls (); 
#if TW_DEBUG
  tw_dump_candidate_list ();
#endif

  do
    {
      iv = 0;
      FOR_EACH_BB (bb)
        iv += tw_gimple_in_bb (bb, 0);
    } while (iv);
  FOR_EACH_BB (bb)
    tw_gimple_in_bb (bb, 1);
  FOR_EACH_BB (bb)
    verify_gimple_in_seq (bb_seq (bb));

  tw_reset ();

  return 0;
}

/* Assumes that we have run verify_gimple_in_seq (stmts)
 * i.e. that we have valid gimple.
 */
static long
tw_gimple_in_seq (gimple_seq stmts, long widen)
{
  gimple_stmt_iterator ittr;
  long iv = 0;

  for (ittr = gsi_start (stmts); !gsi_end_p (ittr); gsi_next (&ittr))
    {
      gimple stmt = gsi_stmt (ittr);
      iv += tw_switch (stmt, widen);
    }
  return iv;  
}

static long
tw_gimple_in_bb (basic_block bb, long widen)
{
  gimple_stmt_iterator ittr;
  long iv = 0;

#if TW_DEBUG
  fprintf (stderr, "Dumping basic block (widen = %ld):\n", widen);
  debug_bb (bb);
  fprintf (stderr, "Done dumping basic block (widen = %ld)\n", widen);
#endif
  for (ittr = gsi_start_bb (bb); !gsi_end_p (ittr); gsi_next (&ittr))
    {
      gimple stmt = gsi_stmt (ittr);
      iv += tw_switch (stmt, widen);
    }
  return iv;  
}

static long
tw_switch (gimple stmt, long widen)
{
  long iv = 0;

  switch (gimple_code (stmt))
    {
    case GIMPLE_BIND:
      iv += tw_gimple_in_seq (gimple_bind_body (stmt), widen);
      break;

    case GIMPLE_TRY:
      iv += tw_gimple_in_seq (gimple_try_eval (stmt), widen);
      iv += tw_gimple_in_seq (gimple_try_cleanup (stmt), widen);
      break;

    case GIMPLE_EH_FILTER:
      iv += tw_gimple_in_seq (gimple_eh_filter_failure (stmt), widen);
      break;

    case GIMPLE_CATCH:
      iv += tw_gimple_in_seq (gimple_catch_handler (stmt), widen);
      break;

    default:
      iv += tw_gimple_stmt (stmt, widen);
      break;
    }
  return iv;  
}

/* tw_gimple_stmt () mimics verify_gimple_stmt ()
 */
static long
tw_gimple_stmt (gimple stmt, long widen)
{
  long iv = 0;

  switch (gimple_code (stmt))
    {
    case GIMPLE_ASSIGN:
      iv = tw_gimple_assign (stmt, widen);
      break;

    case GIMPLE_CALL:
      iv = tw_gimple_call (stmt, widen);
      break;

    case GIMPLE_COND:
      iv = tw_gimple_comparison (stmt, widen);
      break;

    case GIMPLE_SWITCH:
      iv = tw_gimple_switch (stmt, widen);
      break;

    case GIMPLE_RETURN:
      iv = tw_gimple_return (stmt, widen);
      break;

    case GIMPLE_LABEL:
      TWDBG_STMT(stmt);
      break;

    case GIMPLE_GOTO:
      TWDBG_STMT(stmt);
      break;

    case GIMPLE_ASM:
      iv = tw_gimple_asm (stmt, widen);
      break;

    /* Tuples that do not have tree operands.  */
    case GIMPLE_NOP:
    case GIMPLE_PREDICT:
    case GIMPLE_RESX:
    case GIMPLE_EH_DISPATCH:
    case GIMPLE_EH_MUST_NOT_THROW:
      TWDBG_STMT(stmt);
      break;

    CASE_GIMPLE_OMP:
      TWDBG_STMT(stmt);
      break;

    case GIMPLE_DEBUG:
      iv = tw_gimple_debug (stmt, widen);
      break;

    default:
      gcc_unreachable ();
    }
  return iv;  
}

static long
tw_gimple_assign (gimple stmt, long widen)
{
  long iv = 0;

  switch (gimple_assign_rhs_class (stmt))
    {
    case GIMPLE_SINGLE_RHS:
      iv = tw_gimple_assign_single (stmt, widen);
      break;

    case GIMPLE_UNARY_RHS:
      iv = tw_gimple_assign_unary (stmt, widen);
      break;

    case GIMPLE_BINARY_RHS:
      iv = tw_gimple_assign_binary (stmt, widen);
      break;

    case GIMPLE_TERNARY_RHS:
      iv = tw_gimple_assign_ternary (stmt, widen);
      break;

    default:
      gcc_unreachable ();
    }
  return iv;  
}

static long
tw_gimple_assign_single (gimple stmt, long widen)
{
  tree lhs = gimple_assign_lhs (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  tree value;
  long iv = 0;
  unsigned int idx;

  TWDBG_STMT(stmt);
  TWDBG_TREE(lhs);
  TWDBG_TREE(rhs1);

  if (widen)
    {
      TWDBG_MSG ("Widening run.\n");
      if (TREE_CODE (rhs1) == ARRAY_REF &&
          tw_candidacy_valid (TREE_OPERAND (rhs1, 1)))
        {
          /* Note that we are widening the array index, hence no 
           * gimple_assign_set_rhs1 () */
          tw_widen_variable (TREE_OPERAND (rhs1, 1));
        }
      else if (TREE_CODE (lhs) == ARRAY_REF &&
          tw_candidacy_valid (TREE_OPERAND (lhs, 1)))
        {
          /* Note that we are widening the array index, hence no 
           * gimple_assign_set_lhs () */
          tw_widen_variable (TREE_OPERAND (lhs, 1));
        }
      else if (tw_candidacy_valid (lhs) && tw_candidate_const (rhs1))
        {
          gimple_assign_set_lhs (stmt, tw_widen_variable (lhs));
          gimple_assign_set_rhs1 (stmt, tw_widen_constant (rhs1));
        }
      else if (tw_candidacy_valid (lhs) && tw_candidacy_valid (rhs1))
        {
          gimple_assign_set_lhs (stmt, tw_widen_variable (lhs));
          gimple_assign_set_rhs1 (stmt, tw_widen_variable (rhs1));
        }
    }
  else
    {
      TWDBG_MSG ("Validating run.\n");
      if (tw_candidacy_valid (lhs) && tw_candidate_const (rhs1))
        return iv;
      if (tw_candidacy_valid (lhs) && tw_candidacy_valid (rhs1))
        return iv;
      if (TREE_CODE (lhs) == VAR_DECL && TREE_CODE (TREE_TYPE (lhs)) == VECTOR_TYPE &&
          TREE_CODE (rhs1) == CONSTRUCTOR && TREE_CODE (TREE_TYPE (rhs1)) == VECTOR_TYPE)
        { 
          FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (rhs1), idx, value)
            {
              if (tw_candidacy_valid (value))
                {
                  TWDBG_MSG ("Invalidating candidacy of constructor element:\n");
                  TWDBG_TREE (value);
                  tw_candidacy (value, 0);
                  iv++;
                }
            }
        }
      if (tw_candidacy_valid (lhs))
        {
          tw_candidacy (lhs, 0);
          iv++;
        }
      if (tw_candidacy_valid (rhs1))
        {
          tw_candidacy (rhs1, 0);
          iv++;
        }
    }
  return iv;
}

static long
tw_gimple_assign_unary (gimple stmt, long widen)
{
  tree lhs = gimple_assign_lhs (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  long iv = 0;

  TWDBG_STMT(stmt);
  TWDBG_TREE(lhs);
  TWDBG_TREE(rhs1);
  
  if (widen)
    {
      TWDBG_MSG ("Widening run.\n");
      if (gimple_assign_rhs_code (stmt) == NOP_EXPR &&
          tw_candidacy_valid (rhs1) &&
          (TREE_TYPE (lhs) == long_unsigned_type_node ||
           TREE_TYPE (lhs) == long_integer_type_node))
        gimple_assign_set_rhs1 (stmt, tw_widen_variable (rhs1));
    }
  else
    {
      TWDBG_MSG ("Validating run.\n");
      if (gimple_assign_rhs_code (stmt) == NOP_EXPR &&
          tw_candidacy_valid (rhs1) &&
          (TREE_TYPE (lhs) == long_unsigned_type_node ||
           TREE_TYPE (lhs) == long_integer_type_node))
        return iv;
      if (tw_candidacy_valid (lhs))
        {
          tw_candidacy (lhs, 0);
          iv++;
        }
      if (tw_candidacy_valid (rhs1))
        {
          tw_candidacy (rhs1, 0);
          iv++;
        }
    }
    return iv;
}

static long
tw_gimple_assign_binary (gimple stmt, long widen)
{
  tree lhs = gimple_assign_lhs (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  tree rhs2 = gimple_assign_rhs2 (stmt);
  long iv = 0;

  TWDBG_STMT(stmt);
  TWDBG_TREE(lhs);
  TWDBG_TREE(rhs1);
  TWDBG_TREE(rhs2);

  if (widen)
    {
      TWDBG_MSG ("Widening run.\n");
      if (tw_candidacy_valid (lhs) && tw_candidacy_valid (rhs1) && tw_candidate_const (rhs2))
        {
          gimple_assign_set_lhs (stmt, tw_widen_variable (lhs));
          gimple_assign_set_rhs1 (stmt, tw_widen_variable (rhs1));
          gimple_assign_set_rhs2 (stmt, tw_widen_constant (rhs2));
        }
      else if (tw_candidacy_valid (lhs) && tw_candidacy_valid (rhs1) && tw_candidacy_valid (rhs2))
        {
          gimple_assign_set_lhs (stmt, tw_widen_variable (lhs));
          gimple_assign_set_rhs1 (stmt, tw_widen_variable (rhs1));
          gimple_assign_set_rhs2 (stmt, tw_widen_variable (rhs2));
        }
    }
  else
    {
      TWDBG_MSG ("Validating run.\n");
      if (tw_candidacy_valid (lhs) && tw_candidacy_valid (rhs1) && tw_candidate_const (rhs2))
        return iv;
      if (tw_candidacy_valid (lhs) && tw_candidacy_valid (rhs1) && tw_candidacy_valid (rhs2))
        return iv;
      if (tw_candidacy_valid (lhs))
        {
          tw_candidacy (lhs, 0);
          iv++;
        }
      if (tw_candidacy_valid (rhs1))
        {
          tw_candidacy (rhs1, 0);
          iv++;
        }
      if (tw_candidacy_valid (rhs2))
        {
          tw_candidacy (rhs2, 0);
          iv++;
        }
    }
    return iv;
}

static long
tw_gimple_assign_ternary (gimple stmt, long widen)
{
  tree lhs = gimple_assign_lhs (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  tree rhs2 = gimple_assign_rhs2 (stmt);
  tree rhs3 = gimple_assign_rhs3 (stmt);
  long iv = 0;

  TWDBG_STMT(stmt);
  TWDBG_TREE(lhs);
  TWDBG_TREE(rhs1);
  TWDBG_TREE(rhs2);
  TWDBG_TREE(rhs3);

  if (widen)
    {
     TWDBG_MSG ("Widening run.\n");
     return iv;
    }

  TWDBG_MSG ("Validating run.\n");
  if (tw_candidacy_valid (lhs))
    {
      tw_candidacy (lhs, 0);
      iv++;
    }
  if (tw_candidacy_valid (rhs1))
    {
      tw_candidacy (rhs1, 0);
      iv++;
    }
  if (tw_candidacy_valid (rhs2))
    {
      tw_candidacy (rhs2, 0);
      iv++;
    }
  if (tw_candidacy_valid (rhs3))
    {
      tw_candidacy (rhs3, 0);
      iv++;
    }
  return iv;
}

/* Ref. WG14/N1256 Committee Draft - September 7, 2007 ISO/IEC 9899:TC3
 * 7.19.6 Formatted input/output functions
 * Sec. 17.19.6.[1 ... 14]
 */
#define IO_FN_COUNT 14
static const char *IO_fn_names[IO_FN_COUNT] =
{
  "fprintf", "fscanf",
  "printf",  "scanf",
  "snprintf",
  "sprintf", "sscanf",
  "vfprintf", "vfscanf",
  "vprintf", "vscanf",
  "vsnprintf",
  "vsprintf", "vsscanf",
};

static bool
is_formatted_IO_fn (tree decl)
{
  const char *fn_name;
  long i;

  if (decl && TREE_CODE (decl) == FUNCTION_DECL &&
      DECL_NAME (decl) /* TREE_CODE (decl) == IDENTIFIER_NODE */ &&
      (fn_name = IDENTIFIER_POINTER (DECL_NAME (decl))))
    {
      for (i = 0; i < IO_FN_COUNT; i++)
        if (strcmp (IO_fn_names[i], fn_name) == 0)
          return true;
    }
  return false;
}

/* TODO: If you have:
 * 
 *  int i, n, f();
 *
 *  n = f();
 *
 *  for (i = 0; i < n; i++) 
 *    // stuff
 *
 *  then (after the candidate set stabilizes) do:
 *
 *  int n, f();
 *  long twl.n;
 *
 *  n = f();
 *  twl.n = (long) n;
 *
 *  for (i = 0; i < twl.n; i++) 
 *    // stuff
 *
 *  only if adding twl.n does not decrease the size of the stabilized
 *  candidate set or "cause any other damage".
 *
 * This should help in benchmarks where parameters are set via function
 * calls to prevent them from being optimized away.
 *
 */
static long
tw_gimple_call (gimple stmt, long widen)
{
#if TW_DEBUG
  tree fn = gimple_call_fn (stmt);
#endif
  long iv = 0;
  unsigned i;
  tree arg;

  TWDBG_STMT(stmt);
  TWDBG_TREE(fn);
  TWDBG_TREE(gimple_call_fndecl (stmt));

  if (widen)
    {
      TWDBG_MSG ("Widening run.\n");
      for (i = 0; i < gimple_call_num_args (stmt); i++)
        {
          arg = gimple_call_arg (stmt, i);
          if (arg && tw_candidacy_valid (arg))
            gimple_call_set_arg (stmt, i, tw_widen_variable (arg));
        }
      return iv;
    }

  TWDBG_MSG ("Validating run.\n");
  if (gimple_call_lhs (stmt) && tw_candidacy_valid (gimple_call_lhs (stmt)))
    {
      tw_candidacy (gimple_call_lhs (stmt), 0);
      iv++;
    }
  if (gimple_call_fndecl (stmt) &&
      (is_builtin_fn (gimple_call_fndecl (stmt)) ||
       is_formatted_IO_fn (gimple_call_fndecl (stmt))))
    {
      /* Certain types of function (printf-scanf family,
       * formatted IO functions, builtin functions) ought
       * not to have their args widened.
       *
       * e.g. A call to printf () such as:
       * int x;
       * printf ("%d", x);
       * because, we cannot change the %d to a %ld.
       *
       * or e.g. in:
       *
       * int D.2852;
       * int si;
       *
       * si = 2;
       * __builtin_altivec_dst (&vi, si, 0);
       * D.2852 = 0;
       *
       * si should not be widened.
       *
       * PS: We could generate code for casts to their original types in the
       *     call, but that would slow-down performance and we do not expect
       *     a loop index to be used in a call to a formatted IO function.
       */
      for (i = 0; i < gimple_call_num_args (stmt); i++)
        {
          arg = gimple_call_arg (stmt, i);
          if (arg && tw_candidacy_valid (arg))
            {
              tw_candidacy (arg, 0);
              iv++;
            }
        }
    }
  return iv;
}

static long
tw_gimple_comparison (gimple stmt, long widen)
{
  tree lhs = gimple_cond_lhs (stmt);
  tree rhs = gimple_cond_rhs (stmt);
  long iv = 0;

  TWDBG_STMT(stmt);
  TWDBG_TREE(lhs); 
  TWDBG_TREE(rhs); 

  if (widen)
    {
      TWDBG_MSG ("Widening run.\n");
      if (tw_candidacy_valid (lhs) && tw_candidate_const (rhs))
        {
          gimple_cond_set_lhs (stmt, tw_widen_variable (lhs));
          gimple_cond_set_rhs (stmt, tw_widen_constant (rhs));
        }
      else if (tw_candidate_const (lhs) && tw_candidacy_valid (rhs))
        {
          gimple_cond_set_lhs (stmt, tw_widen_constant (lhs));
          gimple_cond_set_rhs (stmt, tw_widen_variable (rhs));
        }
      else if (tw_candidacy_valid (lhs) && tw_candidacy_valid (rhs))
        {
          gimple_cond_set_lhs (stmt, tw_widen_variable (lhs));
          gimple_cond_set_rhs (stmt, tw_widen_variable (rhs));
        }
    }
  else
    {
      TWDBG_MSG ("Validating run.\n");
      if (tw_candidacy_valid (lhs) && tw_candidate_const (rhs))
        return iv;
      if (tw_candidate_const (lhs) && tw_candidacy_valid (rhs))
        return iv;
      if (tw_candidacy_valid (lhs) && tw_candidacy_valid (rhs))
        return iv;
      if (tw_candidacy_valid (lhs))
        {
          tw_candidacy (lhs, 0);
          iv++;
        }
      if (tw_candidacy_valid (rhs))
        {
          tw_candidacy (rhs, 0);
          iv++;
        }
    }
  return iv;
}

static long
tw_gimple_switch (gimple stmt, long widen)
{
  tree index = gimple_switch_index (stmt);
  long iv = 0;

  TWDBG_STMT(stmt);
  TWDBG_TREE(index); 

  if (widen && tw_candidacy_valid (index))
    {
      TWDBG_MSG ("Widening run.\n");
      gimple_switch_set_index (stmt, tw_widen_variable (index));
      return iv;
    }
   
  TWDBG_MSG ("Validating run.\n");
  return iv;
}

static long
tw_gimple_return (gimple stmt, long widen)
{
  tree op = gimple_return_retval (stmt);
  long iv = 0;

  TWDBG_STMT(stmt);
  TWDBG_TREE(op); 

  if (widen)
    {
      TWDBG_MSG ("Widening run.\n");
      return iv;
    }

  TWDBG_MSG ("Validating run.\n");
  if (tw_candidacy_valid (op))
    {
      tw_candidacy (op, 0);
      iv++;
    }
  return iv;
}

static long
tw_gimple_asm (gimple stmt, long widen)
{
  long iv = 0;
  unsigned int ninputs = gimple_asm_ninputs (stmt);
  unsigned int noutputs = gimple_asm_noutputs (stmt);
  unsigned int nclobbers = gimple_asm_nclobbers (stmt);
  unsigned int i;

  TWDBG_STMT(stmt);
  TWDBG_MSG("Inputs:\n");
  for (i = 0; i < ninputs; i++)
    {
      TWDBG_MSG1 ("input %d\n", i);
      TWDBG_TREE (gimple_asm_input_op (stmt, i));
    }
  TWDBG_MSG("Outputs:\n");
  for (i = 0; i < noutputs; i++)
    {
      TWDBG_MSG1 ("output %d\n", i);
      TWDBG_TREE (gimple_asm_output_op (stmt, i));
    }
  TWDBG_MSG("Clobbers:\n");
  for (i = 0; i < nclobbers; i++)
    {
      TWDBG_MSG1 ("clobber %d\n", i);
      TWDBG_TREE (gimple_asm_clobber_op (stmt, i));
    }
  if (widen)
    {
      TWDBG_MSG ("Widening run.\n");
      return iv;
    }
  TWDBG_MSG ("Validating run.\n");
  for (i = 0; i < ninputs; i++)
    {
      if (tw_candidacy_valid (gimple_asm_input_op (stmt, i)))
        {
          tw_candidacy (gimple_asm_input_op (stmt, i), 0);
          iv++;
        }
    }
  for (i = 0; i < noutputs; i++)
    {
      if (tw_candidacy_valid (gimple_asm_output_op (stmt, i)))
        {
          tw_candidacy (gimple_asm_output_op (stmt, i), 0);
          iv++;
        }
    }
  for (i = 0; i < nclobbers; i++)
    {
      if (tw_candidacy_valid (gimple_asm_clobber_op (stmt, i)))
        {
          tw_candidacy (gimple_asm_clobber_op (stmt, i), 0);
          iv++;
        }
    }
  return iv;
}

static long
tw_gimple_debug (gimple stmt, long widen)
{
  long iv = 0;
  tree var, value;

  var = gimple_debug_bind_get_var (stmt);
  value = gimple_debug_bind_get_value (stmt);

  TWDBG_STMT(stmt);
  TWDBG_TREE(var);
  TWDBG_TREE(value);

  /* TODO: What if the value is a constant? */
 
  if (widen)
    {
      TWDBG_MSG ("Widening run.\n");
      if (tw_candidacy_valid (var) && tw_candidacy_valid (value))
        {
          gimple_debug_bind_set_var (stmt, tw_widen_variable (var));
          gimple_debug_bind_set_value (stmt, tw_widen_variable (value));
        }
      else if (tw_candidacy_valid (var) && tw_candidate_const (value))
        {
          gimple_debug_bind_set_var (stmt, tw_widen_variable (var));
          gimple_debug_bind_set_value (stmt, tw_widen_constant (value));
        }
    }
  else
    {
      TWDBG_MSG ("Validating run.\n");

      if (var && !tw_in_candidate_list (var) && tw_candidate (var))
        tw_log_candidate (var);
      if (value && !tw_in_candidate_list (value) && tw_candidate (value))
        tw_log_candidate (value);
      if (tw_candidacy_valid (var) && tw_candidacy_valid (value))
        return iv;
      if (tw_candidacy_valid (var))
        {
          tw_candidacy (var, 0);
          iv++;
        }
      if (tw_candidacy_valid (value))
        {
          tw_candidacy (value, 0);
          iv++;
        }
    }
   
  return iv;
}

/* Notes:
 * ------
 *
 * Limitations to be documented:
 * 0. -g -fopenmp not supported.
 *
 * Known DejaGNU failures:
 * 0. FAIL: gcc.dg/pr38245-2.c scan-tree-dump-not optimized "link_error"
 *    This failure is because the optimization is dependent on the type of the variable;
 *    once the type of the variable has changed from int to long, the inequalities in
 *    this test case no longer hold and the code cannot be optimized anymore, consequently,
 *    the test fails.
 *
 * DejaGNU failures that are not due to type-widening:
 * 0. gcc.dg/vect/vect-120.c scan-tree-dump-times vect "vectorized 1 loops" 1
 * 1.gcc.dg/vect/vect-120.c -flto scan-tree-dump-times vect "vectorized 1 loops" 1
 *
 * DejaGNU PASS'es with -fwiden-types (but FAIL's in the baseline - the baseline needs
 * to be fixed - it just so happens that the debug information happens to be better in
 * the type-converted case. We have verified that the generated assembly is the same in
 * either case (or has extsw eliminated)):
 * gcc.dg/guality/pr45882.c
 * gcc.dg/guality/pr43177.c
 *
 */
