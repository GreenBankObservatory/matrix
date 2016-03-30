
Toy Oscilloscope Demo
=====================

The directory contains the gui portion of the Toy demo, and is not
required, but illustrates how an external application might
interface with a matrix-based application.

Building the Demo:
------------------ 

To build you will need both qt4 and qwt. The source here is just a
modified version of a qwt example. If those are installed then try:

* Edit oscilliscope.pro and change the include and link paths to
  your setup. Sorry no autoconf here.
* Run qmake-qt4 -makefile; This will generate a Makefile.
* Run make

Running the demo:
-----------------
* setup your LD_LIBRARY_PATH to include the paths to the matrix,
  YAML, and ZMQ libraries. This is not necessary if you:
   * a) Install matrix,YAML,ZMQ and boost in the normal /usr/local area.
   * b) Or add the matrix,YAML and ZMQ directories to /etc/ld.so.conf.d
* First run the toyscope demo.
* Then start the oscilloscope gui.
* Have fun.

Changed files in the GUI:
-------------------------

The only changes made to the demo were:
* Editing the oscilliscope.pro
* changes were limited to samplingthread.{h,cc}

