#source: start3.s
#source: bpo-6.s
#source: bpo-5.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix --gc-sections
#objdump: -st

# Check that GC does not mess up things when no BPO:s are collected.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.init	0+ 
0+10 l    d  \.text	0+ 
20+ l    d  \.sbss	0+ 
2000000000000000 l    d  \.bss	0+ 
0+7e8 l    d  \.MMIX\.reg_contents	0+ 
0+ l    d  \*ABS\*	0+ 
0+ l    d  \*ABS\*	0+ 
0+ l    d  \*ABS\*	0+ 
0+ l       \.init	0+ _start
0+14 g       \.text	0+ x
0+10 g       \.text	0+ x2
#...

Contents of section \.init:
 0000 00000000 0000003d 00000000 0000003a  .*
Contents of section \.text:
 0010 232dfe00 232dfd00                    .*
Contents of section \.sbss:
Contents of section \.MMIX\.reg_contents:
 07e8 00000000 0000107c 00000000 0000a420  .*
