[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![MIT][license-shield]][license-url]

<!-- PROJECT LOGO -->
<div align="center">

<h3 align="center">STT-HE (sttserv)</h3>

  <p align="center">
    Low-latency Speech-To-Text server in C++, for local and embedded use
  </p>

</div>

<!-- ABOUT THE PROJECT -->
## About The Project

`sttserv` is a low-latency Speech-To-Text (ASR) library and server written in C++. It wraps existing
ML inference engines behind one backend interface, so the same capture and transcription code runs on
top of different model runtimes.

Backends available today:

* **whisper** — the [whisper.cpp](https://github.com/ggml-org/whisper.cpp) / ggml stack. This backend
  serves two model families through one runtime: classic Whisper models, and NVIDIA
  **Parakeet TDT** models converted to ggml. Runs on Vulkan, CUDA, Metal, or CPU depending on how ggml
  is configured.
* **sherpa-onnx** — the [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) runtime (ONNX Runtime).
  Off by default; enable it for the NeMo / sherpa model targets.

The original goal was Hebrew speech-to-text, but nothing in the library is language-specific — the
language is a property of the model you load. The design target is to make the capture → transcription
path as fast as feasible for local and embedded deployment.

<br></br>

### Project Structure

```
sttserv/          the library + server target (public headers in sttserv/include/sttserv/)
test/             GoogleTest accuracy/functionality harness (asr_test.cpp)
benchmark/        GoogleBenchmark performance targets
sandbox/          Python scratch space for prototyping / measurement
cmake/            workspace CMake modules (CPM, output dirs, diagnostics, ...)
dependencies/     vendored submodules fetched via CPM (whispercpp, sherpa-onnx, util2, ...)
build.sh / build.ps1    convenience build wrappers (Linux / Windows)
bench.sh                ASR model accuracy+latency sweep (see Benchmarks below)
```

Dependencies are pulled through CPM (`safe_cpm_add_package`) from `dependencies/`. Model files and
recordings live under `dependencies/models/` and `dependencies/recordings/` and are git-ignored.

<br></br>

<!-- GETTING STARTED -->
## Getting Started

This project uses the same CMake workspace layout as the wider drone/perception project it was borrowed
from — CPM for dependencies, `build.sh` / `build.ps1` wrappers, out-of-source builds under `build/`.
If you have built that project, this will feel identical.

### Prerequisites

* CMake ≥ 3.16
* A working C/C++ toolchain (clang preferred; gcc works)
* Python 3 (only for the `sandbox/` prototyping scripts)
* Ninja (the build wrappers generate Ninja)
* Model files under `dependencies/models/` — ggml `.bin` for the whisper/parakeet backend, ONNX for
  sherpa

### Building

Use the wrapper. Its signature is `build.sh <build_type> <library_type> <action>`:

```sh
./build.sh --help
./build.sh release static configure    # configure once (fetches submodules, runs CMake)
./build.sh release static build        # compile
```

* `build_type`   — `debug`, `release`, `release_dbginfo`, `debug_perf`, `release_perf`
* `library_type` — `shared` (`.so`/`.dll`) or `static` (`.a`/`.lib`)
* `action`       — `configure`, `build`, `cleanbuild`, `sandbox`, `debugsandbox`

Windows is the same via PowerShell:

```powershell
.\build.ps1 -BuildType release -LinkType shared -Action configure
.\build.ps1 -BuildType release -LinkType shared -Action build
```

Build output lands in `build/<build_type>/<library_type>/`.

#### Choosing backends

Backends and the top-level targets are CMake options (see `CMakeLists.txt`). At least one backend is
required — configuring with none is a fatal error. The defaults baked into `build.sh` are:

| Option | Default | Meaning |
|---|---|---|
| `STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER` | `ON` | whisper.cpp / ggml backend (Whisper + Parakeet) |
| `STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX` | `OFF` | sherpa-onnx (ONNX Runtime) backend |
| `STTSERVER_BUILD_LIBRARY` | `ON` | build the actual server library |
| `STTSERVER_BUILD_TESTS` | `ON` (top-level) | GoogleTest harness |
| `STTSERVER_BUILD_BENCHMARKS` | `ON` (top-level) | GoogleBenchmark targets |

ggml compute backend (Vulkan / CUDA / Metal / CPU) is selected with the usual `-DGGML_*` flags passed
through at configure time; see the commented block in `build.sh` for a worked CUDA + Vulkan example.

<br></br>

<!-- USAGE EXAMPLES -->
## Usage

### As a library (in-source / submodule)

In your `CMakeLists.txt`:

```cmake
add_subdirectory(path/to/sttserv)
target_link_libraries(your_target PRIVATE STTSERVER::SpeechToTextServer)
```

Public headers are under `sttserv/include/sttserv/` — `backend.hpp` (the transcription backend),
`audio2.hpp` (capture / WAV), `async_key.hpp` (push-to-talk key hook), `cmdline.hpp`, and the exported
C API in `sttserver_api.h`. The C API export macros (`STTSERVER_EXPORTS` / `STTSERVER_STATIC_DEFINE`)
are set for you by the library target depending on shared vs static.

As a submodule the workspace-level tests and benchmarks auto-disable, so you only pull in the library.

### Running the transcription tests

The accuracy harness (`test/asr_test.cpp`) builds one `run_<target>` per model. From the build dir:

```sh
cd build/release/static
ninja -t targets | grep run       # list the model targets
ninja run_parakeet-v3-q4-whisper-backend
```

The harness reads clips from `dependencies/recordings/`, transcribes each against a model, and verifies
the output against `sentences.txt`. See `test/run_tests.txt` for the current target list.

<br></br>

## Roadmap / TODO

* Aggregated test runner (currently each model target runs individually)
* Automatic testing matrix across architecture / OS / build type
* SNR-gated or confidence-gated preprocessing (see the denoise finding in Benchmarks)

<br></br>

<!-- CONTRIBUTING -->
## Contributing

If you have a suggestion, fork the repo and open a pull request, or open an issue tagged "enhancement".

<!-- LICENSE -->
## License

Distributed under the MIT License. See the `LICENSE` file for more information.

<br></br>

<!-- Benchmarks -->
## Benchmarks

### ASR model sweep — 2026-08-11

Which ASR model to ship for a real-time voice loop. Reproduce with `./bench.sh` from the workspace
root (build the test targets first). Setup and full results follow.

**Setup.** Local box, GTX 1050 Ti (4 GB), whisper backend on Vulkan, flash-attention (`--fa`), GPU
`gid 0`. The `accuracy_test` harness over **44 clips** = 5 sentences × ~9 noise variations
(quiet / mid / noisy), four speakers (anonymized). Ground truth is `sentences.txt`. "Pass" = a
clip transcribes above the harness accuracy threshold. The three sherpa-onnx NeMo targets were not
built for this run (sherpa backend off) and are excluded.

| Model | Backend | Size | Total (44) | Per-clip ms (min / med / max) | Pass | Acc p50 | Acc min |
|---|---|---|---|---|---|---|---|
| **parakeet-v3 q4_k** | whisper-parakeet | **397 MB** | **27 s** | 157 / **586** / 1460 | **37/44** | 95.1% | 37.8% |
| parakeet-v3 q8_0 | whisper-parakeet | 638 MB | 25 s | 139 / 522 / 915 | 36/44 | 95.1% | 36.2% |
| parakeet-v3 f16 | whisper-parakeet | 1.2 GB | 25 s | 146 / 510 / 900 | 36/44 | 95.1% | 36.2% |
| parakeet-v3 f32 | whisper-parakeet | 2.4 GB | 27 s | 142 / 515 / 916 | 36/44 | 95.1% | 36.2% |
| distil-whisper-large-v3.5 q5k | whisper-whisper | 513 MB | 329 s | 5850 / 6698 / 10389 | 33/44 | 95.1% | 39.5% |
| whisper-large-v3-turbo q4_k | whisper-whisper | 453 MB | 313 s | 5737 / 6650 / 9846 | 33/44 | 94.0% | 63.0% |

**Winner: Parakeet TDT 0.6B v3, q4_k.** Fastest tier (median 0.59 s/clip), smallest footprint
(397 MB — the most headroom when it shares a 4 GB GPU with a VLM), and the best pass rate (37/44).
Accuracy is flat across the parakeet quant ladder (p50 95.1% identical f32 → q4), so quantization is
free here; pick by size. The two whisper-large models are **~12× slower** for no accuracy gain — they
only look more robust on min-accuracy because they drop fewer worst-case clips, but their latency
disqualifies them for a real-time loop.

### Denoise (GTCRN speech-enhancement) — does it help before Parakeet?

The 7 failing clips per parakeet run are the loud/noisy variations. The obvious idea is a denoise stage
in front of the ASR. It was measured, not assumed.

**Cost** (sherpa-onnx GTCRN, `gtcrn_simple.onnx`, 535 KB, CPU ONNX Runtime, RTF = proc / audio):

| threads | proc med (ms) | RTF med |
|---|---|---|
| 1 | 507 | 0.075 |
| 2 | 632 | 0.093 |
| 4 | 692 | 0.101 |

Cost is negligible — threads=1 is fastest (the model is too small to parallelize), ~0.5 s to denoise a
~6.75 s utterance, 13× faster than realtime on one core.

**Accuracy** (parakeet-q4_k on raw vs GTCRN-denoised, same 44 clips):

| set | Pass | Acc p50 | Acc min |
|---|---|---|---|
| raw | 37/44 | 95.1% | 37.8% |
| GTCRN denoised | 27/44 | 92.7% | 28.7% |

**Denoise is net-negative.** It drops 10 previously-good clips and lowers both p50 and min; Sentence[0]
p50 collapsed 87% → 38%. Whole-utterance speech enhancement shifts the audio distribution away from
what Parakeet expects and over-processes the quiet/mid clips, while the genuinely loud fails are **not**
rescued — GTCRN cannot recover speech that noise has already drowned.

**Verdict: ship parakeet-q4 on raw audio; do not blind-denoise.** Cost was never the problem; accuracy
is. If preprocessing is revisited, gate on downstream ASR confidence (ask the speaker to repeat when
confidence is low) rather than SNR, and evaluate a stronger enhancement model — as measured, GTCRN in
front of the ASR hurts.

### Footprint note

Footprint matters because the ASR shares a 4 GB GPU with a VLM. Parakeet-q4 at 397 MB leaves the most
KV headroom; q8_0 (638 MB) is the fallback if q4 ever misbehaves on field audio. The per-model logs
from `bench.sh` hold transcript text and speaker labels, so they are git-ignored and never committed.

<!-- ACKNOWLEDGEMENTS -->
## Acknowledgements

* [whisper.cpp](https://github.com/ggml-org/whisper.cpp) / [ggml](https://github.com/ggml-org/ggml)
* [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx)
* [GoogleTest](https://google.github.io/googletest) / [GoogleBenchmark](https://github.com/google/benchmark)
* [CMake](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)
* [Best-README-Template](https://github.com/othneildrew/Best-README-Template)

<!-- References -->
## References

* [Modern CMake](https://cliutils.gitlab.io/modern-cmake/README.html)
* [GoogleBenchmark User Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
* [Typed Tests (GoogleTest)](https://google.github.io/googletest/advanced.html#typed-tests)

<!-- MARKDOWN LINKS & IMAGES -->
[contributors-shield]: https://img.shields.io/github/contributors/inonitz/sttserv?style=for-the-badge&color=blue
[contributors-url]: https://github.com/inonitz/sttserv/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/inonitz/sttserv?style=for-the-badge&color=blue
[forks-url]: https://github.com/inonitz/sttserv/network/members
[stars-shield]: https://img.shields.io/github/stars/inonitz/sttserv?style=for-the-badge&color=blue
[stars-url]: https://github.com/inonitz/sttserv/stargazers
[license-shield]: https://img.shields.io/github/license/inonitz/sttserv?style=for-the-badge
[license-url]: https://github.com/inonitz/sttserv/blob/main/LICENSE
