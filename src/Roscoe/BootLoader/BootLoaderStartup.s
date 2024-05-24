/* Roscoe startup code
 */

#include "../Hardware/Roscoe.h"
#include "../Shared/16550.h"

	.title "BootLoaderStartup.s"

	.extern SYM (_start)
	.extern SYM (InterruptServiceRoutine)
	.extern	SYM (_end)
	.extern	SYM (_stack)
	.global	BootLoaderEntry

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
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec3:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec4:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec5:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec6:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec7:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec8:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec9:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec10:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec11:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec12:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec13:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec14:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec15:
	movew	#(((POST_7SEG_HEX_0 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec16:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec17:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec18:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec19:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec20:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec21:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec22:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec23:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec24:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec25:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec26:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec27:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec28:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec29:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec30:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec31:
	movew	#(((POST_7SEG_HEX_1 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec32:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec33:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec34:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec35:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec36:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec37:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec38:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec39:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec40:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec41:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec42:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec43:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec44:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec45:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec46:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec47:
	movew	#(((POST_7SEG_HEX_2 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec48:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec49:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec50:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec51:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec52:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec53:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec54:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec55:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec56:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec57:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec58:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec59:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec60:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec61:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec62:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec63:
	movew	#(((POST_7SEG_HEX_3 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec64:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec65:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec66:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec67:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec68:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec69:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec70:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec71:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec72:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec73:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec74:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec75:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec76:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec77:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec78:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec79:
	movew	#(((POST_7SEG_HEX_4 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec80:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec81:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec82:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec83:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec84:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec85:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec86:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec87:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec88:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec89:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec90:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec91:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec92:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec93:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec94:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec95:
	movew	#(((POST_7SEG_HEX_5 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec96:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec97:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec98:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec99:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec100:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec101:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec102:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec103:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec104:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec105:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec106:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec107:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec108:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec109:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec110:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec111:
	movew	#(((POST_7SEG_HEX_6 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec112:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec113:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec114:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec115:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec116:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec117:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec118:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec119:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec120:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec121:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec122:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec123:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec124:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec125:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec126:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec127:
	movew	#(((POST_7SEG_HEX_7 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec128:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec129:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec130:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec131:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec132:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec133:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec134:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec135:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec136:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec137:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec138:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec139:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec140:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec141:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec142:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec143:
	movew	#(((POST_7SEG_HEX_8 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec144:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec145:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec146:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec147:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec148:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec149:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec150:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec151:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec152:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec153:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec154:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec155:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec156:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec157:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec158:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec159:
	movew	#(((POST_7SEG_HEX_9 & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec160:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec161:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec162:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec163:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec164:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec165:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec166:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec167:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec168:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec169:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec170:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec171:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec172:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec173:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec174:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec175:
	movew	#(((POST_7SEG_HEX_A & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec176:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec177:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec178:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec179:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec180:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec181:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec182:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec183:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec184:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec185:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec186:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec187:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec188:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec189:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec190:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec191:
	movew	#(((POST_7SEG_HEX_B & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec192:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec193:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec194:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec195:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec196:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec197:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec198:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec199:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec200:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec201:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec202:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec203:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec204:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec205:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec206:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec207:
	movew	#(((POST_7SEG_HEX_C & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec208:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec209:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec210:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec211:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec212:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec213:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec214:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec215:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec216:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec217:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec218:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec219:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec220:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec221:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec222:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec223:
	movew	#(((POST_7SEG_HEX_D & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec224:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec225:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec226:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec227:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec228:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec229:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec230:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec231:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec232:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec233:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec234:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec235:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec236:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec237:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec238:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec239:
	movew	#(((POST_7SEG_HEX_E & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec240:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_0 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec241:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_1 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec242:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_2 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec243:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_3 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec244:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_4 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec245:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_5 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec246:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_6 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec247:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_7 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec248:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_8 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec249:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_9 & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec250:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_A & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec251:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_B & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec252:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_C & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec253:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_D & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec254:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_E & POST_7SEG_DP)), %d7
	bra	POSTHalt

Vec255:
	movew	#(((POST_7SEG_HEX_F & POST_7SEG_DP) << 8) + (POST_7SEG_HEX_F & POST_7SEG_DP)), %d7
	bra	POSTHalt

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
	lsrb	#8, %d7			/* Top byte left digit */
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
	moveb	#0xff, %d0
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


