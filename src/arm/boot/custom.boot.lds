OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)

/* Entry Point  */
ENTRY(Custom)

/* LPC2103 memory areas  */
MEMORY 
{
/* CDFJ+          these values get auto adjusted and need to maintain their digit size */
    boot (RX)   : ORIGIN = 0x2800    , LENGTH = 0x60		/* C-runtime booter */
    C_code (RX) : ORIGIN = 0x2860    , LENGTH = 0x5790		/* C code size */
    ram         : ORIGIN = 0x40001800, LENGTH = 0x0800		/* free RAM */    
}

/* Global symbol _stack_end */
/* _stack_end = 0x40001FDC; */

/* Output sections  */
SECTIONS
{
    .bootcode :
    {
        _sboot = .;
        *(.boot)
        . = ALIGN(4);
        _eboot = .;
    } > boot

    .text : {
        .stext = .;
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } >C_code
    
    .data : AT ( _etext ) {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } >ram
    
    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >ram
}
end = .;
