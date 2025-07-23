/* Roscoe startup code
 */

#include "../Hardware/Roscoe.h"
#include "../Shared/16550.h"

	.title "BootLoaderStartup.s"

	.extern SYM (_start)
	.extern SYM (InterruptServiceRoutine)
	.extern	SYM (_end)
	.extern	SYM (_stack)
	.extern SYM (InterruptFaultGroupA)
	.extern SYM (InterruptFaultGroupB)
	.global	BootLoaderEntry
	.global Trap7

/* Set a POST code macro */
	.macro	POSTSet, POSTCode
	moveb	#\POSTCode >> 8, %d0
	moveb	%d0, (%a4)
	addl	#0x100, %a4
	moveb	#\POSTCode & 0xff, %d0
	moveb	%d0, (%a4)
	subl	#0x100, %a4

	.endm

/* Stackless call */
	.macro	StacklessCall, CallTarget
	moveal	#.+10+ROSCOE_FLASH_BOOT_BASE, %sp
	braw	\CallTarget
	.long	.+ROSCOE_FLASH_BOOT_BASE+4
/* Call returns here */
	.endm

/* Halt macro */
	.macro	HALT
	stop	#0x2700			/* Shut off all interrupts and halt */	
	.endm

/* Roscoe address ranges and peripherals */

	.text

/* 68030 Vector table */
	
