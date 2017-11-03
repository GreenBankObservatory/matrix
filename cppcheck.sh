#!/bin/bash

# Run cppcheck and output results to text file
/home/sandboxes/monctrl/cppcheck-1.79/cppcheck --verbose --force `pwd` 2> cppcheck.txt
