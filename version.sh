#!/bin/sh
VI='version.in'
VF=$1
TF=`tempfile`
RV=`head -n1 version.in`
SV=`svnversion | grep -E '^[0-9]+.*'`
echo "$RV" > "$VF"
echo "#define SVNVER \"$SV $2\"" >> "$VF"
cat "$VF"
