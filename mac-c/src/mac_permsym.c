/**************************************************************************
** MAC-C PERMANENT SYMBOL TABLE - byte-verified against MAC.BPUN         **
**                                                                       **
** GENERATED, DO NOT HAND-EDIT. Every (name,value) pair below was        **
** decoded directly from the permanent symbol table in the real MAC      **
** binary D:\ND\BPUN\MAC.BPUN ("MAC - 14. MARS 1978"), 3-word entries    **
** starting at address octal 164663 (0xE9B3), names packed 5 chars x     **
** 6 bits. The mac_sym_class column is assigned from the mnemonic:       **
**   MRI   = memory-reference (address operand -> 8-bit displacement)    **
**   JUMP8 = conditional jump (8-bit P-relative displacement)            **
**   ARG8  = argument instr SAA/AAA/... (operand & 0377)                 **
**   PLAIN = value contributes its full 16 bits to the word sum          **
** Duplicate values are intentional and match the binary (STF==STR,      **
** LDF==LDR==LDD-adjacent, IIC==IIE, etc.).                              **
**                                                                       **
** NLZ/DNZ are ARG8, NOT PLAIN. Their scaling factor is an 8-bit field   **
** (NLZ 0151400-0151777, DNZ 0152000-0152377), so a PLAIN 16-bit sum     **
** lets a negative scaling borrow out of it and change the OPCODE:       **
** "DNZ -20" became 0152000-0000020 = 0151760, which is not DNZ at all   **
** but NLZ+0360 - the machine normalised where the program asked it to   **
** denormalise. ND-60.096.01 sec 3.2.2.4 describes exactly this borrow   **
** and its fix ("Bit 7 is now examined and it is 1, so 400 is added"),   **
** which is what ARG8's "opc + (arg & 0377)" performs: 0152360. Twelve   **
** sites in TSS use "DNZ -20", all of them the float-to-integer step of  **
** the date/time arithmetic. Positive operands are unaffected. Do NOT    **
** apply ARG8 to SHA/SHT/SHD: their shift count is a 6-bit field with    **
** modifier bits above it, so an 8-bit mask would be a different bug.    **
** Pinned by test [18].                                                  **
**                                                                       **
** Ronny Hansen                                                          **
***************************************************************************/
#include "mac.h"

const mac_permsym_entry MAC_PERMSYM[] =
{
    { "LDR", 034000, MAC_CLS_MRI },
    { "STR", 030000, MAC_CLS_MRI },
    { "LDD", 024000, MAC_CLS_MRI },
    { "FDV", 0114000, MAC_CLS_MRI },
    { "FMU", 0110000, MAC_CLS_MRI },
    { "FSB", 0104000, MAC_CLS_MRI },
    { "FAD", 0100000, MAC_CLS_MRI },
    { "LDF", 034000, MAC_CLS_MRI },
    { "JXN", 0133400, MAC_CLS_JUMP8 },
    { "JXZ", 0133000, MAC_CLS_JUMP8 },
    { "JNC", 0132400, MAC_CLS_JUMP8 },
    { "JPC", 0132000, MAC_CLS_JUMP8 },
    { "JAZ", 0131000, MAC_CLS_JUMP8 },
    { "JAN", 0130400, MAC_CLS_JUMP8 },
    { "JAF", 0131400, MAC_CLS_JUMP8 },
    { "JAP", 0130000, MAC_CLS_JUMP8 },
    { "ORA", 074000, MAC_CLS_MRI },
    { "AND", 070000, MAC_CLS_MRI },
    { "STD", 020000, MAC_CLS_MRI },
    { "STF", 030000, MAC_CLS_MRI },
    { "STZ", 00, MAC_CLS_MRI },
    { "MPY", 0120000, MAC_CLS_MRI },
    { "STT", 010000, MAC_CLS_MRI },
    { "LDT", 050000, MAC_CLS_MRI },
    { "STX", 014000, MAC_CLS_MRI },
    { "LDX", 054000, MAC_CLS_MRI },
    { "MIN", 040000, MAC_CLS_MRI },
    { "SUB", 064000, MAC_CLS_MRI },
    { "ADD", 060000, MAC_CLS_MRI },
    { "JPL", 0134000, MAC_CLS_MRI },
    { "JMP", 0124000, MAC_CLS_MRI },
    { "STA", 04000, MAC_CLS_MRI },
    { "LDA", 044000, MAC_CLS_MRI },
    { "AAB", 0172000, MAC_CLS_ARG8 },
    { "SAB", 0170000, MAC_CLS_ARG8 },
    { "AAT", 0173000, MAC_CLS_ARG8 },
    { "SAT", 0171000, MAC_CLS_ARG8 },
    { "AAX", 0173400, MAC_CLS_ARG8 },
    { "SAX", 0171400, MAC_CLS_ARG8 },
    { "AAA", 0172400, MAC_CLS_ARG8 },
    { "SAA", 0170400, MAC_CLS_ARG8 },
    { "DNZ", 0152000, MAC_CLS_ARG8  },
    { "NLZ", 0151400, MAC_CLS_ARG8  },
    { ",X", 02000, MAC_CLS_PLAIN },
    { ",B", 0400, MAC_CLS_PLAIN },
    { "I", 01000, MAC_CLS_PLAIN },
    { "SHR", 0200, MAC_CLS_PLAIN },
    { "DX", 07, MAC_CLS_PLAIN },
    { "DT", 06, MAC_CLS_PLAIN },
    { "DA", 05, MAC_CLS_PLAIN },
    { "DL", 04, MAC_CLS_PLAIN },
    { "DB", 03, MAC_CLS_PLAIN },
    { "DP", 02, MAC_CLS_PLAIN },
    { "DD", 01, MAC_CLS_PLAIN },
    { "SX", 070, MAC_CLS_PLAIN },
    { "ST", 060, MAC_CLS_PLAIN },
    { "SA", 050, MAC_CLS_PLAIN },
    { "SL", 040, MAC_CLS_PLAIN },
    { "SB", 030, MAC_CLS_PLAIN },
    { "SP", 020, MAC_CLS_PLAIN },
    { "SD", 010, MAC_CLS_PLAIN },
    { "COPY", 0146100, MAC_CLS_PLAIN },
    { "SWAP", 0144000, MAC_CLS_PLAIN },
    { "BSET", 0174000, MAC_CLS_PLAIN },
    { "BSKP", 0175000, MAC_CLS_PLAIN },
    { "ZRO", 00, MAC_CLS_PLAIN },
    { "ONE", 0200, MAC_CLS_PLAIN },
    { "RADD", 0146000, MAC_CLS_PLAIN },
    { "RSUB", 0146600, MAC_CLS_PLAIN },
    { "EXIT", 0146142, MAC_CLS_PLAIN },
    { "LST", 03000, MAC_CLS_PLAIN },
    { "GRE", 01000, MAC_CLS_PLAIN },
    { "UEQ", 02000, MAC_CLS_PLAIN },
    { "EQL", 00, MAC_CLS_PLAIN },
    { "SKP", 0140000, MAC_CLS_PLAIN },
    { "SAD", 0154600, MAC_CLS_PLAIN },
    { "SHA", 0154400, MAC_CLS_PLAIN },
    { "SHD", 0154200, MAC_CLS_PLAIN },
    { "SHT", 0154000, MAC_CLS_PLAIN },
    { "LIN", 03000, MAC_CLS_PLAIN },
    { "ZIN", 02000, MAC_CLS_PLAIN },
    { "ROT", 01000, MAC_CLS_PLAIN },
    { "IF", 00, MAC_CLS_PLAIN },
    { "ADC", 01000, MAC_CLS_PLAIN },
    { "AD1", 0400, MAC_CLS_PLAIN },
    { "RDCR", 0146200, MAC_CLS_PLAIN },
    { "RINC", 0146400, MAC_CLS_PLAIN },
    { "RCLR", 0146100, MAC_CLS_PLAIN },
    { "CM1", 0200, MAC_CLS_PLAIN },
    { "CM2", 0600, MAC_CLS_PLAIN },
    /* CLD = clear-destination register-operation sub-field, bit 6.
     * MISSED by the original 0xE9B3 extraction (COPY=RADD|CLD=0146100 gives it
     * away). Confirmed EMPIRICALLY on the real MAC: it assembles
     *   "SWAP CLD DT SA" -> 144156   and   "SWAP DT SA" -> 144056
     * whose difference is exactly 0100. Without this entry mac-c silently
     * dropped the clear bit on every "SWAP CLD ..." (5 sites in TSS). Adding it
     * is FAITHFUL to real MAC, not a deviation. TODO: re-audit the permsym
     * extraction for other missing register-op modifiers.                      */
    { "CLD", 0100, MAC_CLS_PLAIN },
    { "REXO", 0145000, MAC_CLS_PLAIN },
    { "RORA", 0145400, MAC_CLS_PLAIN },
    { "RAND", 0144400, MAC_CLS_PLAIN },
    { "BORA", 0177600, MAC_CLS_PLAIN },
    { "BAND", 0177200, MAC_CLS_PLAIN },
    { "BORC", 0177400, MAC_CLS_PLAIN },
    { "BANC", 0177000, MAC_CLS_PLAIN },
    { "BLDC", 0176400, MAC_CLS_PLAIN },
    { "BLDA", 0176600, MAC_CLS_PLAIN },
    { "BSTC", 0176000, MAC_CLS_PLAIN },
    { "BSTA", 0176200, MAC_CLS_PLAIN },
    { "BAC", 0600, MAC_CLS_PLAIN },
    { "BCM", 0400, MAC_CLS_PLAIN },
    { "WAIT", 0151000, MAC_CLS_PLAIN },
    { "MST", 0150300, MAC_CLS_PLAIN },
    { "MCL", 0150200, MAC_CLS_PLAIN },
    { "TRR", 0150100, MAC_CLS_PLAIN },
    { "PIE", 07, MAC_CLS_PLAIN },
    { "PID", 06, MAC_CLS_PLAIN },
    { "STS", 01, MAC_CLS_PLAIN },
    { "OPR", 02, MAC_CLS_PLAIN },
    { "TRA", 0150000, MAC_CLS_PLAIN },
    { "2OR3", 03, MAC_CLS_PLAIN },
    { "IOF", 0150401, MAC_CLS_PLAIN },
    { "ION", 0150402, MAC_CLS_PLAIN },
    { "SSTG", 010, MAC_CLS_PLAIN },
    { "RMPY", 0141200, MAC_CLS_PLAIN },
    { "RDIV", 0141600, MAC_CLS_PLAIN },
    { "PVL", 04, MAC_CLS_PLAIN },
    { "POF", 0150404, MAC_CLS_PLAIN },
    { "PON", 0150410, MAC_CLS_PLAIN },
    { "PCR", 03, MAC_CLS_PLAIN },
    { "PL13", 043, MAC_CLS_PLAIN },
    { "PL12", 022, MAC_CLS_PLAIN },
    { "PL11", 011, MAC_CLS_PLAIN },
    { "PL10", 04, MAC_CLS_PLAIN },
    { "PGS", 03, MAC_CLS_PLAIN },
    { "PES", 013, MAC_CLS_PLAIN },
    { "PEA", 015, MAC_CLS_PLAIN },
    { "MIX3", 0143200, MAC_CLS_PLAIN },
    { "MON", 0153000, MAC_CLS_PLAIN },
    { "LSS", 02400, MAC_CLS_PLAIN },
    { "GEQ", 0400, MAC_CLS_PLAIN },
    { "MLST", 03400, MAC_CLS_PLAIN },
    { "MGRE", 01400, MAC_CLS_PLAIN },
    { "SRB", 0152402, MAC_CLS_PLAIN },
    { "LRB", 0152600, MAC_CLS_PLAIN },
    { "LMP", 02, MAC_CLS_PLAIN },
    { "SBYT", 0142600, MAC_CLS_PLAIN },
    { "LBYT", 0142200, MAC_CLS_PLAIN },
    { "IOX", 0164000, MAC_CLS_PLAIN },
    { "IIE", 05, MAC_CLS_PLAIN },
    { "IIC", 05, MAC_CLS_PLAIN },
    { "IDENT", 0143600, MAC_CLS_PLAIN },
    { "IRW", 0153400, MAC_CLS_PLAIN },
    { "IRR", 0153600, MAC_CLS_PLAIN },
    { "EXR", 0140600, MAC_CLS_PLAIN },
    { "ALD", 012, MAC_CLS_PLAIN },
    { "SSM", 070, MAC_CLS_PLAIN },
    { "SSC", 060, MAC_CLS_PLAIN },
    { "SSO", 050, MAC_CLS_PLAIN },
    { "SSQ", 040, MAC_CLS_PLAIN },
    { "SSZ", 030, MAC_CLS_PLAIN },
    { "SSK", 020, MAC_CLS_PLAIN },
};

const size_t MAC_PERMSYM_COUNT = sizeof(MAC_PERMSYM) / sizeof(MAC_PERMSYM[0]);
