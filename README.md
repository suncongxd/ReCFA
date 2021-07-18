# ReCFA
Resillent Control-Flow Attestation

## Requirement

- ReCFA has been tested on Ubuntu-18.04.5 LTS 64-bit
- Tool dependency
  - gcc 7.5.0
  - llvm 10.0.0
  - Dyninst 10.1.0

## Deployment

- Install Dyninst 10.1.0 and configure the PATH environment.
- (Optional) compile the preCFG used by the call-site filtering:
  ```
  cd src/preCFG
  make
  make install
  ```
- Generate the .dot and .asm files used by the call-site filtering of ReCFA, then perform the call-site filtering:
  ```
  ./prepare_csfiltering.sh gcc
  ./prepare_csfiltering.sh llvm
  ```

## Directory structure

- spec_gcc, spec_llvm: the working directories of ReCFA's benchmark evaluations.
- bin: executables of ReCFA.
- src: related source code of ReCFA.