_vectorTable:
	.long	_stack			/* 0   - Initial stack pointer */
	.long	BootLoaderEntry - ROSCOE_BOOT_BASE_SRAM		/* 1   - PC startup vector */
	.long   Vec2  	        	/* 2   - Bus error */
	.long   Vec3  	        	/* 3   - Address error */
	.long   Vec4  	        	/* 4   - Illegal instruction */
	.long   Vec5  	        	/* 5   - Division by 0 */
	.long   Vec6  	        	/* 6   - CHK Instruction */
	.long   Vec7  	        	/* 7   - TRAPV Instruction */
	.long   Vec8  	        	/* 8   - Privilege violation */
	.long   Vec9  	        	/* 9   - Trace */
	.long   Vec10 	        	/* 10  - Unimplemented instruction */
	.long   Vec11 	        	/* 11  - Unimplemented instruction */
	.long   Vec12 	        	/* 12  - Reserved */
	.long   Vec13 	        	/* 13  - Reserved */
	.long   Vec14 	        	/* 14  - Reserved */
	.long   Vec15 	        	/* 15  - Uninitialized interrupt vector */
	.long   Vec16 	        	/* 16  - Reserved */
	.long   Vec17 	        	/* 17  - Reserved */
	.long   Vec18 	        	/* 18  - Reserved */
	.long   Vec19 	        	/* 19  - Reserved */
	.long   Vec20 	        	/* 20  - Reserved */
	.long   Vec21 	        	/* 21  - Reserved */
	.long   Vec22 	        	/* 22  - Reserved */
	.long   Vec23 	        	/* 23  - Reserved */
	.long   Vec24 	        	/* 24  - Spurious interrupt */
	.long   Vec25			/* 25  - Level 1 interrupt autovector */
	.long   Vec26			/* 26  - Level 2 interrupt autovector */
	.long   Vec27			/* 27  - Level 3 interrupt autovector */
	.long   Vec28			/* 28  - Level 4 interrupt autovector */
	.long   Vec29			/* 29  - Level 5 interrupt autovector */
	.long   Vec30			/* 30  - Level 6 interrupt autovector */
	.long   Vec31			/* 31  - Level 7 interrupt autovector */
	.long   Vec32 	        	/* 32  - Trap #0 instruction */
	.long   Vec33 	        	/* 33  - Trap #1 instruction */
	.long   Vec34 	        	/* 34  - Trap #2 instruction */
	.long   Vec35 	        	/* 35  - Trap #3 instruction */
	.long   Vec36 	        	/* 36  - Trap #4 instruction */
	.long   Vec37 	        	/* 37  - Trap #5 instruction */
	.long   Vec38 	        	/* 38  - Trap #6 instruction */
	.long   Vec39 	        	/* 39  - Trap #7 instruction */
	.long   Vec40 	        	/* 40  - Trap #8 instruction */
	.long   Vec41 	        	/* 41  - Trap #9 instruction */
	.long   Vec42 	        	/* 42  - Trap #10 instruction */
	.long   Vec43 	        	/* 43  - Trap #11 instruction */
	.long   Vec44 	        	/* 44  - Trap #12 instruction */
	.long   Vec45 	        	/* 45  - Trap #13 instruction */
	.long   Vec46 	        	/* 46  - Trap #14 instruction */
	.long   Vec47 	        	/* 47  - Trap #15 instruction */
	.long   Vec48 	        	/* 48  - Reserved */
	.long   Vec49 	        	/* 49  - Reserved */
	.long   Vec50 	        	/* 50  - Reserved */
	.long   Vec51 	        	/* 51  - Reserved */
	.long   Vec52 	        	/* 52  - Reserved */
	.long   Vec53 	        	/* 53  - Reserved */
	.long   Vec54 	        	/* 54  - Reserved */
	.long   Vec55 	        	/* 55  - Reserved */
	.long   Vec56 	        	/* 56  - Reserved */
	.long   Vec57 	        	/* 57  - Reserved */
	.long   Vec58 	        	/* 58  - Reserved */
	.long   Vec59 	        	/* 59  - Reserved */
	.long   Vec60 	        	/* 60  - Reserved */
	.long   Vec61 	        	/* 61  - Reserved */
	.long   Vec62 	        	/* 62  - Reserved */
	.long   Vec63 	        	/* 63  - Reserved */
	.long   Vec64 	        	/* 64  - User vector #0 */
	.long   Vec65 	        	/* 65  - User vector #1 */
	.long   Vec66 	        	/* 66  - User vector #2 */
	.long   Vec67 	        	/* 67  - User vector #3 */
	.long   Vec68 	        	/* 68  - User vector #4 */
	.long   Vec69 	        	/* 69  - User vector #5 */
	.long   Vec70 	        	/* 70  - User vector #6 */
	.long   Vec71 	        	/* 71  - User vector #7 */
	.long   Vec72 	        	/* 72  - User vector #8 */
	.long   Vec73 	        	/* 73  - User vector #9 */
	.long   Vec74 	        	/* 74  - User vector #10 */
	.long   Vec75 	        	/* 75  - User vector #11 */
	.long   Vec76 	        	/* 76  - User vector #12 */
	.long   Vec77 	        	/* 77  - User vector #13 */
	.long   Vec78 	        	/* 78  - User vector #14 */
	.long   Vec79 	        	/* 79  - User vector #15 */
	.long   Vec80 	        	/* 80  - User vector #16 */
	.long   Vec81 	        	/* 81  - User vector #17 */
	.long   Vec82 	        	/* 82  - User vector #18 */
	.long   Vec83 	        	/* 83  - User vector #19 */
	.long   Vec84 	        	/* 84  - User vector #20 */
	.long   Vec85 	        	/* 85  - User vector #21 */
	.long   Vec86 	        	/* 86  - User vector #22 */
	.long   Vec87 	        	/* 87  - User vector #23 */
	.long   Vec88 	        	/* 88  - User vector #24 */
	.long   Vec89 	        	/* 89  - User vector #25 */
	.long   Vec90 	        	/* 90  - User vector #26 */
	.long   Vec91 	        	/* 91  - User vector #27 */
	.long   Vec92 	        	/* 92  - User vector #28 */
	.long   Vec93 	        	/* 93  - User vector #29 */
	.long   Vec94 	        	/* 94  - User vector #30 */
	.long   Vec95 	        	/* 95  - User vector #31 */
	.long   Vec96 	        	/* 96  - User vector #32 */
	.long   Vec97 	        	/* 97  - User vector #33 */
	.long   Vec98 	        	/* 98  - User vector #34 */
	.long   Vec99 	        	/* 99  - User vector #35 */
	.long   Vec100	        	/* 100 - User vector #36 */
	.long   Vec101	        	/* 101 - User vector #37 */
	.long   Vec102	        	/* 102 - User vector #38 */
	.long   Vec103	        	/* 103 - User vector #39 */
	.long   Vec104	        	/* 104 - User vector #40 */
	.long   Vec105	        	/* 105 - User vector #41 */
	.long   Vec106	        	/* 106 - User vector #42 */
	.long   Vec107	        	/* 107 - User vector #43 */
	.long   Vec108	        	/* 108 - User vector #44 */
	.long   Vec109	        	/* 109 - User vector #45 */
	.long   Vec110	        	/* 110 - User vector #46 */
	.long   Vec111	        	/* 111 - User vector #47 */
	.long   Vec112	        	/* 112 - User vector #48 */
	.long   Vec113	        	/* 113 - User vector #49 */
	.long   Vec114	        	/* 114 - User vector #50 */
	.long   Vec115	        	/* 115 - User vector #51 */
	.long   Vec116	        	/* 116 - User vector #52 */
	.long   Vec117	        	/* 117 - User vector #53 */
	.long   Vec118	        	/* 118 - User vector #54 */
	.long   Vec119	        	/* 119 - User vector #55 */
	.long   Vec120	        	/* 120 - User vector #56 */
	.long   Vec121	        	/* 121 - User vector #57 */
	.long   Vec122	        	/* 122 - User vector #58 */
	.long   Vec123	        	/* 123 - User vector #59 */
	.long   Vec124	        	/* 124 - User vector #60 */
	.long   Vec125	        	/* 125 - User vector #61 */
	.long   Vec126	        	/* 126 - User vector #62 */
	.long   Vec127	        	/* 127 - User vector #63 */
	.long   Vec128	        	/* 128 - User vector #64 */
	.long   Vec129	        	/* 129 - User vector #65 */
	.long   Vec130	        	/* 130 - User vector #66 */
	.long   Vec131	        	/* 131 - User vector #67 */
	.long   Vec132	        	/* 132 - User vector #68 */
	.long   Vec133	        	/* 133 - User vector #69 */
	.long   Vec134	        	/* 134 - User vector #70 */
	.long   Vec135	        	/* 135 - User vector #71 */
	.long   Vec136	        	/* 136 - User vector #72 */
	.long   Vec137	        	/* 137 - User vector #73 */
	.long   Vec138	        	/* 138 - User vector #74 */
	.long   Vec139	        	/* 139 - User vector #75 */
	.long   Vec140	        	/* 140 - User vector #76 */
	.long   Vec141	        	/* 141 - User vector #77 */
	.long   Vec142	        	/* 142 - User vector #78 */
	.long   Vec143	        	/* 143 - User vector #79 */
	.long   Vec144	        	/* 144 - User vector #80 */
	.long   Vec145	        	/* 145 - User vector #81 */
	.long   Vec146	        	/* 146 - User vector #82 */
	.long   Vec147	        	/* 147 - User vector #83 */
	.long   Vec148	        	/* 148 - User vector #84 */
	.long   Vec149	        	/* 149 - User vector #85 */
	.long   Vec150	        	/* 150 - User vector #86 */
	.long   Vec151	        	/* 151 - User vector #87 */
	.long   Vec152	        	/* 152 - User vector #88 */
	.long   Vec153	        	/* 153 - User vector #89 */
	.long   Vec154	        	/* 154 - User vector #90 */
	.long   Vec155	        	/* 155 - User vector #91 */
	.long   Vec156	        	/* 156 - User vector #92 */
	.long   Vec157	        	/* 157 - User vector #93 */
	.long   Vec158	        	/* 158 - User vector #94 */
	.long   Vec159	        	/* 159 - User vector #95 */
	.long   Vec160	        	/* 160 - User vector #96 */
	.long   Vec161	        	/* 161 - User vector #97 */
	.long   Vec162	        	/* 162 - User vector #98 */
	.long   Vec163	        	/* 163 - User vector #99 */
	.long   Vec164	        	/* 164 - User vector #100 */
	.long   Vec165	        	/* 165 - User vector #101 */
	.long   Vec166	        	/* 166 - User vector #102 */
	.long   Vec167	        	/* 167 - User vector #103 */
	.long   Vec168	        	/* 168 - User vector #104 */
	.long   Vec169	        	/* 169 - User vector #105 */
	.long   Vec170	        	/* 170 - User vector #106 */
	.long   Vec171	        	/* 171 - User vector #107 */
	.long   Vec172	        	/* 172 - User vector #108 */
	.long   Vec173	        	/* 173 - User vector #109 */
	.long   Vec174	        	/* 174 - User vector #110 */
	.long   Vec175	        	/* 175 - User vector #111 */
	.long   Vec176	        	/* 176 - User vector #112 */
	.long   Vec177	        	/* 177 - User vector #113 */
	.long   Vec178	        	/* 178 - User vector #114 */
	.long   Vec179	        	/* 179 - User vector #115 */
	.long   Vec180	        	/* 180 - User vector #116 */
	.long   Vec181	        	/* 181 - User vector #117 */
	.long   Vec182	        	/* 182 - User vector #118 */
	.long   Vec183	        	/* 183 - User vector #119 */
	.long   Vec184	        	/* 184 - User vector #120 */
	.long   Vec185	        	/* 185 - User vector #121 */
	.long   Vec186	        	/* 186 - User vector #122 */
	.long   Vec187	        	/* 187 - User vector #123 */
	.long   Vec188	        	/* 188 - User vector #124 */
	.long   Vec189	        	/* 189 - User vector #125 */
	.long   Vec190	        	/* 190 - User vector #126 */
	.long   Vec191	        	/* 191 - User vector #127 */
	.long   Vec192	        	/* 192 - User vector #128 */
	.long   Vec193	        	/* 193 - User vector #129 */
	.long   Vec194	        	/* 194 - User vector #130 */
	.long   Vec195	        	/* 195 - User vector #131 */
	.long   Vec196	        	/* 196 - User vector #132 */
	.long   Vec197	        	/* 197 - User vector #133 */
	.long   Vec198	        	/* 198 - User vector #134 */
	.long   Vec199	        	/* 199 - User vector #135 */
	.long   Vec200	        	/* 200 - User vector #136 */
	.long   Vec201	        	/* 201 - User vector #137 */
	.long   Vec202	        	/* 202 - User vector #138 */
	.long   Vec203	        	/* 203 - User vector #139 */
	.long   Vec204	        	/* 204 - User vector #140 */
	.long   Vec205	        	/* 205 - User vector #141 */
	.long   Vec206	        	/* 206 - User vector #142 */
	.long   Vec207	        	/* 207 - User vector #143 */
	.long   Vec208	        	/* 208 - User vector #144 */
	.long   Vec209	        	/* 209 - User vector #145 */
	.long   Vec210	        	/* 210 - User vector #146 */
	.long   Vec211	        	/* 211 - User vector #147 */
	.long   Vec212	        	/* 212 - User vector #148 */
	.long   Vec213	        	/* 213 - User vector #149 */
	.long   Vec214	        	/* 214 - User vector #150 */
	.long   Vec215	        	/* 215 - User vector #151 */
	.long   Vec216	        	/* 216 - User vector #152 */
	.long   Vec217	        	/* 217 - User vector #153 */
	.long   Vec218	        	/* 218 - User vector #154 */
	.long   Vec219	        	/* 219 - User vector #155 */
	.long   Vec220	        	/* 220 - User vector #156 */
	.long   Vec221	        	/* 221 - User vector #157 */
	.long   Vec222	        	/* 222 - User vector #158 */
	.long   Vec223	        	/* 223 - User vector #159 */
	.long   Vec224	        	/* 224 - User vector #160 */
	.long   Vec225	        	/* 225 - User vector #161 */
	.long   Vec226	        	/* 226 - User vector #162 */
	.long   Vec227	        	/* 227 - User vector #163 */
	.long   Vec228	        	/* 228 - User vector #164 */
	.long   Vec229	        	/* 229 - User vector #165 */
	.long   Vec230	        	/* 230 - User vector #166 */
	.long   Vec231	        	/* 231 - User vector #167 */
	.long   Vec232	        	/* 232 - User vector #168 */
	.long   Vec233	        	/* 233 - User vector #169 */
	.long   Vec234	        	/* 234 - User vector #170 */
	.long   Vec235	        	/* 235 - User vector #171 */
	.long   Vec236	        	/* 236 - User vector #172 */
	.long   Vec237	        	/* 237 - User vector #173 */
	.long   Vec238	        	/* 238 - User vector #174 */
	.long   Vec239	        	/* 239 - User vector #175 */
	.long   Vec240	        	/* 240 - User vector #176 */
	.long   Vec241	        	/* 241 - User vector #177 */
	.long   Vec242	        	/* 242 - User vector #178 */
	.long   Vec243	        	/* 243 - User vector #179 */
	.long   Vec244	        	/* 244 - User vector #180 */
	.long   Vec245	        	/* 245 - User vector #181 */
	.long   Vec246	        	/* 246 - User vector #182 */
	.long   Vec247	        	/* 247 - User vector #183 */
	.long   Vec248	        	/* 248 - User vector #184 */
	.long   Vec249	        	/* 249 - User vector #185 */
	.long   Vec250	        	/* 250 - User vector #186 */
	.long   Vec251	        	/* 251 - User vector #187 */
	.long   Vec252	        	/* 252 - User vector #188 */
	.long   Vec253	        	/* 253 - User vector #189 */
	.long   Vec254	        	/* 254 - User vector #190 */
	.long   Vec255	        	/* 255 - User vector #191 */
