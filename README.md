
DebugTools for ESP8266/Arduino.

This is a collection of miscellaneous bits of code I have been using to
debug issues with the ESP8266 and the Arduino IDE.

Some parts of the code may mot work, may not work well, may be partly written
or conflict with parts of the ESP8266/Arduino library as I have modified
some of its features as part of the debugging process.

It is provided as-is, with little documentation other than what comments are
in the source, and I provide no support.

Particular features/tests are mostly contained in their own file, with
DebugTools.ino tying them together / providing examples of use.

See config.h to turn on/off various tests and settings.
Update credentials.h with your wifi network details.


Scripts
-------
stack_decode.pl looks at the .elf and .ld files to provide a more
comprehensive (and sometimes better) stack decode.
It also tries to traverse the stack in both directions.

calltree.pl reads Trebisky's decoded boot.txt available from 
https://github.com/trebisky/esp8266/tree/master/reverse/bootrom
to build a call tree for functions in the rom.
