#!/bin/bash

source /opt/sphenix/core/bin/sphenix_setup.sh -n new

export USER="$(id -u -n)"
export LOGNAME=${USER}
export HOME=/sphenix/u/${LOGNAME}

# uncomment for local development
export MYINSTALL=${HOME}/install
export LD_LIBRARY_PATH=$MYINSTALL/lib:$LD_LIBRARY_PATH
export ROOT_INCLUDE_PATH=$MYINSTALL/include:$ROOT_INCLUDE_PATH
export PATH="$HOME/.local/bin:$PATH"
#
source /opt/sphenix/core/bin/setup_local.sh $MYINSTALL

this_script=$BASH_SOURCE
this_script=`readlink -f $this_script`
this_dir=`dirname $this_script`
tutorial_dir=`dirname $this_dir`
macro_dir=$tutorial_dir/macro
echo rsyncing from $macro_dir
echo running: $this_script $*

if [[ ! -z "$_CONDOR_SCRATCH_DIR" && -d $_CONDOR_SCRATCH_DIR ]]
then
  cd $_CONDOR_SCRATCH_DIR
  rsync -av $macro_dir/* .
else
  echo condor scratch NOT set
  exit -1
fi

nEvents=$1
inDst=$2
outDir=$3
nSkip=$4

if [[ "${inDst}" == *.root ]]; then
  getinputfiles.pl $inDst
elif [[ "${inDst}" == *.list ]]; then
  getinputfiles.pl --filelist $inDst
fi

# print the environment - needed for debugging
#printenv

echo running: runTrackCaloMatch.sh $*
root.exe -q -b Fun4All_TrackCaloMatch.C\(${nEvents},\"${inDst}\",\"${outDir}\",${nSkip}\)
echo Script done
