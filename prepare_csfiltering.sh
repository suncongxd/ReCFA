#! /bin/bash

#specify the compiler of the binaries.
CP=$1

recfa_root=$(pwd)
#echo $recfa_root

binary_root=$recfa_root"/spec_"$CP
#echo $binary_root

bin_csfilter=$recfa_root"/bin/csfilter.jar"

for OL in O0; do # O1 O2 O3
  if [ -d $binary_root"/"$OL ]; then
     for file in $(ls $binary_root"/"$OL); do
        if [[ $file == *$CP"_"$OL ]]; then #-f $file && 
           echo $file
           binary_path=$binary_root"/"$OL"/"$file
           dot_path=$binary_path".dot"
           asm_path=$binary_path".asm"
           filtered_output_path=$binary_path".filtered"

           if [ -f $dot_path ]; then
              rm $dot_path
           fi
           $recfa_root"/bin/preCFG" $binary_path > $dot_path

           if [ -f $asm_path ]; then
              rm $asm_path
              #sleep 2s
           fi
           objdump -d $binary_path > $asm_path
           
           if [[ ! -f $dot_path || ! -f $asm_path ]]; then
              echo ".dot or .asm file does not exist..."
              continue
           fi
           
           echo "> $bin_csfilter $dot_path $asm_path $filtered_output_path"
           java -jar $bin_csfilter $dot_path $asm_path $filtered_output_path
           echo -e "\n\n"
        fi
     done
  fi
done
