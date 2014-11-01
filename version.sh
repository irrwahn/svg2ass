#!/bin/sh
VI=$1
VF=$2
TAG=$3
TF=`tempfile`
RV=`head -n1 version.in`
SV=`svnversion | grep -E '^[0-9]+[^\s]*' || echo 'Unknown'`
echo "$RV" > "$VF"
echo "#define SVNVER \"$SV$TAG\"" >> "$VF"
cat "$VF"
