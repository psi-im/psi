#!/bin/bash

if [ ! -z $1 ]; then
    listpaths="$@"
else
    listpaths="cutestuff iris src"
fi

for listpath in $listpaths; do
    listpath_dir=$listpath
    if [ ! -d "$listpath_dir" ]; then
        listpath_dir=${listpath_dir%/*}            # bash-ism
    fi

    for n in `svn list -R $listpath`; do
        n=$listpath_dir/$n
        dirpath=${n%/*}            # bash-ism
        filename=${n##*/}          # bash-ism

        # directory
	if [ -z "$filename" ]; then
            continue
        fi

        filebase=${filename%%.*}   # bash-ism
        fileext=${filename#*.}     # bash-ism
        pfilebase=${filebase%%_b*} # bash-ism

        bfilename=${filebase}.$fileext
        pfilename=${pfilebase}.$fileext

        bfilepath=$dirpath/$bfilename
        pfilepath=psi/$dirpath/$pfilename

        # not in cuda repository
	if [ ! -e "$bfilepath" ]; then
            continue
        fi

        # not in psi repository
	if [ ! -e "$pfilepath" ]; then
            continue
        fi

        diffcmd="diff -u $pfilepath $bfilepath"
        echo $diffcmd
        $diffcmd
    done
done