_vectorTableEnd:

/* Fault vectors */

Vec2:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x02, %d1
	bra	FaultHandlerEntryGroupA

Vec3:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x03, %d1
	bra	FaultHandlerEntryGroupA

Vec4:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x04, %d1
	bra	FaultHandlerEntryGroupB

Vec5:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x05, %d1
	bra	FaultHandlerEntryGroupB

Vec6:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x06, %d1
	bra	FaultHandlerEntryGroupB

Vec7:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x07, %d1
	bra	FaultHandlerEntryGroupB

Vec8:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x08, %d1
	bra	FaultHandlerEntryGroupB

Vec9:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x09, %d1
	bra	FaultHandlerEntryGroupB

Vec10:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x0a, %d1
	bra	FaultHandlerEntryGroupB

Vec11:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x0b, %d1
	bra	FaultHandlerEntryGroupB

Vec12:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x0c, %d1
	bra	FaultHandlerEntryGroupB

Vec13:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x0d, %d1
	bra	FaultHandlerEntryGroupB

Vec14:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x0e, %d1
	bra	FaultHandlerEntryGroupB

Vec15:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x0f, %d1
	bra	FaultHandlerEntryGroupB

Vec16:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x10, %d1
	bra	FaultHandlerEntryGroupB

Vec17:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x11, %d1
	bra	FaultHandlerEntryGroupB

Vec18:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x12, %d1
	bra	FaultHandlerEntryGroupB

Vec19:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x13, %d1
	bra	FaultHandlerEntryGroupB

Vec20:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x14, %d1
	bra	FaultHandlerEntryGroupB

Vec21:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x15, %d1
	bra	FaultHandlerEntryGroupB

Vec22:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x16, %d1
	bra	FaultHandlerEntryGroupB

Vec23:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x17, %d1
	bra	FaultHandlerEntryGroupB

Vec24:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x18, %d1
	bra	FaultHandlerEntryGroupB

