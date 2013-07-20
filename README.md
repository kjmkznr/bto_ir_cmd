bto_ir_cmd
==========

Bit Trade One IR Remocon tool for Linux

Require
-------
* \>=libusb-1.0.0

Compile
-------
  
    $ make

Usage
-----

Help:

    $ ./bto_ir_cmd -h
    usage: bto_ir_cmd <option>
      -r            Receive Infrared code.
      -t <code>     Transfer Infrared code.
      -e            Extend Infrared code.
                      Normal:  7 octets
                      Extend: 35 octets
    
IR Code Receive:

    $ ./bto_ir_cmd -e -r
    Extend mode on.
    Receive mode
      Received code: 014000AA5ACF16010221A1000000000000000000000000000000000000000000000000

IR Code Transfer:

    $ ./bto_ir_cmd -e -t 014000AA5ACF16010221A1000000000000000000000000000000000000000000000000


