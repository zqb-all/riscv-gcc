;; Machine description for RISC-V 'V' Extension for GNU compiler.
;; Copyright (C) 2022-2022 Free Software Foundation, Inc.
;; Contributed by Juzhe Zhong (juzhe.zhong@rivai.ai), RiVAI Technologies Ltd.

;; This file is part of GCC.

;; GCC is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.

;; GCC is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING3.  If not see
;; <http://www.gnu.org/licenses/>.

;; This file describes the RISC-V 'V' Extension, Version 1.0.
;;
;; This file include :
;;
;; - Intrinsics (https://github.com/riscv/rvv-intrinsic-doc)
;; - Auto-vectorization (TBD)
;; - Combine optimization (TBD)

(define_c_enum "unspec" [
  UNSPEC_VSETVL
])

;; -----------------------------------------------------------------
;; ---- 6. Configuration-Setting Instructions
;; -----------------------------------------------------------------
;; Includes:
;; - 6.1 vsetvli/vsetivl/vsetvl instructions
;; -----------------------------------------------------------------

;; we dont't define vsetvli as unspec_volatile which has side effects.
;; This instruction can be scheduled by the instruction scheduler.
;; This means these instructions will be deleted when
;; there is no instructions using vl or vtype in the following.
;; rd  | rs1 | AVL value | Effect on vl
;; -   | !x0 | x[rs1]    | Normal stripmining
;; !x0 | x0  | ~0        | Set vl to VLMAX
;; operands[0] is VL.
;; operands[1] is AVL.
;; operands[2] is machine_mode concatenate with Tail && Mask policy.
;; The bitmap value of operands[2] is as follows:
;; hi<----------------------------lo
;; |------------| 1 bit  | 1 bit  |
;; |machine_mode|  tail  |  mask  |
;; machine_mode specifies SEW and LMUL:
;;  - VNx2SI describes SEW = 32, LMUL = M1 when TARGET_MIN_VLEN > 32.
;;  - VNx2SI describes SEW = 32, LMUL = M2 when TARGET_MIN_VLEN = 32.
;; Tail && Mask policy:
;;  - 0x00: tu, mu.
;;  - 0x01: tu, ma.
;;  - 0x10: ta, mu.
;;  - 0x11: ta, ma.
(define_insn "@vsetvl<mode>"
  [(set (match_operand:P 0 "register_operand" "=r,r")
	(unspec:P [(match_operand:P 1 "csr_operand" "r,K")
		   (match_operand 2 "const_int_operand" "i,i")] UNSPEC_VSETVL))
   (set (reg:SI VL_REGNUM)
	(unspec:SI [(match_dup 1)
		    (match_dup 2)] UNSPEC_VSETVL))
   (set (reg:SI VTYPE_REGNUM)
	(unspec:SI [(match_dup 2)] UNSPEC_VSETVL))]
  "TARGET_VECTOR"
  "@
   vsetvli\t%0,%1,%v2
   vsetivli\t%0,%1,%v2"
  [(set_attr "type" "vsetvl")
   (set_attr "mode" "<MODE>")])