Vec25:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x19, %d1
	bra	FaultHandlerEntryGroupB

Vec26:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x1a, %d1
	bra	FaultHandlerEntryGroupB

Vec27:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x1b, %d1
	bra	FaultHandlerEntryGroupB

Vec28:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x1c, %d1
	bra	FaultHandlerEntryGroupB

Vec29:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x1d, %d1
	bra	FaultHandlerEntryGroupB

Vec30:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x1e, %d1
	bra	FaultHandlerEntryGroupB

Vec31:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x1f, %d1
	bra	FaultHandlerEntryGroupB

Vec32:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x20, %d1
	bra	FaultHandlerEntryGroupB

Vec33:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x21, %d1
	bra	FaultHandlerEntryGroupB

Vec34:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x22, %d1
	bra	FaultHandlerEntryGroupB

Vec35:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x23, %d1
	bra	FaultHandlerEntryGroupB

Vec36:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x24, %d1
	bra	FaultHandlerEntryGroupB

Vec37:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x25, %d1
	bra	FaultHandlerEntryGroupB

Vec38:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x26, %d1
	bra	FaultHandlerEntryGroupB

Vec39:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x27, %d1
	bra	FaultHandlerEntryGroupB

Vec40:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x28, %d1
	bra	FaultHandlerEntryGroupB

Vec41:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x29, %d1
	bra	FaultHandlerEntryGroupB

Vec42:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x2a, %d1
	bra	FaultHandlerEntryGroupB

Vec43:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x2b, %d1
	bra	FaultHandlerEntryGroupB

Vec44:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x2c, %d1
	bra	FaultHandlerEntryGroupB

Vec45:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x2d, %d1
	bra	FaultHandlerEntryGroupB

Vec46:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x2e, %d1
	bra	FaultHandlerEntryGroupB

Vec47:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x2f, %d1
	bra	FaultHandlerEntryGroupB

Vec48:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x30, %d1
	bra	FaultHandlerEntryGroupB

Vec49:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x31, %d1
	bra	FaultHandlerEntryGroupB

Vec50:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x32, %d1
	bra	FaultHandlerEntryGroupB

Vec51:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x33, %d1
	bra	FaultHandlerEntryGroupB

Vec52:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x34, %d1
	bra	FaultHandlerEntryGroupB

Vec53:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x35, %d1
	bra	FaultHandlerEntryGroupB

Vec54:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x36, %d1
	bra	FaultHandlerEntryGroupB

Vec55:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x37, %d1
	bra	FaultHandlerEntryGroupB

Vec56:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x38, %d1
	bra	FaultHandlerEntryGroupB

Vec57:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x39, %d1
	bra	FaultHandlerEntryGroupB

Vec58:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x3a, %d1
	bra	FaultHandlerEntryGroupB

Vec59:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x3b, %d1
	bra	FaultHandlerEntryGroupB

Vec60:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x3c, %d1
	bra	FaultHandlerEntryGroupB

Vec61:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x3d, %d1
	bra	FaultHandlerEntryGroupB

Vec62:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x3e, %d1
	bra	FaultHandlerEntryGroupB

Vec63:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x3f, %d1
	bra	FaultHandlerEntryGroupB

Vec64:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x40, %d1
	bra	FaultHandlerEntryGroupB

Vec65:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x41, %d1
	bra	FaultHandlerEntryGroupB

Vec66:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x42, %d1
	bra	FaultHandlerEntryGroupB

Vec67:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x43, %d1
	bra	FaultHandlerEntryGroupB

Vec68:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x44, %d1
	bra	FaultHandlerEntryGroupB

Vec69:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x45, %d1
	bra	FaultHandlerEntryGroupB

Vec70:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x46, %d1
	bra	FaultHandlerEntryGroupB

Vec71:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x47, %d1
	bra	FaultHandlerEntryGroupB

Vec72:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x48, %d1
	bra	FaultHandlerEntryGroupB

Vec73:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x49, %d1
	bra	FaultHandlerEntryGroupB

Vec74:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x4a, %d1
	bra	FaultHandlerEntryGroupB

Vec75:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x4b, %d1
	bra	FaultHandlerEntryGroupB

Vec76:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x4c, %d1
	bra	FaultHandlerEntryGroupB

Vec77:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x4d, %d1
	bra	FaultHandlerEntryGroupB

Vec78:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x4e, %d1
	bra	FaultHandlerEntryGroupB

Vec79:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x4f, %d1
	bra	FaultHandlerEntryGroupB

Vec80:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x50, %d1
	bra	FaultHandlerEntryGroupB

Vec81:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x51, %d1
	bra	FaultHandlerEntryGroupB

Vec82:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x52, %d1
	bra	FaultHandlerEntryGroupB

Vec83:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x53, %d1
	bra	FaultHandlerEntryGroupB

Vec84:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x54, %d1
	bra	FaultHandlerEntryGroupB

Vec85:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x55, %d1
	bra	FaultHandlerEntryGroupB

Vec86:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x56, %d1
	bra	FaultHandlerEntryGroupB

Vec87:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x57, %d1
	bra	FaultHandlerEntryGroupB

Vec88:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x58, %d1
	bra	FaultHandlerEntryGroupB

Vec89:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x59, %d1
	bra	FaultHandlerEntryGroupB

Vec90:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x5a, %d1
	bra	FaultHandlerEntryGroupB

Vec91:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x5b, %d1
	bra	FaultHandlerEntryGroupB

Vec92:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x5c, %d1
	bra	FaultHandlerEntryGroupB

Vec93:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x5d, %d1
	bra	FaultHandlerEntryGroupB

Vec94:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x5e, %d1
	bra	FaultHandlerEntryGroupB

Vec95:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x5f, %d1
	bra	FaultHandlerEntryGroupB

Vec96:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x60, %d1
	bra	FaultHandlerEntryGroupB

Vec97:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x61, %d1
	bra	FaultHandlerEntryGroupB

Vec98:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x62, %d1
	bra	FaultHandlerEntryGroupB

Vec99:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x63, %d1
	bra	FaultHandlerEntryGroupB

Vec100:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x64, %d1
	bra	FaultHandlerEntryGroupB

Vec101:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x65, %d1
	bra	FaultHandlerEntryGroupB

Vec102:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x66, %d1
	bra	FaultHandlerEntryGroupB

Vec103:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x67, %d1
	bra	FaultHandlerEntryGroupB

Vec104:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x68, %d1
	bra	FaultHandlerEntryGroupB

