#source: loc1.s
#ld: -e loc1 -m elf64mmix
#objdump: -str

# Single text file.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+1000 l    d  \.text	0+ 
2000000000000000 l    d  \.data	0+ 
2000000000000000 l    d  \.sbss	0+ 
2000000000000000 l    d  \.bss	0+ 
0+ l    d  \*ABS\*	0+ 
0+ l    d  \*ABS\*	0+ 
0+ l    d  \*ABS\*	0+ 
0+1000 g       \.text	0+ loc1
0+1000 g       \*ABS\*	0+ __\.MMIX\.start\.\.text
2000000000000000 g       \*ABS\*	0+ __bss_start
2000000000000000 g       \*ABS\*	0+ _edata
2000000000000000 g       \*ABS\*	0+ _end
0+1000 g       \.text	0+ _start\.

Contents of section \.text:
 1000 fd030303                             .*
Contents of section \.data:
Contents of section \.sbss:
