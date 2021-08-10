#! /bin/bash

#specify the compiler of the binaries.
CP=$1

OL=O0 # O1 O2 O3

recfa_root=$(pwd)
#echo $recfa_root

event_root=$recfa_root"/spec_"$CP/$OL

folding_path=$recfa_root"/bin/folding"

declare -A test_case_option=(
["perlbench"]="l"
["bzip2"]="r"
["gcc"]="l"
["mcf"]="l"
["milc"]="r"
["gobmk"]="l"
["hmmer"]="r"
["libquantum"]="r"
["h264ref"]="l"
["lbm"]="r"
["sphinx_livepretend"]="l"
)


#case_names=( bzip2 mcf milc hmmer libquantum h264ref sphinx_livepretend gobmk lbm) #   perlbench    gcc  
#success:

for name in ${!test_case_option[@]}; do
  file_path=$event_root/$name"_base."$CP"_"$OL"_instru-re"
  echo $name
  if [[ -f $file_path ]]; then
     if [ ${test_case_option[$name]} = "r" ]; then
        $folding_path -r $file_path
     elif [ ${test_case_option[$name]} = "l" ]; then
        $folding_path $file_path
     fi
  fi
done


#$folding_path $event_root/h264ref_base.gcc_O0_instru-re