Vec105:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x69, %d1
	bra	FaultHandlerEntryGroupB

Vec106:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x6a, %d1
	bra	FaultHandlerEntryGroupB

Vec107:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x6b, %d1
	bra	FaultHandlerEntryGroupB

Vec108:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x6c, %d1
	bra	FaultHandlerEntryGroupB

Vec109:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x6d, %d1
	bra	FaultHandlerEntryGroupB

Vec110:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x6e, %d1
	bra	FaultHandlerEntryGroupB

Vec111:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x6f, %d1
	bra	FaultHandlerEntryGroupB

Vec112:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x70, %d1
	bra	FaultHandlerEntryGroupB

Vec113:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x71, %d1
	bra	FaultHandlerEntryGroupB

Vec114:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x72, %d1
	bra	FaultHandlerEntryGroupB

Vec115:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x73, %d1
	bra	FaultHandlerEntryGroupB

Vec116:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x74, %d1
	bra	FaultHandlerEntryGroupB

Vec117:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x75, %d1
	bra	FaultHandlerEntryGroupB

Vec118:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x76, %d1
	bra	FaultHandlerEntryGroupB

Vec119:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x77, %d1
	bra	FaultHandlerEntryGroupB

Vec120:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x78, %d1
	bra	FaultHandlerEntryGroupB

Vec121:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x79, %d1
	bra	FaultHandlerEntryGroupB

Vec122:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x7a, %d1
	bra	FaultHandlerEntryGroupB

Vec123:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x7b, %d1
	bra	FaultHandlerEntryGroupB

Vec124:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x7c, %d1
	bra	FaultHandlerEntryGroupB

Vec125:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x7d, %d1
	bra	FaultHandlerEntryGroupB

Vec126:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x7e, %d1
	bra	FaultHandlerEntryGroupB

Vec127:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x7f, %d1
	bra	FaultHandlerEntryGroupB

Vec128:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x80, %d1
	bra	FaultHandlerEntryGroupB

Vec129:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x81, %d1
	bra	FaultHandlerEntryGroupB

Vec130:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x82, %d1
	bra	FaultHandlerEntryGroupB

Vec131:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x83, %d1
	bra	FaultHandlerEntryGroupB

Vec132:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x84, %d1
	bra	FaultHandlerEntryGroupB

Vec133:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x85, %d1
	bra	FaultHandlerEntryGroupB

Vec134:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x86, %d1
	bra	FaultHandlerEntryGroupB

Vec135:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x87, %d1
	bra	FaultHandlerEntryGroupB

Vec136:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x88, %d1
	bra	FaultHandlerEntryGroupB

Vec137:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x89, %d1
	bra	FaultHandlerEntryGroupB

Vec138:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x8a, %d1
	bra	FaultHandlerEntryGroupB

Vec139:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x8b, %d1
	bra	FaultHandlerEntryGroupB

Vec140:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x8c, %d1
	bra	FaultHandlerEntryGroupB

Vec141:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x8d, %d1
	bra	FaultHandlerEntryGroupB

Vec142:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x8e, %d1
	bra	FaultHandlerEntryGroupB

Vec143:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x8f, %d1
	bra	FaultHandlerEntryGroupB

Vec144:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x90, %d1
	bra	FaultHandlerEntryGroupB

Vec145:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x91, %d1
	bra	FaultHandlerEntryGroupB

Vec146:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x92, %d1
	bra	FaultHandlerEntryGroupB

Vec147:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x93, %d1
	bra	FaultHandlerEntryGroupB

Vec148:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x94, %d1
	bra	FaultHandlerEntryGroupB

Vec149:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x95, %d1
	bra	FaultHandlerEntryGroupB

Vec150:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x96, %d1
	bra	FaultHandlerEntryGroupB

Vec151:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x97, %d1
	bra	FaultHandlerEntryGroupB

Vec152:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x98, %d1
	bra	FaultHandlerEntryGroupB

Vec153:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x99, %d1
	bra	FaultHandlerEntryGroupB

Vec154:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x9a, %d1
	bra	FaultHandlerEntryGroupB

Vec155:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x9b, %d1
	bra	FaultHandlerEntryGroupB

Vec156:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x9c, %d1
	bra	FaultHandlerEntryGroupB

Vec157:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x9d, %d1
	bra	FaultHandlerEntryGroupB

Vec158:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x9e, %d1
	bra	FaultHandlerEntryGroupB

Vec159:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0x9f, %d1
	bra	FaultHandlerEntryGroupB

Vec160:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa0, %d1
	bra	FaultHandlerEntryGroupB

Vec161:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa1, %d1
	bra	FaultHandlerEntryGroupB

Vec162:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa2, %d1
	bra	FaultHandlerEntryGroupB

Vec163:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa3, %d1
	bra	FaultHandlerEntryGroupB

Vec164:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa4, %d1
	bra	FaultHandlerEntryGroupB

Vec165:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa5, %d1
	bra	FaultHandlerEntryGroupB

Vec166:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa6, %d1
	bra	FaultHandlerEntryGroupB

Vec167:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa7, %d1
	bra	FaultHandlerEntryGroupB

Vec168:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa8, %d1
	bra	FaultHandlerEntryGroupB

Vec169:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xa9, %d1
	bra	FaultHandlerEntryGroupB

Vec170:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xaa, %d1
	bra	FaultHandlerEntryGroupB

Vec171:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xab, %d1
	bra	FaultHandlerEntryGroupB

Vec172:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xac, %d1
	bra	FaultHandlerEntryGroupB

Vec173:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xad, %d1
	bra	FaultHandlerEntryGroupB

Vec174:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xae, %d1
	bra	FaultHandlerEntryGroupB

Vec175:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xaf, %d1
	bra	FaultHandlerEntryGroupB

Vec176:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb0, %d1
	bra	FaultHandlerEntryGroupB

Vec177:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb1, %d1
	bra	FaultHandlerEntryGroupB

Vec178:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb2, %d1
	bra	FaultHandlerEntryGroupB

Vec179:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb3, %d1
	bra	FaultHandlerEntryGroupB

Vec180:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb4, %d1
	bra	FaultHandlerEntryGroupB

Vec181:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb5, %d1
	bra	FaultHandlerEntryGroupB

Vec182:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb6, %d1
	bra	FaultHandlerEntryGroupB

Vec183:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb7, %d1
	bra	FaultHandlerEntryGroupB

Vec184:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb8, %d1
	bra	FaultHandlerEntryGroupB

Vec185:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xb9, %d1
	bra	FaultHandlerEntryGroupB

