#!/bin/sh
VI=$1
VF=$2
TAG=$3
TF=`tempfile`
RV=`head -n1 version.in`
SV="$(svnversion | grep -E '^[0-9]+[^\s]*' | tr ':' '_' )"
if [ -z "$SV" ]; then
    SV="git$(git show -s --pretty=format:%h)"
fi
echo "$RV" > "$VF"
echo "#define SVNVER \"$SV$TAG\"" >> "$VF"
cat "$VF"
