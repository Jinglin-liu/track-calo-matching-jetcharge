#!/bin/bash

script_path=$(readlink -f "${BASH_SOURCE[0]}")
script_dir=$(dirname "${script_path}")
repo_dir=$(dirname "${script_dir}")
cd "${repo_dir}"

get_closest_numbers() {
    number=$1
    lower=$(( (number / 100) * 100 ))
    upper=$(( lower + 100 ))
    echo "$lower $upper"
}

runspecies='run3pp'
runtype='physics'
anabuild='ana556'
cdbtag='2025p013'
version='v001'

path=/sphenix/lustre01/sphnxpro/production/${runspecies}/${runtype}/${anabuild}_${cdbtag}_${version}
echo Searching in ${path}

run=$1

NumEvtPerDst=10000
NumEvtPerJob=10000
NumJobPerDst=$((${NumEvtPerDst} / ${NumEvtPerJob}))

current_dir=${repo_dir}
outDir=fileLists

if [ -d "${outDir}/${run}" ]; then
  echo "Dir ${outDir}/${run} exists. Delete!"
  /usr/bin/rm -rf ${outDir}/${run}
fi
mkdir -p ${outDir}/${run}

result=$(get_closest_numbers ${run})
result_arr=($result)
numlower=${result_arr[0]}
numupper=${result_arr[1]}

nseg=100000000

subsystem='SUBSYSTEM'

if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
  subsystem='intt'
fi
if [[ "${runspecies}" == "run2pp" ]]; then
  subsystem='INTT'
fi
for ((server=0; server<8; server++))
do
  nsegtemp=0
  if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
    nsegtemp=`ls ${path}/DST_STREAMING_EVENT_${subsystem}${server}/run_000${numlower}_000${numupper}/*${run}* | wc -l`
  fi
  if [[ "${runspecies}" == "run2pp" ]]; then
    nsegtemp=`ls ${path}/DST_STREAMING_EVENT_${subsystem}${server}/run_000${numlower}_000${numupper}/dst/*${run}* | wc -l`
  fi
  echo run ${run} ${subsystem}${server} raw hit DST ${nsegtemp} segments
  if [ $nseg -gt $nsegtemp ]; then
    nseg=$nsegtemp
  fi
done

if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
  subsystem='mvtx'
fi
if [[ "${runspecies}" == "run2pp" ]]; then
  subsystem='MVTX'
fi
for ((felix=0; felix<6; felix++))
do
  nsegtemp=0
  if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
    nsegtemp=`ls ${path}/DST_STREAMING_EVENT_${subsystem}${felix}/run_000${numlower}_000${numupper}/*${run}* | wc -l`
  fi
  if [[ "${runspecies}" == "run2pp" ]]; then
    nsegtemp=`ls ${path}/DST_STREAMING_EVENT_${subsystem}${felix}/run_000${numlower}_000${numupper}/dst/*${run}* | wc -l`
  fi
  echo run ${run} ${subsystem}${felix} raw hit DST ${nsegtemp} segments
  if [ $nseg -gt $nsegtemp ]; then
    nseg=$nsegtemp
  fi
done

if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
  subsystem='ebdc'
  for ((i=0; i<24; i++))
  do
    ebdc=$(printf "%02d" $i)
    for ((ii=0; ii<2; ii++))
    do
      nsegtemp=`ls ${path}/DST_STREAMING_EVENT_${subsystem}${ebdc}_${ii}/run_000${numlower}_000${numupper}/*${run}* | wc -l`
      echo run ${run} ${subsystem}${ebdc}_${ii} raw hit DST ${nsegtemp} segments
      if [ $nseg -gt $nsegtemp ]; then
        nseg=$nsegtemp
      fi
    done
  done

  ebdc=39
  nsegtemp=`ls ${path}/DST_STREAMING_EVENT_${subsystem}${ebdc}/run_000${numlower}_000${numupper}/*${run}* | wc -l`
  echo run ${run} ${subsystem} raw hit DST ${nsegtemp} segments
  if [ $nseg -gt $nsegtemp ]; then
    nseg=$nsegtemp
  fi
fi

if [[ "${runspecies}" == "run2pp" ]]; then
  subsystem='TPC'
  for ((i=0; i<24; i++))
  do
    ebdc=$(printf "%02d" $i)
    nsegtemp=`ls ${path}/DST_STREAMING_EVENT_${subsystem}${ebdc}/run_000${numlower}_000${numupper}/dst/*${run}* | wc -l`
    echo run ${run} ${subsystem}${ebdc} raw hit DST ${nsegtemp} segments
    if [ $nseg -gt $nsegtemp ]; then
      nseg=$nsegtemp
    fi
  done

  subsystem='TPOT'
  nsegtemp=`ls ${path}/DST_STREAMING_EVENT_${subsystem}/run_000${numlower}_000${numupper}/dst/*${run}* | wc -l`
  echo run ${run} ${subsystem} raw hit DST ${nsegtemp} segments
  if [ $nseg -gt $nsegtemp ]; then
    nseg=$nsegtemp
  fi
