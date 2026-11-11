#!/bin/bash

# Condor execute nodes may not have a working UID->username lookup (NSS).
# In that case 'id -un' may return the numeric UID, which would incorrectly
# construct /sphenix/u/<uid>/setup.sh.  Use this production repo owner's
# account name as a fallback; no change is needed to getenv=False in the job.
account_name=$(id -un 2>/dev/null || true)
if [[ -z "$account_name" || "$account_name" =~ ^[0-9]+$ ]]; then
  account_name="jlnliu"
  echo "WARNING: UID $(id -u) has no resolvable username on $(hostname); using $account_name for setup." >&2
fi
export USER="$account_name"
export LOGNAME="$account_name"
export HOME="/sphenix/u/$account_name"

# Initialize the sPHENIX environment and local installation in one place.
if [[ ! -r "$HOME/setup.sh" ]]; then
  echo "ERROR: setup file is not readable: $HOME/setup.sh (UID $(id -u))." >&2
  exit 1
fi
source "$HOME/setup.sh" || exit 1

export PATH="$HOME/.local/bin:$PATH"

# Record the environment for cross-node debugging.
echo "HOSTNAME: $(hostname)"
echo "OFFLINE_MAIN: $OFFLINE_MAIN"
echo "MYINSTALL: $MYINSTALL"
echo "TrackToCalo library:"
readlink -f "$MYINSTALL/lib/libtrack_to_calo.so"

this_script=$BASH_SOURCE
this_script=`readlink -f $this_script`
this_dir=`dirname $this_script`
tutorial_dir=`dirname $this_dir`
macro_dir=$tutorial_dir/macro
echo rsyncing from $macro_dir
echo running: $this_script $*

# Resolve input paths before changing directories.
# Condor may supply a relative file list staged in its scratch directory.
nEvents=${1:?Missing number of events}
inDst=${2:?Missing input ROOT file or list}
outDir=${3:?Missing output directory}
nSkip=${4:-0}
writeJetChargeTree=${5:-true}
jetMode=${6:-unsubtracted}
enableTrackToCalo=${7:-true}
minTowerPt=${8:-0.0}
if [[ "$writeJetChargeTree" != true && "$writeJetChargeTree" != false ]]; then
  echo 'ERROR: fifth argument must be true or false' >&2
  exit 1
fi
if [[ "$jetMode" != unsubtracted && "$jetMode" != sub1 ]]; then
  echo "ERROR: sixth argument jetMode must be unsubtracted or sub1" >&2
  exit 1
fi
if [[ "$enableTrackToCalo" != true && "$enableTrackToCalo" != false ]]; then
  echo 'ERROR: seventh argument enableTrackToCalo must be true or false' >&2
  exit 1
fi

if ! [[ "$minTowerPt" =~ ^([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+]?[0-9]+)?$ ]]; then
  echo "ERROR: eighth argument minTowerPt must be a nonnegative finite number (GeV/c)" >&2
  exit 1
fi

launch_dir=$PWD
if [[ "$inDst" != /* ]]; then
  if [[ -e "$launch_dir/$inDst" ]]; then
    inDst=$(readlink -f "$launch_dir/$inDst")
  elif [[ -n "${_CONDOR_SCRATCH_DIR:-}" && -e "$_CONDOR_SCRATCH_DIR/$inDst" ]]; then
    inDst=$(readlink -f "$_CONDOR_SCRATCH_DIR/$inDst")
  fi
fi

if [[ -n "${_CONDOR_SCRATCH_DIR:-}" ]]; then
  if [[ ! -d "$_CONDOR_SCRATCH_DIR" ]]; then
    echo "ERROR: Condor scratch directory does not exist: $_CONDOR_SCRATCH_DIR" >&2
    exit 1
  fi
  echo "Condor mode: $_CONDOR_SCRATCH_DIR"
  cd "$_CONDOR_SCRATCH_DIR" || exit 1
  rsync -av "$macro_dir/" ./ || exit 1
else
  echo "Interactive mode: $macro_dir"
  cd "$macro_dir" || exit 1
fi

if [[ "$inDst" == *.root ]]; then
  if [[ ! -f "$inDst" ]]; then
    getinputfiles.pl "$inDst" || exit 1
  fi
elif [[ "$inDst" == *.list ]]; then
  getinputfiles.pl --filelist "$inDst" || exit 1
fi

# print the environment - needed for debugging
#printenv

echo running: runTrackCaloMatch.sh $*
root.exe -q -b Fun4All_TrackCaloMatch.C\(${nEvents},\"${inDst}\",\"${outDir}\",${nSkip},false,${writeJetChargeTree},\"${jetMode}\",${enableTrackToCalo},${minTowerPt}\)
status=$?

echo "ROOT exit status: $status"
exit "$status"