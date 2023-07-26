#!/bin/bash

cd /home/mourad/WaspRL/
pwd
echo runReconfigurate.bash
until [ -p reconfigurate ]
do
    sleep 0.0000000001
done
# cat pup-instances/stream/partnerunits-dbl-11-3.64-5 > reconfigurate
cat output.lp > reconfigurate
