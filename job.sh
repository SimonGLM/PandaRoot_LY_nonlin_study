#!/bin/bash

# This script is indended to be used by GNU parallel
# Using `nice -n5` to give other processes priority to not freeze up the system.

# Usage:
# $ parallel ./job.sh {} ::: 0 1 2 3 4 5

pos=$1
pdg=13
p=0.08
nevts=100000
# 10113004 crystal coordinates:
# x=[56.952,76.952]cm
# y=3.2cm
# z=3.8cm
posexact=$(echo 56.952+$pos | bc)
pospadded=$(printf "%02d" $pos) # Pad position variable with leading zeros for better sorting
name="data/muonX${pospadded}"

# Important note:
# In my thesis I conviniently used the fact that the particular investigated crystal (DetID: 10113004) is parallel to the x-Axis.
# Therefore I was able to simply add $pos onto the x value where the crystal began. (See coords above)
# You can freely choose the position of the BoxGen and its direction to test any crystal.
# Please modify accordingly.

nice -n5 root -l -b -q LY_sim.C\($nevts,\"$name\",\"box:\[type\($pdg,1\):p\($p\):tht\(0\):phi\(0\):xyz\($posexact,3.2,3.8\)\]\",0,\"TGeant4\"\)
nice -n5 root -l -b -q LY_digi.C\(0,\"$name\"\)
nice -n5 root -l -b -q LY_ana.C\(0,\"$name\"\)
