/* This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "tree.h"
#include "stringpool.h"
#include "function.h"
#include "memmodel.h"
#include "emit-rtl.h"
#include "tm_p.h"
#include "expr.h"
#include "selftest.h"
#include "selftest-rtl.h"
#include <map>

using namespace selftest;
class riscv_selftest_arch_abi_setter
{
private:
  std::string m_arch_backup;
  enum riscv_abi_type m_abi_backup;

public:
  riscv_selftest_arch_abi_setter (const char *arch, enum riscv_abi_type abi)
    : m_arch_backup (riscv_arch_str ()), m_abi_backup (riscv_abi)
  {
    riscv_parse_arch_string (arch, &global_options, UNKNOWN_LOCATION);
    riscv_abi = abi;
    riscv_reinit ();
  }
  ~riscv_selftest_arch_abi_setter ()
  {
    riscv_parse_arch_string (m_arch_backup.c_str (), &global_options,
			     UNKNOWN_LOCATION);
    riscv_abi = m_abi_backup;
    riscv_reinit ();
  }
};

/* Return true if x is REG or SUBREG.  */
static bool
reg_or_subreg (rtx x)
{
  return REG_P (x) || SUBREG_P (x);
}

/* Return the REGNO of x.  */
static unsigned
regno (rtx x)
{
  gcc_assert (reg_or_subreg (x));
  return REGNO (REG_P (x) ? x : SUBREG_REG (x));
}

/* Search the worklist and return poly value of x.  */
poly_int64
get_value (std::map<int, poly_int64> worklist, rtx x)
{
  if (reg_or_subreg (x))
    {
      gcc_assert (worklist.find (regno (x)) != worklist.end ());
      return worklist[regno (x)];
    }
  else if (CONST_INT_P (x))
    return INTVAL (x);
  gcc_unreachable ();
}

/* Return the rtx_insn * if the REG_EQUAL for corresponding expression is found.
 */
static rtx_insn *
get_reg_equal_insn (enum rtx_code code, rtx_insn *insn, rtx op1, rtx op2)
{
  if (!SUBREG_P (op1) && !SUBREG_P (op2))
    return insn;
  rtx expr;
  switch (code)
    {
    case MULT:
      expr = gen_rtx_MULT (DImode, XEXP (op1, 0), XEXP (op2, 0));
      break;
    case PLUS:
      expr = gen_rtx_PLUS (DImode, XEXP (op1, 0), XEXP (op2, 0));
      break;
    case MINUS:
      expr = gen_rtx_MINUS (DImode, XEXP (op1, 0), XEXP (op2, 0));
      break;
    default:
      gcc_unreachable ();
    }

  rtx_insn *iter;
  for (iter = NEXT_INSN (insn); iter; iter = NEXT_INSN (iter))
    {
      if (!iter)
	break;
      rtx note = find_reg_equal_equiv_note (iter);
      if (note && rtx_equal_p (XEXP (note, 0), expr))
	return iter;
    }
  gcc_unreachable ();
}

