#!/bin/bash

cd /home/mourad/WaspRL/
pwd
echo runWasp.bash
time=$(./build/release/wasp "$1" < nqueens-instances/stream/enc-30)
if [ $1 = "--executeRL=true" ];
then
    echo "${time}" >> timesRL.txt
else
    echo "${time}" >> times.txt
fi