Vec186:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xba, %d1
	bra	FaultHandlerEntryGroupB

Vec187:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xbb, %d1
	bra	FaultHandlerEntryGroupB

Vec188:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xbc, %d1
	bra	FaultHandlerEntryGroupB

Vec189:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xbd, %d1
	bra	FaultHandlerEntryGroupB

Vec190:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xbe, %d1
	bra	FaultHandlerEntryGroupB

Vec191:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xbf, %d1
	bra	FaultHandlerEntryGroupB

Vec192:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc0, %d1
	bra	FaultHandlerEntryGroupB

Vec193:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc1, %d1
	bra	FaultHandlerEntryGroupB

Vec194:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc2, %d1
	bra	FaultHandlerEntryGroupB

Vec195:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc3, %d1
	bra	FaultHandlerEntryGroupB

Vec196:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc4, %d1
	bra	FaultHandlerEntryGroupB

Vec197:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc5, %d1
	bra	FaultHandlerEntryGroupB

Vec198:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc6, %d1
	bra	FaultHandlerEntryGroupB

Vec199:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc7, %d1
	bra	FaultHandlerEntryGroupB

Vec200:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc8, %d1
	bra	FaultHandlerEntryGroupB

Vec201:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xc9, %d1
	bra	FaultHandlerEntryGroupB

Vec202:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xca, %d1
	bra	FaultHandlerEntryGroupB

Vec203:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xcb, %d1
	bra	FaultHandlerEntryGroupB

Vec204:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xcc, %d1
	bra	FaultHandlerEntryGroupB

Vec205:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xcd, %d1
	bra	FaultHandlerEntryGroupB

Vec206:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xce, %d1
	bra	FaultHandlerEntryGroupB

Vec207:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xcf, %d1
	bra	FaultHandlerEntryGroupB

Vec208:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd0, %d1
	bra	FaultHandlerEntryGroupB

Vec209:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd1, %d1
	bra	FaultHandlerEntryGroupB

Vec210:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd2, %d1
	bra	FaultHandlerEntryGroupB

Vec211:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd3, %d1
	bra	FaultHandlerEntryGroupB

Vec212:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd4, %d1
	bra	FaultHandlerEntryGroupB

Vec213:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd5, %d1
	bra	FaultHandlerEntryGroupB

Vec214:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd6, %d1
	bra	FaultHandlerEntryGroupB

Vec215:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd7, %d1
	bra	FaultHandlerEntryGroupB

Vec216:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd8, %d1
	bra	FaultHandlerEntryGroupB

Vec217:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xd9, %d1
	bra	FaultHandlerEntryGroupB

Vec218:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xda, %d1
	bra	FaultHandlerEntryGroupB

Vec219:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xdb, %d1
	bra	FaultHandlerEntryGroupB

Vec220:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xdc, %d1
	bra	FaultHandlerEntryGroupB

Vec221:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xdd, %d1
	bra	FaultHandlerEntryGroupB

Vec222:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xde, %d1
	bra	FaultHandlerEntryGroupB

Vec223:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xdf, %d1
	bra	FaultHandlerEntryGroupB

Vec224:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe0, %d1
	bra	FaultHandlerEntryGroupB

Vec225:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe1, %d1
	bra	FaultHandlerEntryGroupB

Vec226:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe2, %d1
	bra	FaultHandlerEntryGroupB

Vec227:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe3, %d1
	bra	FaultHandlerEntryGroupB

Vec228:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe4, %d1
	bra	FaultHandlerEntryGroupB

Vec229:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe5, %d1
	bra	FaultHandlerEntryGroupB

Vec230:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe6, %d1
	bra	FaultHandlerEntryGroupB

Vec231:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe7, %d1
	bra	FaultHandlerEntryGroupB

Vec232:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe8, %d1
	bra	FaultHandlerEntryGroupB

Vec233:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xe9, %d1
	bra	FaultHandlerEntryGroupB

Vec234:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xea, %d1
	bra	FaultHandlerEntryGroupB

Vec235:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xeb, %d1
	bra	FaultHandlerEntryGroupB

Vec236:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xec, %d1
	bra	FaultHandlerEntryGroupB

Vec237:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xed, %d1
	bra	FaultHandlerEntryGroupB

Vec238:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xee, %d1
	bra	FaultHandlerEntryGroupB

Vec239:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xef, %d1
	bra	FaultHandlerEntryGroupB

Vec240:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf0, %d1
	bra	FaultHandlerEntryGroupB

Vec241:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf1, %d1
	bra	FaultHandlerEntryGroupB

Vec242:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf2, %d1
	bra	FaultHandlerEntryGroupB

Vec243:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf3, %d1
	bra	FaultHandlerEntryGroupB

Vec244:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf4, %d1
	bra	FaultHandlerEntryGroupB

Vec245:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf5, %d1
	bra	FaultHandlerEntryGroupB

Vec246:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf6, %d1
	bra	FaultHandlerEntryGroupB

Vec247:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf7, %d1
	bra	FaultHandlerEntryGroupB

Vec248:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf8, %d1
	bra	FaultHandlerEntryGroupB

Vec249:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xf9, %d1
	bra	FaultHandlerEntryGroupB

Vec250:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xfa, %d1
	bra	FaultHandlerEntryGroupB

Vec251:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xfb, %d1
	bra	FaultHandlerEntryGroupB

Vec252:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xfc, %d1
	bra	FaultHandlerEntryGroupB

Vec253:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xfd, %d1
	bra	FaultHandlerEntryGroupB

Vec254:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xfe, %d1
	bra	FaultHandlerEntryGroupB

Vec255:
	movem.l	%d1-%d2, -(%sp)
	move.l	#0xff, %d1
	bra	FaultHandlerEntryGroupB

/* Fault handler entry points. At this point, d7/a7 have been saved on the stack.
 * d7 Contains the offending vector #
 */

FaultHandlerEntryGroupA:
	move	#0x2700, %sr			/* Shut off interrupts */
	move.l	%sp, %d2 			/* Save off our stack pointer into d2 */

	movel	#ROSCOE_STICKY_RAM_BASE + STICKYRAM_OFFSET_FAULT_REGS, %a7 /* Point to where we're storing out faulted registers */

/* Save off all registers*/

	moveml	%d0-%d7/%a0-%a7, (%a7)

	/* %d0 = Fake "return address" */
	/* %d1 = Vector */
	/* %d2 = Stack pointer */

	moveml	%d0-%d2, -(%a7)
	bra	InterruptFaultGroupA

