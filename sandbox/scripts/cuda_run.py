import onnx_asr
import time
import onnxruntime as ort


audio_file="../../dependencies/models/sherpa-onnx/sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-fp16/test_wavs/0.wav"

# Correct argument: `providers` as a list
model = onnx_asr.load_model(
    "nemo-parakeet-tdt-0.6b-v3", 
    quantization="int8",
    providers=["WebGpuExecutionProvider"]
)

# WARMUP (Compiles shaders / avoids deferral)
print("Available Providers:", ort.get_available_providers())

print("Warming up...")
_ = model.recognize(audio_file) 

# BENCHMARK
print("Measuring...")
start = time.perf_counter_ns()

result = model.recognize(audio_file)

end = time.perf_counter_ns()
total_ms = (end - start) / 1_000_000

print(f"\nTime: {total_ms:.2f} ms")
print(f"Result: {result}")