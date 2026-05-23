import os
import psutil

# Force ORT to try initializing the session
import onnxruntime as ort

try:
    # Creating a dummy session to force-trigger the EP init logic
    sess = ort.InferenceSession(None, providers=['CUDAExecutionProvider'])
except Exception:
    pass

# Print out exactly what shared libraries are hooked into the memory space
p = psutil.Process(os.getpid())
for lib in p.memory_maps():
    if "cuda" in lib.path.lower() or "cudnn" in lib.path.lower():
        print(lib.path)