FaultHandlerEntryGroupB:
	move	#0x2700, %sr			/* Shut off interrupts */
	move.l	%sp, %d2 			/* Save off our stack pointer into d2 */

	movel	#ROSCOE_STICKY_RAM_BASE + STICKYRAM_OFFSET_FAULT_REGS, %a7 /* Point to where we're storing out faulted registers */

/* Save off all registers*/

	moveml	%d0-%d7/%a0-%a7, (%a7)

	/* %d0 = Fake "return address" */
	/* %d1 = Vector */
	/* %d2 = Stack pointer */

	moveml	%d0-%d2, -(%a7)

	bra	InterruptFaultGroupB



BootLoaderFlashBase:
	.long	ROSCOE_FLASH_BOOT_BASE
BootLoaderSRAMBase:
	.long	ROSCOE_BOOT_BASE_SRAM

/* Binary to Hex table */

BinaryToHex:
	dc.b	'0'
	dc.b	'1'
	dc.b	'2'
	dc.b	'3'
	dc.b	'4'
	dc.b	'5'
	dc.b	'6'
	dc.b	'7'
	dc.b	'8'
	dc.b	'9'
	dc.b	'a'
	dc.b	'b'
	dc.b	'c'
	dc.b	'd'
	dc.b	'e'
	dc.b	'f'

/******************************************************************************
   Displays the LED segment bits in d7 to the POST LEDs, turns off interrupts, and halts
 ******************************************************************************/

POSTHalt:
	lea	ROSCOE_POST_SEGMENT_LEFT, %a6		/* Load POST LEDs in a6 */
	movel	%d7, %d6		/* Make a copy */
	lsrw	#8, %d7			/* Top byte left digit */
	moveb	%d7, (%a6)		/* Set the left POST LEDs */

	lea	ROSCOE_POST_SEGMENT_RIGHT, %a6
	moveb	%d6, (%a6)		/* Set the right POST LEDs */

/* Turn on all bar graph LEDs (drive low) */

	lea	ROSCOE_BOARD_STATUS_LED, %a0
	movew	#0x0, %d0
	movew	%d0, (%a0)

	HALT

/******************************************************************************
   Sends a 32 bit hex dump out the UART that's in d2.
 ******************************************************************************/

SendHex:
	lea	ROSCOE_UART_A, %a6
	lea	BinaryToHex + ROSCOE_FLASH_BOOT_BASE, %a5
	movew	#7, %d5			/* 8 digits minus 1 for dbra */

SendHexLoop:
	roll	#4, %d2			/* Rotate highest nibble to lowest nibble */
	moveb	%d2, %d0
	andil	#0x0f, %d0		/* Lowest 4 bits are all that matters */
	moveb	(%d0,%a5), %d0

/* d0 is the byte to send */

SendHexLoopTHRE:
	moveb	(UART_REG_LSR)(%a6), %d1
	andb	#UART_LSR_THRE, %d1
	beq	SendHexLoopTHRE

/* Send the byte */
	move.b	%d0, (UART_REG_RBRTHR)(%a6)
	dbra	%d5, SendHexLoop
	rts

/******************************************************************************
   Send a 0x00 terminated string pointed to by a5
 ******************************************************************************/

SendString:
	lea	ROSCOE_UART_A, %a6

SendStringLoop:
	moveb	(%a5)+, %d0
	tstb	%d0
	beq	SendStringDone

/* Wait until the THRE is empty */

SendStringLoopTHRE:
	moveb	(UART_REG_LSR)(%a6), %d1
	andb	#UART_LSR_THRE, %d1
	beq	SendStringLoopTHRE

/* Send the byte */
	move.b	%d0, (UART_REG_RBRTHR)(%a6)
	bras	SendStringLoop

SendStringDone:
	rts

/******************************************************************************
   Boot loader entry
 ******************************************************************************/

BootLoaderEntry:

/* Shut off interrupts */
	move	#0x2700, %sr

/* Turn off all LEDs - 7 segment and individual - to indicate POST start */
	lea	ROSCOE_POST_SEGMENT_LEFT, %a4
	POSTSet	((POST_7SEG_OFF << 8) + POST_7SEG_OFF)

/* And the individual LEDs */
	lea	ROSCOE_BOARD_STATUS_LED, %a0
	moveb	#0xff, %d0
	moveb	%d0, (%a0)

/* Mask all interrupts */
	lea	ROSCOE_INTC_MASK, %a0
	moveb	#0, %d0
	moveb	%d0, (%a0)
	lea	ROSCOE_INTC_MASK2, %a0
	moveb	%d0, (%a0)

/* Ensure VBR is set to base of boot loader flash to start with */
	movel	BootLoaderFlashBase, %a0
	movec	%a0, %vbr

/* Signal that we're starting the UART init */

	POSTSet	POSTCODE_UART_A_INIT

/* Set the interrupt identification register with a 0. It should return UART_IIR_NOT_PENDING */

	lea	ROSCOE_UART_A, %a6

	moveb	#(UART_IIR_FIFO_ENABLE + UART_IIR_FIFO_14), %d0	 	       	/* Turn on FIFOs */
	moveb	%d0, (UART_REG_IIR) (%a6)

/* Now read the IIR back */

	moveb	(UART_REG_IIR) (%a6), %d0
	cmpib	#(UART_IIR_FIFO_14 + UART_IIR_NOT_PENDING), %d0
	beq	UARTTestIIR

/* Failed UART presence test */
	HALT

/* Now try it again but shut off the FIFOs */

UARTTestIIR:
	moveb	#0x00, %d0			/* Turn off FIFOs */
	moveb	%d0, (UART_REG_IIR) (%a6)

/* Now read the IIR back */

	moveb	(UART_REG_IIR) (%a6), %d0
	cmpib	#UART_IIR_NOT_PENDING, %d0
	beq	UARTTestLCR

/* Failed UART presence test */
	HALT

UARTTestLCR:
	moveb	#0xa5, %d0
	moveb	%d0, (UART_REG_LCR)(%a6)
	moveb	(UART_REG_LCR)(%a6), %d0
	cmpib	#0xa5, %d0
	beq	UARTTestLCR2

/* Failed line control register test */
	HALT

UARTTestLCR2:
	moveb	#0x5a, %d0
	moveb	%d0, (UART_REG_LCR)(%a6)
	moveb	(UART_REG_LCR)(%a6), %d0
	cmpib	#0x5a, %d0
	beq	UARTInit

/* Failed line control register test */
	HALT

/* Initialize the UART to 115200kbps, 8 data bits, 1 stop bit, no FIFO, no interrupts */

