; RUN: llc -mtriple=powerpc-unknown-unknown-eabi -ppc-ssection-threshold=8 < %s | FileCheck %s -check-prefix=SMALL8
; RUN: llc -mtriple=powerpc-unknown-unknown-eabi -ppc-ssection-threshold=0 < %s | FileCheck %s -check-prefix=SMALL0
; RUN: llc -mtriple=powerpc-unknown-unknown-eabi -ppc-ssection-threshold=8 < %s | FileCheck %s -check-prefix=SMALLRET8
; RUN: llc -mtriple=powerpc-unknown-unknown-eabi -ppc-ssection-threshold=0 < %s | FileCheck %s -check-prefix=SMALLRET0

@s1 = internal unnamed_addr global i32 8, align 4
@s2 = internal unnamed_addr global i32 4, align 4
@g1 = external global i32
@c1 = external constant i32

define float @foo() nounwind {
entry:
  %0 = load i32, i32* @s1, align 4
; SMALL8: lwz 3, s1@sda21(13)
; SMALL0: lwz 3, s1@l(30)
  tail call void @foo1(i32 %0) nounwind
; SMALL8: bl foo1
; SMALL0: bl foo1
  %1 = load i32, i32* @g1, align 4
  %2 = load i32, i32* @c1, align 4
; SMALL8: lwz 3, g1@sda21(13)
; SMALL8: lwz 4, c1@sda21(2)
; SMALL0: lis 3, g1@ha
; SMALL0: lis 4, c1@ha
; SMALL0: lwz 5, g1@l(3)
; SMALL0: lwz 4, c1@l(4)
  %add = add nsw i32 %1, 2
; SMALL8: stw 3, s1@sda21(13)
; SMALL8: addi 3, 3, 2
; SMALL0: stw 5, s1@l(30)
; SMALL0: addi 5, 5, 2
  store i32 %1, i32* @s1, align 4
  store i32 %2, i32* @s2, align 4
  store i32 %add, i32* @g1, align 4
; SMALL8: stw 4, s2@sda21(13)
; SMALL8: stw 3, g1@sda21(13)
; SMALL0: stw 4, s2@l(7)
; SMALL0: stw 5, g1@l(3)
  ret float 1.0
; SMALLRET8: lfs 1, .LCPI0_0@sda21(2)
; SMALLRET0: lis 6, .LCPI0_0@ha
; SMALLRET0: lfs 1, .LCPI0_0@l(6)
}

declare void @foo1(i32)
