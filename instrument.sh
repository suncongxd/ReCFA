#! /bin/bash

#specify the compiler of the binaries.
CP=$1

recfa_root=$(pwd)
#echo $recfa_root

binary_root=$recfa_root"/spec_"$CP
#echo $binary_root

mutator_path=$recfa_root"/bin/mutator"
#echo $mutator_path



OL=O0 # O1 O2 O3

#option "-r" means the instrumented binary will be considerd to fold both the loops and recursions of its execution
#option "-l" means the instrumented binary will only be considerd to fold the loops of its execution
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

#["sjeng"]="r" : instrumentation failed.


for case_name in ${!test_case_option[*]}; do
   bin_path=$binary_root/$OL/$case_name"_base."$CP"_"$OL
   filtered_path=$bin_path".filtered"
   mutatee_out_path=$bin_path"_instru"

   if [ -f $bin_path ] && [ -f $filtered_path ]; then
      #echo "aaa"
      option=${test_case_option[$case_name]}
      if [ $option = "r" ]; then
         $mutator_path -r $bin_path $mutatee_out_path $filtered_path
      elif [ $option = "l" ]; then # option = -l. Only consider loop folding and ignore recursion folding.
         $mutator_path $bin_path $mutatee_out_path $filtered_path
      fi
   fi
done
