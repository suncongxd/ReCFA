# ReCFA
Resillent Control-Flow Attestation

## Requirement

- ReCFA has been tested on Ubuntu-18.04.5 LTS 64-bit
- Tool dependency
  - gcc 7.5.0
  - llvm 10.0.0
  - Dyninst 10.1.0
  - zstandard 1.5.0
  - typearmor (latest) with Dyninst 9.3.1

## Deployment

- Install Dyninst 10.1.0 and configure the PATH environment.
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
  For example, for the binary `bzip2_base.gcc_O0`, the `preCFG` will generate an `.dot` file. The script `prepare_csfiltering.sh` will invoke `csfilter.jar` to generate: 1) a `.filtered` file which stores the skipped direct call addresses, 2) a `.filtered.map` file which stores the mapping `M` of the CFI policy, that maps from the predecessor's target address to the skipped call sites.
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
- Move the instrumented prover programs into SPEC2k6 environment (in our repository ReCFA-dev) to run with the standard workloads. The runtime control-flow events will be stored in the `spec_gcc` or `spec_llvm` directory. For example, for the instrumented prover binary `spec_gcc/O0/bzip2_base.gcc_O0_instru`, the runtime events will be stored in `spec_gcc/O0/bzip2_base.gcc_O0_instru-re`.
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
  - Generate the CFI policy mapping `F` with the patched typearmor. Following the instructions of the repository `ReCFA-dev` to patch and run typearmor. In the directory `policy/F/` there are the `binfo.*` for the SPEC2k6 binaries evaluated in our paper.

- Run the verifier. (Ensure the policy files are well deployed in `policy/`)
  ```
  ./verify.sh gcc
  ./verify.sh llvm
  ```

## Directory structure

- spec_gcc, spec_llvm: the working directories of ReCFA's benchmark evaluations.
- bin: executables of ReCFA.
- src: source code of the main modules of ReCFA.
  - preCFG: using dyninst to generate the `.dot` file (indeed a dcfg + i-jumps) used by the call-site filtering and the verification. 
  - csfilter: generating the skipped direct call sites.
  - mutator: the program taking the original binary as input and statically instrumenting the binary into a new binary running as the prover.
  - folding: the control-flow events condensing programs, including the events folding program and the greedy compression program.
  - verifier: the verifier program taking the policy and attesting the control-flow integrity of provers.

