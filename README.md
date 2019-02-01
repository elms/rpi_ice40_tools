What is this?
-------------

Raspberry Pi tools for working with iCE40 FPGAs

It's just experimental and working with some custom hardware. If it
works well, parameters to support other boards will be easy to add.


Build
-----

```
make
```


Configure FPGAs
---------------

```
sudo ./rpi_ice40_config -1r rgb.bin # reset both and configure FPGA1
sudo ./rpi_ice40_config -2 rgb.bin  # now configure FPGA2
```
