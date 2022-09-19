/* Builtins definitions for RISC-V 'V' Extension for GNU compiler.
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

#ifndef GCC_RISCV_V_BUILTINS_H
#define GCC_RISCV_V_BUILTINS_H

namespace riscv_vector {

/* Static information about each vector type.  */
struct vector_type_info
{
  /* The name of the type as declared by riscv_vector.h
     which is recommend to use. For example: 'vint32m1_t'.  */
  const char *user_name;

  /* ABI name of vector type. The type is always available
     under this name, even when riscv_vector.h isn't included.
     For example:  '__rvv_int32m1_t'.  */
  const char *abi_name;

  /* The C++ mangling of ABI_NAME.  */
  const char *mangled_name;
};

/* Enumerates the RVV types, together called
   "vector types" for brevity.  */
enum vector_type_index
{
#define DEF_RVV_TYPE(USER_NAME, ABI_NAME, NCHARS, ARGS...)    \
  VECTOR_TYPE_##USER_NAME,
#include "riscv-vector-builtins.def"
  NUM_VECTOR_TYPES
};

extern machine_mode vector_modes[NUM_VECTOR_TYPES];

void init_builtins ();
const char *mangle_builtin_type (const_tree);
bool verify_type_context (location_t, type_context_kind, const_tree, bool);

} // end namespace riscv_vector

#endif