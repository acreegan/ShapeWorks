#!/bin/bash

shapeworks readimage --name $DATA/1x2x2.nrrd threshold compare --name $DATA/threshold1.nrrd
if [[ $? == 1 ]]; then exit -1; fi
shapeworks readimage --name $DATA/1x2x2.nrrd threshold --min 0.0 --max 1.0 compare --name $DATA/threshold2.nrrd
if [[ $? == 1 ]]; then exit -1; fi
shapeworks readimage --name $DATA/1x2x2.nrrd threshold --min -0.1 compare --name $DATA/threshold3.nrrd
