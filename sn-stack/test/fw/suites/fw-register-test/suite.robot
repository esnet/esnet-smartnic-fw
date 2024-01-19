*** Settings ***
Documentation    Basic register tests.
Library          fw.registers.Library
Variables        variables


*** Test Cases ***
FW Word to Bytes Endian Test
    Endian Check Packed to Unpacked    ${dev}    ${0x44332211}
    Endian Check Packed to Unpacked    ${dev}    ${0x11223344}

FW Bytes to Word Endian Test
    Endian Check Unpacked to Packed    ${dev}    ${0x11223344}
    Endian Check Unpacked to Packed    ${dev}    ${0x44332211}
