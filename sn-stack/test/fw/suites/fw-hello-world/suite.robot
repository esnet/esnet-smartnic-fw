*** Settings ***
Documentation    A simple test suite file for firmware.
Library          fw.hello.Library


*** Test Cases ***
FW Hello World
    Print    Hello world from firmware!