/* Set baud rate */

UARTInit:
	moveb	#UART_LCR_DLAB_ENABLE, %d0
	moveb	%d0, (UART_REG_LCR)(%a6)

	moveb	#(BOOTLOADER_BAUD_RATE_DIVISOR & 0xff), %d0
	moveb	%d0, (UART_REG_RBRTHR)(%a6)

	moveb	#(BOOTLOADER_BAUD_RATE_DIVISOR >> 8), %d0
	moveb	%d0, (UART_REG_IER)(%a6)

/* Now set 8N1 and clear DLAB */

	moveb	#UART_LCR_8DB + UART_LCR_NO_PARITY + UART_LCR_1SB, %d0
	moveb	%d0, (UART_REG_LCR)(%a6)

/* Now assert DTR, RTS, OUT1 and OUT2 */

	moveb	#UART_MCR_DTR + UART_MCR_RTS + UART_MCR_OUT1 + UART_MCR_OUT2, %d0
	moveb	%d0, (UART_REG_MCR) (%a6)

/* Now we start the copy of our code to the base RAM and check it */
	POSTSet	POSTCODE_BOOTLOADER_COPY

	lea	__end, %a0
	movel	%a0, %a2

	lea	ROSCOE_FLASH_BOOT_BASE, %a0
	lea	ROSCOE_BOOT_BASE_SRAM, %a1

BootLoaderCopy:
	movel	(%a0)+, %d0
	movel	%d0, (%a1)+
	cmpl	%a1, %a2
	bne	BootLoaderCopy

/* Now we do a readback to see if the data was copied OK */

/* Figure out where the end address of everything is and turn it in to UINT32 counts */
	movel	%a2, %a0
	lea	ROSCOE_FLASH_BOOT_BASE, %a0
	lea	ROSCOE_BOOT_BASE_SRAM, %a1

BootLoaderCompare:
	movel	(%a0)+, %d3		/* Read data from flash */
	movel	(%a1)+, %d4		/* Read data from DRAM */
	cmpl	%d3, %d4		/* Are they the same? */
	bne	BootLoaderDRAMFault
	cmpl	%a1, %a2
	bne	BootLoaderCompare

/* Now init the heap - test it first */

	POSTSet	POSTCODE_BOOTLOADER_HEAP_INIT

/* Fill the zero init/heap/stack area with 0xa55a5aa5 and check it */

	lea	__bss_start, %a1
	lea	_stack, %a0
	movel	#0xa55a5aa5, %d3

HeapTestFillLoop1:
	movel	%d3, (%a1)+
	cmpl	%a0, %a1
	bne	HeapTestFillLoop1

/* Now run through and see if we see our same pattern */

	lea	__bss_start, %a1
	movel	#0x5aa5a55a, %d7

HeapTestCheckLoop1:
	movel	(%a1), %d4
	movel	%d7, (%a1)+
	cmpl	%d3, %d4
	bne	HeapTestCheckFailed
	cmpl	%a0, %a1
	bne	HeapTestCheckLoop1

/* Now run through and see if it's the same */

	lea	__bss_start, %a1
	movel	%d7, %d3
	movel	#0xffffffff, %d7

HeapTestCheckLoop2:
	movel	(%a1), %d4
	movel	%d7, (%a1)+
	cmpl	%d3, %d4
	bne	HeapTestCheckFailed
	cmpl	%a0, %a1
	bne	HeapTestCheckLoop2

/* Check it */

	lea	__bss_start, %a1
	movel	%d7, %d3

HeapTestFillLoop:
	movel	(%a1)+, %d4
	cmpl	%d3, %d4
	bne	HeapTestCheckFailed
	cmpl	%a0, %a1
	bne	HeapTestFillLoop

/* All good! Flag POST codes so we know we're dispatching to RAM */

	POSTSet	POSTCODE_BOOTLOADER_RAMEXEC

	lea	CRLFSequence + ROSCOE_FLASH_BOOT_BASE, %a5
	StacklessCall	SendString

/* Set the vector base register to the RAM position */

	movel	BootLoaderSRAMBase, %a0
	movecl	%a0, %vbr

	jmp	_start

HeapFault:
	dc.b	0x0d 
	dc.b	0x0a
	.ascii	"Heap fill/check at address 0x"
	dc.b	0x00

DRAMFault:
	dc.b	0x0d 
	dc.b	0x0a
	.ascii	"Flash->SRAM readback fault at address 0x"
	dc.b	0x00

DRAMFault2:
	.ascii	" - Expected 0x"
	dc.b	0x00

DRAMFault3:
	.ascii	", got 0x"
	dc.b	0x00

DRAMFault4:
	dc.b	0x0d 
	dc.b	0x0a
	dc.b	0x00

	.align	4

/* If we get here, then we had a heap fill/memory check problem

   a1 = DRAM Address + 4
   d3 = Correct data
   d4 = Incorrect data
*/

HeapTestCheckFailed:
	lea	HeapFault + ROSCOE_FLASH_BOOT_BASE, %a5
	StacklessCall	SendString
	bras	faultAddress

/* If we get here, then we had a readback failure/miscompare for the code area. On entry:

   a0 = Flash address + 4
   a1 = DRAM Address + 4
   d3 = Correct data
   d4 = Incorrect data
*/
	.align	4

BootLoaderDRAMFault:

/* Flash->RAM Readback fault at address */

	lea	DRAMFault + ROSCOE_FLASH_BOOT_BASE, %a5
	StacklessCall	SendString

/* Fault address in hex */

faultAddress:
	movel	%a1, %d2
	sub	#0x4, %d2 
	StacklessCall	SendHex

/* Expected */

	lea	DRAMFault2 + ROSCOE_FLASH_BOOT_BASE, %a5
	StacklessCall	SendString

/* Expected data in hex */

	movel	%d3, %d2
	StacklessCall	SendHex

/* Got */

	lea	DRAMFault3 + ROSCOE_FLASH_BOOT_BASE, %a5
	StacklessCall	SendString

/* Bad data in hex */

	movel	%d4, %d2
	StacklessCall	SendHex

/* CR/LF */
	lea	DRAMFault4 + ROSCOE_FLASH_BOOT_BASE, %a5
	StacklessCall	SendString
	HALT

	.global StackPointerGet
StackPointerGet:
	movel	%sp, %d0
	rts

	.global FramePointerGet
FramePointerGet:
	movel	%fp, %d0
	rts

CRLFSequence:
	dc.b	0x0d
	dc.b	0x0a
	dc.b	0x00



