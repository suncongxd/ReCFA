#! /bin/bash

#specify the compiler of the binaries.
CP=$1

OL=O0 # O1 O2 O3

recfa_root=$(pwd)
#echo $recfa_root

event_root=$recfa_root"/spec_"$CP/$OL

verifier_path=$recfa_root"/bin/check"
#echo $verifier_path

case_names=(gcc perlbench) # bzip2 milc gobmk hmmer libquantum h264ref lbm sphinx_livepretend  mcf


for name in ${case_names[@]}; do
  echo "Attestation result of $name:"

  disassem_file_path=$event_root/$name"_base."$CP"_"$OL".asm"
  #echo $disassem_file_path

  dyninst_graph_path=$event_root/$name"_base."$CP"_"$OL".dot"
  #echo $dyninst_graph_path

  policy_F_path=$recfa_root"/policy/F/binfo."$name"_base."$CP"_"$OL
  #echo $policy_F_path
  
  policy_M_path=$recfa_root"/policy/M/"$name"_base."$CP"_"$OL".filtered.map"
  #echo $policy_M_path

  folded_runtime_event_path=$event_root/$name"_base."$CP"_"$OL"_instru-re_folded"
  #echo $folded_runtime_event_path

  num_executions=1

  $verifier_path $disassem_file_path $dyninst_graph_path $policy_F_path $policy_M_path $folded_runtime_event_path $num_executions $CP

  echo -e "\n"
done