/* Calculate the value of x register in the sequence.  */
static poly_int64
calculate_x_in_sequence (rtx x)
{
  rtx_insn *insn;
  std::map<int, poly_int64> worklist;

  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    {
      if (!insn)
	break;

      rtx pat = PATTERN (insn);

      if (GET_CODE (pat) == CLOBBER)
	continue;

      rtx dest = SET_DEST (pat);
      rtx src = SET_SRC (pat);
      poly_int64 new_value;
      rtx op1, op2;
      poly_int64 op1_val, op2_val;

      if (UNARY_P (src))
	{
	  op1 = XEXP (src, 0);
	  op1_val = get_value (worklist, op1);
	}
      if (BINARY_P (src))
	{
	  op1 = XEXP (src, 0);
	  op2 = XEXP (src, 1);
	  op1_val = get_value (worklist, op1);
	  op2_val = get_value (worklist, op2);
	}

      switch (GET_CODE (src))
	{
	case CONST_POLY_INT:
	  new_value = rtx_to_poly_int64 (src);
	  break;
	case CONST_INT:
	  if (SUBREG_P (dest))
	    {
	      /* For DImode in RV32 system, the sequence should be:
	      (insn 1 0 2 (set (subreg:SI (reg:DI 135) 4)
		(const_int 0 [0])) -1
	       (nil))
	      (insn 2 1 3 (set (subreg:SI (reg:DI 135) 0)
		(const_poly_int:SI [8, 8])) -1
	       (nil))  */
	      gcc_assert (rtx_equal_p (src, const0_rtx));
	      continue;
	    }
	  else
	    new_value = INTVAL (src);
	  break;
	case REG:
	  new_value = get_value (worklist, src);
	  break;
	case NEG:
	  new_value = -op1_val;
	  break;
	case ASHIFT:
	  if (SUBREG_P (op1))
	    {
	      insn = NEXT_INSN (insn);
	      new_value = exact_div (op1_val, 1 << (GET_MODE_BITSIZE (Pmode)
						    - INTVAL (op2)));
	      dest = SET_DEST (PATTERN (insn));
	      /* The consecutive 4 instructions in the sequence belongs to
	       * ashift:DI in RV32 system.  */
	      insn = NEXT_INSN (NEXT_INSN (insn));
	    }
	  else
	    new_value = op1_val << INTVAL (op2);
	  break;
	case LSHIFTRT:
	  new_value = exact_div (op1_val, 1 << INTVAL (op2));
	  break;
	case MULT:
	  if (op1_val.is_constant ())
	    new_value = op1_val.to_constant () * op2_val;
	  else if (op2_val.is_constant ())
	    new_value = op1_val * op2_val.to_constant ();
	  else
	    gcc_unreachable ();
	  /* For (mult:DI) in RV32 system, we search REG_EQUAL note.  */
	  insn = get_reg_equal_insn (MULT, insn, op1, op2);
	  dest = SET_DEST (PATTERN (insn));
	  break;
	case PLUS:
	  new_value = op1_val + op2_val;
	  /* For (plus:DI) in RV32 system, we search REG_EQUAL note.  */
	  insn = get_reg_equal_insn (PLUS, insn, op1, op2);
	  dest = SET_DEST (PATTERN (insn));
	  break;
	case MINUS:
	  new_value = op1_val - op2_val;
	  insn = get_reg_equal_insn (MINUS, insn, op1, op2);
	  dest = SET_DEST (PATTERN (insn));
	  break;
	default:
	  gcc_unreachable ();
	}

      if (worklist.find (regno (dest)) != worklist.end ())
	worklist[regno (dest)] = new_value;
      else
	worklist.insert (std::make_pair (regno (dest), new_value));
    }

  if (worklist.find (REGNO (x)) != worklist.end ())
    return worklist.find (REGNO (x))->second;

  return 0;
}

