#!/bin/bash
./waf --run "center-manet --nSinks=1" > 3-0-1.txt
./waf --run "center-manet --nSinks=2" > 3-0-2.txt
./waf --run "center-manet --nSinks=3" > 3-0-3.txt
./waf --run "center-manet --nSinks=4" > 3-0-4.txt
./waf --run "center-manet --nSinks=5" > 3-0-5.txt
