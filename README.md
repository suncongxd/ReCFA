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
- (Optional) Compile the call-site filter:
  ```
  cd src/csfilter
  make
  make install
  ```
- Generate the .dot and .asm files used by the call-site filtering of ReCFA, then perform the call-site filtering:
  ```
  ./prepare_csfiltering.sh gcc
  ./prepare_csfiltering.sh llvm
  ```
  For example, for the binary `bzip2_base.gcc_O0`, the `preCFG` will generate an `.dot` file, and the script `prepare_csfiltering.sh` will then generate a `.filtered` file which stores the skipped direct call addresses.
- (Optional) Compile the mutator of binary for the static instrumentation with Dyninst.
  ```
  cd src/mutator
  make
  make install
  ```
- Use the mutator to instrument binaries.
  ```
  ./instrument.sh gcc
  ./instrument.sh llvm
  ```
- Move the instrumented prover programs into SPEC2k6 environment (in our repository ReCFA-dev) to run with the stardard workloads. The runtime control-flow events will be stored in the `spec_gcc` or `spec_llvm` directory. For example, for the instrumented prover binary `spec_gcc/O0/bzip2_base.gcc_O0_instru`, the runtime events will be stored in `spec_gcc/O0/bzip2_base.gcc_O0_instru-re`.
- (Optional) Build the folding and greedy-compression program. 
  
## Directory structure

- spec_gcc, spec_llvm: the working directories of ReCFA's benchmark evaluations.
- bin: executables of ReCFA.
- src: source code of the main modules of ReCFA.

