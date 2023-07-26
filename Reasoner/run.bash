#!/bin/bash

echo run.bash
rm times.txt timesRL.txt
touch times.txt timesRL.txt
for i in {1..2..1}
do
    gnome-terminal -- ./runReconfigurate.bash
    if [ $(($i%2)) -eq 0 ];
    then
        ./runWasp.bash --executeRL=false;
    else
        ./runWasp.bash --executeRL=true;
    fi
done