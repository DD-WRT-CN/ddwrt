#objdump: -srt

.*:     file format elf64-mmix

SYMBOL TABLE:
0000000000000000 l    d  \.text	0000000000000000 
0000000000000000 l    d  \.data	0000000000000000 
0000000000000000 l    d  \.bss	0000000000000000 
0000000000000000 l       \.MMIX\.reg_contents	0000000000000000 im
0000000000000000 l    d  \.MMIX\.reg_contents	0000000000000000 


RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0000000000000000 R_MMIX_64         \.text


Contents of section \.text:
Contents of section \.data:
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000000                    .*
