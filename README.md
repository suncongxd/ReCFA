# ReCFA
Resillent Control-Flow Attestation

## Requirement

- ReCFA has been tested on Ubuntu-18.04.5 LTS 64-bit
- Tool dependency (**Please deploy the tools on the host Ubuntu 18.04 running ReCFA, except typearmor.**)
  - gcc 7.5.0
  - llvm 10.0.0
  - Dyninst 10.1.0
  - zstandard 1.5.0
  - typearmor (latest) with Dyninst 9.3.1. (**Please follow the instructions of the repository `ReCFA-dev` to deploy typearmor in a virtualbox guest Ubuntu 16.04 64bit.**)

## Deployment

- Install Dyninst 10.1.0 and configure the PATH environment. (**Please follow the instructions of the repository `ReCFA-dev`.**)
- Install zstandard.
- (Optional) Build the preCFG used by the call-site filtering:
  ```
  cd src/preCFG
  make
  make install
  ```
- (Optional) Build the call-site filter:
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
  (**The outputs of this step will be found in `spec_gcc/O0` and `spec_llvm/O0`. For each binary, e.g. `bzip2_base.gcc_O0`, this step will generate an `.dot` file, a `.filtered` file, and a `.filtered.map` file**)

- (Optional) Build the mutator of binary for the static instrumentation with Dyninst.
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
  (**The outputs of this step will be found in `spec_gcc/O0` and `spec_llvm/O0`. For each binary, e.g. `bzip2_base.gcc_O0`, this step will generate a new instrumented binary `bzip2_base.gcc_O0_instru`**)

- **The next step should be running the instrumented binary with the standard workload of SPEC CPU 2k6 benchmark to generate the control-flow events. Because we cannot release SPEC CPU 2k6, we assume this step is done for the artifact evaluation. Please download the following zip files for the control-flow events.**

  - https://drive.google.com/file/d/10WiR7L3w_sRVK1JG6Tu8OKVNexhmwhB6/view?usp=sharing
  - https://drive.google.com/file/d/1aoc1BppBAKIRDSAT0wsxq9WbkZ_rz_jS/view?usp=sharing

  Put `re-gcc.zip` into directory `spec_gcc/O0` and unzip it. We got the control-flow events files. For example, for the instrumented binary `spec_gcc/O0/bzip2_base.gcc_O0_instru`, the corresponding runtime events extracted from `re-gcc.zip` is the file `spec_gcc/O0/bzip2_base.gcc_O0_instru-re`.

- (Optional) Build the folding and greedy-compression program.
  ```
  cd src/folding
  ./build.sh
  ```
- Prover-side control-flow event folding and greedy compression.
  ```
  ./compress.sh gcc
  ./compress.sh llvm
  ```
  The procedure will output the folded runtime control-flow events (e.g. in `bzip2_base.gcc_O0_instru-re_folded`), the greedy compression result (e.g. in `bzip2_base.gcc_O0_instru-re_folded_gr`), and the zstandard compression result (e.g. in `bzip2_base.gcc_O0_instru-re_folded_gr.zst`).

- Prepare the verifier.
  - (Optional) Build the verifier program.
  ```
  cd src/verifier
  ./build.sh
  ```

- Generate CFI policy mapping `F` with the patched typearmor. **Please follow the instructions of `ReCFA-dev` to deploy and patch typearmor. (The artifact reviewers can safely skip this step. In the directory `policy/F/` there are the `binfo.*` for the SPEC2k6 binaries evaluated in our paper. The policy files are there.)**
  - Put the original (un-instrumented) binaries of SPEC2k6 in `typearmor/server-bins`. Then,
  ```
  cd typearmor/server-bins
  ../run-ta-static.sh ./bzip2_base.gcc_O0
  ```
  - The policy file will be generated into `typearmor/out/`, e.g. `typearmor/out/binfo.bzip2_base.gcc_O0`.
  - Move all the policy files into the repository `ReCFA` for use by the verifier.

- Run the verifier. (Ensure the policy files are well deployed in `policy/`)
  ```
  ./verify.sh gcc
  ./verify.sh llvm
  ```
  The attestation results are reported by the verifier at the console.

## Directory structure

- spec_gcc, spec_llvm: the working directories of ReCFA's benchmark evaluations.
- bin: executables of ReCFA.
- src: source code of the main modules of ReCFA.
  - preCFG: using dyninst to generate the `.dot` file (indeed a dcfg + i-jumps) used by the call-site filtering and the verification. 
  - csfilter: generating the skipped direct call sites.
  - mutator: the program taking the original binary as input and statically instrumenting the binary into a new binary running as the prover.
  - folding: the control-flow events condensing programs, including the events folding program and the greedy compression program.
  - verifier: the verifier program taking the policy and attesting the control-flow integrity of provers.
- lib: the share object used by the control-flow folding and greedy compression.
- policy: the CFI policy files.

