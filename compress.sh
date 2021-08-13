#! /bin/bash

#specify the compiler of the binaries.
CP=$1

OL=O0 # O1 O2 O3

recfa_root=$(pwd)
#echo $recfa_root

event_root=$recfa_root"/spec_"$CP/$OL

folding_path=$recfa_root"/bin/folding"
greedy_path=$recfa_root"/bin/greedy"

declare -A test_case_option=(
["bzip2"]="r"
["mcf"]="l"
["milc"]="r"
["gobmk"]="l"
["hmmer"]="r"
["libquantum"]="r"
["lbm"]="r"
["sphinx_livepretend"]="l"
["h264ref"]="l"
["perlbench"]="l"
["gcc"]="l"

)


for name in ${!test_case_option[@]}; do
  file_path=$event_root/$name"_base."$CP"_"$OL"_instru-re"
  echo $name
  if [[ -f $file_path ]]; then
     if [ ${test_case_option[$name]} = "r" ]; then
        $folding_path -r $file_path
     elif [ ${test_case_option[$name]} = "l" ]; then
        $folding_path $file_path 
     fi
     sleep 1s
     folded_path=$file_path"_folded"
     if [ -f $folded_path ]; then
        $greedy_path $folded_path
        sleep 1s
        zstd $folded_path"_gr"
     fi
  fi
done


