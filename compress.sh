#! /bin/bash

#specify the compiler of the binaries.
CP=$1

OL=O0 # O1 O2 O3

recfa_root=$(pwd)
#echo $recfa_root

event_root=$recfa_root"/spec_"$CP/$OL

folding_path=$recfa_root"/bin/folding"

case_names=(perlbench ) #       gcc  
#success:bzip2 mcf milc hmmer libquantum h264ref sphinx_livepretend gobmk lbm

for name in ${case_names[@]}; do
  file_path=$event_root/$name"_base."$CP"_"$OL"_instru-re"
  if [[ -f $file_path ]]; then
    $folding_path $file_path
  fi
done


#$folding_path $event_root/h264ref_base.gcc_O0_instru-re
