#!/bin/bash
./waf --run "corner-manet --nSinks=1" > 2-0-1.txt
./waf --run "corner-manet --nSinks=2" > 2-0-2.txt
./waf --run "corner-manet --nSinks=3" > 2-0-3.txt
./waf --run "corner-manet --nSinks=4" > 2-0-4.txt
./waf --run "corner-manet --nSinks=5" > 2-0-5.txt