void
run_poly_int_selftests (void)
{
  auto_vec<poly_int64> worklist;
  /* Case 1: (set ((dest) (const_poly_int:P [BYTES_PER_RISCV_VECTOR]))).  */
  worklist.safe_push (BYTES_PER_RISCV_VECTOR);
  /* Case 2: (set ((dest) (const_poly_int:P [BYTES_PER_RISCV_VECTOR * 8]))).  */
  worklist.safe_push (BYTES_PER_RISCV_VECTOR * 8);
  /* Case 3: (set ((dest) (const_poly_int:P [BYTES_PER_RISCV_VECTOR * 32]))). */
  worklist.safe_push (BYTES_PER_RISCV_VECTOR * 32);
  /* Case 4: (set ((dest) (const_poly_int:P [207, 0]))).  */
  worklist.safe_push (poly_int64 (207, 0));
  /* Case 5: (set ((dest) (const_poly_int:P [-207, 0]))).  */
  worklist.safe_push (poly_int64 (-207, 0));
  /* Case 6: (set ((dest) (const_poly_int:P [0, 207]))).  */
  worklist.safe_push (poly_int64 (0, 207));
  /* Case 7: (set ((dest) (const_poly_int:P [0, -207]))).  */
  worklist.safe_push (poly_int64 (0, -207));
  /* Case 8: (set ((dest) (const_poly_int:P [5555, 0]))).  */
  worklist.safe_push (poly_int64 (5555, 0));
  /* Case 9: (set ((dest) (const_poly_int:P [0, 5555]))).  */
  worklist.safe_push (poly_int64 (0, 5555));
  /* Case 10: (set ((dest) (const_poly_int:P [4096, 4096]))).  */
  worklist.safe_push (poly_int64 (4096, 4096));
  /* Case 11: (set ((dest) (const_poly_int:P [17, 4088]))).  */
  worklist.safe_push (poly_int64 (17, 4088));
  /* Case 12: (set ((dest) (const_poly_int:P [3889, 4104]))).  */
  worklist.safe_push (poly_int64 (3889, 4104));
  /* Case 13: (set ((dest) (const_poly_int:P [-4096, -4096]))).  */
  worklist.safe_push (poly_int64 (-4096, -4096));
  /* Case 14: (set ((dest) (const_poly_int:P [219, -4088]))).  */
  worklist.safe_push (poly_int64 (219, -4088));
  /* Case 15: (set ((dest) (const_poly_int:P [-4309, -4104]))).  */
  worklist.safe_push (poly_int64 (-4309, -4104));
  /* Case 16: (set ((dest) (const_poly_int:P [-7337, 88]))).  */
  worklist.safe_push (poly_int64 (-7337, 88));
  /*  Case 17: (set ((dest) (const_poly_int:P [9317, -88]))).  */
  worklist.safe_push (poly_int64 (9317, -88));
  /* Case 18: (set ((dest) (const_poly_int:P [4, 4]))).  */
  worklist.safe_push (poly_int64 (4, 4));
  /* Case 19: (set ((dest) (const_poly_int:P [17, 4]))).  */
  worklist.safe_push (poly_int64 (17, 4));
  /* Case 20: (set ((dest) (const_poly_int:P [-7337, 4]))).  */
  worklist.safe_push (poly_int64 (-7337, 4));
  /* Case 21: (set ((dest) (const_poly_int:P [-4, -4]))).  */
  worklist.safe_push (poly_int64 (-4, -4));
  /* Case 22: (set ((dest) (const_poly_int:P [-389, -4]))).  */
  worklist.safe_push (poly_int64 (-389, -4));
  /* Case 23: (set ((dest) (const_poly_int:P [4789, -4]))).  */
  worklist.safe_push (poly_int64 (4789, -4));
  /* Case 24: (set ((dest) (const_poly_int:P [-5977, 1508]))).  */
  worklist.safe_push (poly_int64 (-5977, 1508));
  /* Case 25: (set ((dest) (const_poly_int:P [219, -1508]))).  */
  worklist.safe_push (poly_int64 (219, -1508));
  /* Case 26: (set ((dest) (const_poly_int:P [2, 2]))).  */
  worklist.safe_push (poly_int64 (2, 2));
  /* Case 27: (set ((dest) (const_poly_int:P [33, 2]))).  */
  worklist.safe_push (poly_int64 (33, 2));
  /* Case 28: (set ((dest) (const_poly_int:P [-7337, 2]))).  */
  worklist.safe_push (poly_int64 (-7337, 2));
  /* Case 29: (set ((dest) (const_poly_int:P [-2, -2]))).  */
  worklist.safe_push (poly_int64 (-2, -2));
  /* Case 30: (set ((dest) (const_poly_int:P [-389, -2]))).  */
  worklist.safe_push (poly_int64 (-389, -2));
  /* Case 31: (set ((dest) (const_poly_int:P [4789, -2]))).  */
  worklist.safe_push (poly_int64 (4789, -2));
  /* Case 32: (set ((dest) (const_poly_int:P [-3567, 954]))).  */
  worklist.safe_push (poly_int64 (-3567, 954));
  /* Case 33: (set ((dest) (const_poly_int:P [945, -954]))).  */
  worklist.safe_push (poly_int64 (945, -954));
  /* Case 34: (set ((dest) (const_poly_int:P [1, 1]))).  */
  worklist.safe_push (poly_int64 (1, 1));
  /* Case 35: (set ((dest) (const_poly_int:P [977, 1]))).  */
  worklist.safe_push (poly_int64 (977, 1));
  /* Case 36: (set ((dest) (const_poly_int:P [-339, 1]))).  */
  worklist.safe_push (poly_int64 (-339, 1));
  /* Case 37: (set ((dest) (const_poly_int:P [-1, -1]))).  */
  worklist.safe_push (poly_int64 (-1, -1));
  /* Case 38: (set ((dest) (const_poly_int:P [-12, -1]))).  */
  worklist.safe_push (poly_int64 (-12, -1));
  /* Case 39: (set ((dest) (const_poly_int:P [44, -1]))).  */
  worklist.safe_push (poly_int64 (44, -1));
  /* Case 40: (set ((dest) (const_poly_int:P [-9567, 77]))).  */
  worklist.safe_push (poly_int64 (9567, 77));
  /* Case 41: (set ((dest) (const_poly_int:P [3467, -77]))).  */
  worklist.safe_push (poly_int64 (3467, -77));

  {
    /* Test poly manipulation in RV64 system with TARGET_MIN_VLEN > 32.  */
    riscv_selftest_arch_abi_setter rv ("rv64imafdv", ABI_LP64D);
    rtl_dump_test t (SELFTEST_LOCATION, locate_file ("riscv/empty-func.rtl"));
    set_new_first_and_last_insn (NULL, NULL);

    /* Test compilation.  */
    emit_move_insn (gen_reg_rtx (QImode),
		    gen_int_mode (BYTES_PER_RISCV_VECTOR, QImode));
    emit_move_insn (gen_reg_rtx (HImode),
		    gen_int_mode (BYTES_PER_RISCV_VECTOR, HImode));
    emit_move_insn (gen_reg_rtx (SImode),
		    gen_int_mode (BYTES_PER_RISCV_VECTOR, SImode));
    emit_move_insn (gen_reg_rtx (DImode),
		    gen_int_mode (BYTES_PER_RISCV_VECTOR, DImode));

    /* Test results for (const_poly_int:P).  */
    for (unsigned int i = 0; i < worklist.length (); ++i)
      {
	start_sequence ();
	rtx dest = gen_reg_rtx (Pmode);
	emit_move_insn (dest, gen_int_mode (worklist[i], Pmode));
	ASSERT_TRUE (known_eq (calculate_x_in_sequence (dest), worklist[i]));
	end_sequence ();
      }
  }
  {
    riscv_selftest_arch_abi_setter rv ("rv64imafd_zve32x1p0", ABI_LP64D);
    rtl_dump_test t (SELFTEST_LOCATION, locate_file ("riscv/empty-func.rtl"));
    set_new_first_and_last_insn (NULL, NULL);
    /* Test results for (const_poly_int:DI) in RV32 system.  */
    for (unsigned int i = 0; i < worklist.length (); ++i)
      {
	start_sequence ();
	rtx dest = gen_reg_rtx (Pmode);
	emit_move_insn (dest, gen_int_mode (worklist[i], Pmode));
	ASSERT_TRUE (known_eq (calculate_x_in_sequence (dest), worklist[i]));
	end_sequence ();
      }
  }
  {
    riscv_selftest_arch_abi_setter rv ("rv32imafd_zve32x1p0", ABI_ILP32);
    rtl_dump_test t (SELFTEST_LOCATION, locate_file ("riscv/empty-func.rtl"));
    set_new_first_and_last_insn (NULL, NULL);
    /* Test results for (const_poly_int:DI) in RV32 system.  */
    for (unsigned int i = 0; i < worklist.length (); ++i)
      {
	start_sequence ();
	rtx dest = gen_reg_rtx (Pmode);
	emit_move_insn (dest, gen_int_mode (worklist[i], Pmode));
	ASSERT_TRUE (known_eq (calculate_x_in_sequence (dest), worklist[i]));
	end_sequence ();
      }
  }
  {
    riscv_selftest_arch_abi_setter rv ("rv32imafdv", ABI_ILP32);
    rtl_dump_test t (SELFTEST_LOCATION, locate_file ("riscv/empty-func.rtl"));
    set_new_first_and_last_insn (NULL, NULL);
    /* Test results for (const_poly_int:DI) in RV32 system.  */
    for (unsigned int i = 0; i < worklist.length (); ++i)
      {
	start_sequence ();
	rtx dest = gen_reg_rtx (DImode);
	emit_move_insn (dest, gen_int_mode (worklist[i], DImode));
	ASSERT_TRUE (known_eq (calculate_x_in_sequence (dest), worklist[i]));
	end_sequence ();
      }
  }
}