# vortrack
A simple VOR receiver
git the radial in degree from the VOR.


## Usage
> vortrack [-g gain] [-l interval ] [-r device] frequency (in Mhz)

 -l interval : interval in second between two measurements

 -g gain in tens of db (ie : -g 400 ) 

 -p ppm :  ppm freq shift

 -r n : rtl device number (mandatory for rtl sdr)

## Example

> vortrack 109.25

## Build

airwav depends of usb lib and rtlsdr or airspy lib

For rtl sdr :
> make -f Makefile.rtl

for airspy :
> make -f Makefile.airspy