fi

for ((j=0; j<${nseg}; j++))
do
  segment=$(printf "%05d" $j)
  out=${outDir}/${run}/rawhit_${segment}.list
  > ${out}

  if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
    subsystem='intt'
  fi
  if [[ "${runspecies}" == "run2pp" ]]; then
    subsystem='INTT'
  fi
  for ((server=0; server<8; server++))
  do
    if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
      #echo "${path}/DST_STREAMING_EVENT_${subsystem}${server}/run_000${numlower}_000${numupper}/DST_STREAMING_EVENT_${subsystem}${server}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
      echo "DST_STREAMING_EVENT_${subsystem}${server}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
    fi
    if [[ "${runspecies}" == "run2pp" ]]; then
      #echo "${path}/DST_STREAMING_EVENT_${subsystem}${server}/run_000${numlower}_000${numupper}/dst/DST_STREAMING_EVENT_${subsystem}${server}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
      echo "DST_STREAMING_EVENT_${subsystem}${server}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
    fi
  done

  if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
    subsystem='mvtx'
  fi
  if [[ "${runspecies}" == "run2pp" ]]; then
    subsystem='MVTX'
  fi
  for ((felix=0; felix<6; felix++))
  do
    if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
      #echo "${path}/DST_STREAMING_EVENT_${subsystem}${felix}/run_000${numlower}_000${numupper}/DST_STREAMING_EVENT_${subsystem}${felix}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
      echo "DST_STREAMING_EVENT_${subsystem}${felix}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
    fi
    if [[ "${runspecies}" == "run2pp" ]]; then
      #echo "${path}/DST_STREAMING_EVENT_${subsystem}${felix}/run_000${numlower}_000${numupper}/dst/DST_STREAMING_EVENT_${subsystem}${felix}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
      echo "DST_STREAMING_EVENT_${subsystem}${felix}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
    fi
  done

  if [[ "${runspecies}" == "run3auau" || "${runspecies}" == "run3pp" ]]; then
    subsystem='ebdc'
    for ((i=0; i<24; i++))
    do
      ebdc=$(printf "%02d" $i)
      for ((ii=0; ii<2; ii++))
      do
        #echo "${path}/DST_STREAMING_EVENT_${subsystem}${ebdc}_${ii}/run_000${numlower}_000${numupper}/DST_STREAMING_EVENT_${subsystem}${ebdc}_${ii}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
        echo "DST_STREAMING_EVENT_${subsystem}${ebdc}_${ii}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
      done
    done

    ebdc=39
    #echo "${path}/DST_STREAMING_EVENT_${subsystem}${ebdc}/run_000${numlower}_000${numupper}/DST_STREAMING_EVENT_${subsystem}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
    echo "DST_STREAMING_EVENT_${subsystem}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
  fi

  if [[ "${runspecies}" == "run2pp" ]]; then
    subsystem='TPC'
    for ((i=0; i<24; i++))
    do
      ebdc=$(printf "%02d" $i)
      #echo "${path}/DST_STREAMING_EVENT_${subsystem}${ebdc}/run_000${numlower}_000${numupper}/dst/DST_STREAMING_EVENT_${subsystem}${ebdc}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
      echo "DST_STREAMING_EVENT_${subsystem}${ebdc}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
    done

    subsystem='TPOT'
    #echo "${path}/DST_STREAMING_EVENT_${subsystem}/run_000${numlower}_000${numupper}/dst/DST_STREAMING_EVENT_${subsystem}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
    echo "DST_STREAMING_EVENT_${subsystem}_${runspecies}_${anabuild}_${cdbtag}_${version}-000${run}-${segment}.root" >> ${out}
  fi
done

out=${outDir}/file_run${run}_0.list
> ${out}

nseg=`ls ${outDir}/${run}/rawhit_*.list | wc -l`
echo run ${run} : ${nseg} dsts, $((${nseg} * ${NumJobPerDst})) jobs

for ((i=0; i<${nseg}; i++))
do
  segment=$(printf "%05d" $i)
  for ((j=0; j<${NumJobPerDst}; j++))
  do
    nSkip=$(( ${j} * ${NumEvtPerJob} ))
    echo "${NumEvtPerJob} ${current_dir}/${outDir}/${run}/rawhit_${segment}.list ${nSkip}" >> ${out}
  done
done

echo "Done"
