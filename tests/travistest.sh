#!/bin/bash
mkdir /input
cd /input
curl -O http://physics.nyu.edu/~lh1132/DAOD_SUSY1.06451147._000041.pool.root.1
cd /analysis
source setup.sh
runSTTest