#objdump: -d -mmips:8000
#as: -32 -march=8000 -EB -mgp32 -mfp64 -KPIC
#name: MIPS -mgp32 -mfp64 (SVR4 PIC)

.*: +file format.*

Disassembly of section .text:

0+000 <[^>]*>:
   0:	3c1c0000 	lui	gp,0x0
   4:	279c0000 	addiu	gp,gp,0
   8:	0399e021 	addu	gp,gp,t9
   c:	afbc0008 	sw	gp,8\(sp\)
  10:	009c2021 	addu	a0,a0,gp
  14:	3c041234 	lui	a0,0x1234
  18:	34845678 	ori	a0,a0,0x5678
  1c:	8f840000 	lw	a0,0\(gp\)
  20:	00000000 	nop
  24:	24840000 	addiu	a0,a0,0
  28:	8f840000 	lw	a0,0\(gp\)
  2c:	00000000 	nop
  30:	24840000 	addiu	a0,a0,0
  34:	8f840000 	lw	a0,0\(gp\)
  38:	00000000 	nop
  3c:	248401a4 	addiu	a0,a0,420
  40:	10000058 	b	1a4 <[^>]*>
  44:	00000000 	nop
  48:	8f990000 	lw	t9,0\(gp\)
  4c:	00000000 	nop
  50:	273901a4 	addiu	t9,t9,420
  54:	0320f809 	jalr	t9
  58:	00000000 	nop
  5c:	8fbc0008 	lw	gp,8\(sp\)
  60:	8f840000 	lw	a0,0\(gp\)
  64:	00000000 	nop
  68:	24840000 	addiu	a0,a0,0
  6c:	8c840000 	lw	a0,0\(a0\)
  70:	8f840000 	lw	a0,0\(gp\)
  74:	00000000 	nop
  78:	24840000 	addiu	a0,a0,0
  7c:	8c840000 	lw	a0,0\(a0\)
  80:	8f840000 	lw	a0,0\(gp\)
  84:	00000000 	nop
  88:	248401a4 	addiu	a0,a0,420
  8c:	8c840000 	lw	a0,0\(a0\)
  90:	8f810000 	lw	at,0\(gp\)
  94:	00000000 	nop
  98:	8c240000 	lw	a0,0\(at\)
  9c:	8c250004 	lw	a1,4\(at\)
  a0:	8f810000 	lw	at,0\(gp\)
  a4:	00000000 	nop
  a8:	8c240000 	lw	a0,0\(at\)
  ac:	8c250004 	lw	a1,4\(at\)
  b0:	8f810000 	lw	at,0\(gp\)
  b4:	00000000 	nop
  b8:	8c2401a4 	lw	a0,420\(at\)
  bc:	8c2501a8 	lw	a1,424\(at\)
  c0:	8f810000 	lw	at,0\(gp\)
  c4:	00000000 	nop
  c8:	24210000 	addiu	at,at,0
  cc:	ac240000 	sw	a0,0\(at\)
  d0:	8f810000 	lw	at,0\(gp\)
  d4:	00000000 	nop
  d8:	24210000 	addiu	at,at,0
  dc:	ac240000 	sw	a0,0\(at\)
  e0:	8f810000 	lw	at,0\(gp\)
  e4:	00000000 	nop
  e8:	ac240000 	sw	a0,0\(at\)
  ec:	ac250004 	sw	a1,4\(at\)
  f0:	8f810000 	lw	at,0\(gp\)
  f4:	00000000 	nop
  f8:	ac240000 	sw	a0,0\(at\)
  fc:	ac250004 	sw	a1,4\(at\)
 100:	8f810000 	lw	at,0\(gp\)
 104:	00000000 	nop
 108:	24210000 	addiu	at,at,0
 10c:	80240000 	lb	a0,0\(at\)
 110:	90210001 	lbu	at,1\(at\)
 114:	00042200 	sll	a0,a0,0x8
 118:	00812025 	or	a0,a0,at
 11c:	8f810000 	lw	at,0\(gp\)
 120:	00000000 	nop
 124:	24210000 	addiu	at,at,0
 128:	a0240001 	sb	a0,1\(at\)
 12c:	00042202 	srl	a0,a0,0x8
 130:	a0240000 	sb	a0,0\(at\)
 134:	90210001 	lbu	at,1\(at\)
 138:	00042200 	sll	a0,a0,0x8
 13c:	00812025 	or	a0,a0,at
 140:	8f810000 	lw	at,0\(gp\)
 144:	00000000 	nop
 148:	24210000 	addiu	at,at,0
 14c:	88240000 	lwl	a0,0\(at\)
 150:	98240003 	lwr	a0,3\(at\)
 154:	8f810000 	lw	at,0\(gp\)
 158:	00000000 	nop
 15c:	24210000 	addiu	at,at,0
 160:	a8240000 	swl	a0,0\(at\)
 164:	b8240003 	swr	a0,3\(at\)
 168:	3c043ff0 	lui	a0,0x3ff0
 16c:	00002821 	move	a1,zero
 170:	8f810000 	lw	at,0\(gp\)
 174:	8c240000 	lw	a0,0\(at\)
 178:	8c250004 	lw	a1,4\(at\)
 17c:	8f810000 	lw	at,0\(gp\)
 180:	d4200008 	ldc1	\$f0,8\(at\)
 184:	8f810000 	lw	at,0\(gp\)
 188:	d4200010 	ldc1	\$f0,16\(at\)
 18c:	24a40064 	addiu	a0,a1,100
 190:	2c840001 	sltiu	a0,a0,1
 194:	24a40064 	addiu	a0,a1,100
 198:	0004202b 	sltu	a0,zero,a0
 19c:	00a02021 	move	a0,a1
 1a0:	46231040 	add.d	\$f1,\$f2,\$f3

0+01a4 <[^>]*>:
	...
