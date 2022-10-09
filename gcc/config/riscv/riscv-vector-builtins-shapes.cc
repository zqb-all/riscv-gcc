/* function_shape implementation for RISC-V 'V' Extension for GNU compiler.
   Copyright (C) 2022-2022 Free Software Foundation, Inc.
   Contributed by Ju-Zhe Zhong (juzhe.zhong@rivai.ai), RiVAI Technologies Ltd.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "memmodel.h"
#include "insn-codes.h"
#include "optabs.h"
#include "riscv-vector-builtins.h"
#include "riscv-vector-builtins-shapes.h"

namespace riscv_vector {

/* Add one function instance for GROUP, using operand suffix at index OI,
   mode suffix at index PAIR && bi and predication suffix at index pi.  */
static void
build_one (function_builder &b, const function_group_info &group,
	   unsigned int oi, unsigned int pi, const vector_type_field_pair &pair)
{
  /* Byte forms of non-tuple vlxusegei take 21 arguments.  */
  auto_vec<tree, 21> argument_types;
  function_instance function_instance (group.base_name, *group.base,
				       *group.shape, group.ops[oi], pair,
				       group.preds[pi]);
  tree return_type = (*group.shape)->get_return_type (pair);
  (*group.shape)->allocate_argument_types (function_instance, argument_types);
  b.add_unique_function (function_instance, (*group.shape), return_type,
			 argument_types);
}

/* Add function instances for all combination from relation.  */
static void
build_group (function_builder &b, relation_index relation,
	     const function_group_info &group, unsigned int oi, unsigned int pi,
	     vector_type_field type)
{
  if (relation == RELATION_ONE_ONE)
    {
      vector_type_field_pair pair = {type, type};
      build_one (b, group, oi, pi, pair);
    }
  else
    {
      for (unsigned int ti = 0; group.pairs[ti][1].type != NUM_VECTOR_TYPES;
	   ++ti)
	{
	  vector_type_field_pair pair = {type, group.pairs[ti][1]};
	  build_one (b, group, oi, pi, pair);
	}
    }
}

/* Add a function instance for every operand && type && predicate
   combination in GROUP.  Take the function base name from GROUP && operand
   suffix from operand_suffixes && mode suffix from type_suffixes && predication
   suffix from predication_suffixes. Use apply_predication to add in
   the predicate.  */
static void
build_all (function_builder &b, relation_index relation,
	   const function_group_info &group)
{
  for (unsigned int oi = 0; group.ops[oi] != NUM_OP_TYPES; ++oi)
    for (unsigned int pi = 0; group.preds[pi] != NUM_PRED_TYPES; ++pi)
      for (unsigned int ti = 0; group.pairs[ti][0].type != NUM_VECTOR_TYPES;
	   ++ti)
	build_group (b, relation, group, oi, pi, group.pairs[ti][0]);
}

/* Declare the function shape NAME, pointing it to an instance
   of class <NAME>_def.  */
#define SHAPE(NAME)                                                            \
  static CONSTEXPR const NAME##_def NAME##_obj;                                \
  namespace shapes {                                                           \
  const function_shape *const NAME = &NAME##_obj;                              \
  }

/* Base class for one to one functions.  */
struct one_one_base : public function_shape
{
  void build (function_builder &b,
	      const function_group_info &group) const override
  {
    build_all (b, RELATION_ONE_ONE, group);
  }
};

/* Base class for one to many functions.  */
struct one_many_base : public function_shape
{
  void build (function_builder &b,
	      const function_group_info &group) const override
  {
    build_all (b, RELATION_ONE_MANY, group);
  }
};

/* Base class for return 'size_t' instructions.  */
struct size_return_base : public one_one_base
{
  tree get_return_type (const vector_type_field_pair) const override
  {
    return size_type_node;
  }
};

/* configure_def class.  */
struct configure_def : public size_return_base
{
  char *get_name (function_builder &b, const function_instance &instance,
		  bool overloaded_p) const override
  {
    /* vsetvl* instruction doesn't have C++ overloaded functions.  */
    if (overloaded_p)
      return nullptr;
    b.append_name (instance.base_name);
    b.append_name (type_suffixes[instance.pair[0].type][BUILT_IN_VSETVL]);
    return b.finish_name ();
  }
};

/* vsetvl class.  */
struct vsetvl_def : public configure_def
{
  void allocate_argument_types (const function_instance &,
				vec<tree> &argument_types) const
  {
    argument_types.quick_push (size_type_node);
  }
};

SHAPE (configure)
SHAPE (vsetvl)
} // end namespace riscv_vector