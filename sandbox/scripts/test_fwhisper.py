import os
import signal
import sys
import time
import numpy as np
import sounddevice as sd
import soundfile as sf
from pynput import keyboard
import sherpa_onnx
from faster_whisper import WhisperModel
import logging


# Configure logging format and level
logging.basicConfig(
    level=logging.DEBUG, # Set to logging.INFO for less verbose engine steps
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    datefmt="%H:%M:%S"
)

# Optional: Ensure the faster_whisper logger explicitly inherits and outputs DEBUG logs
logging.getLogger("faster_whisper").setLevel(logging.DEBUG)


# --- CONFIGURATION ---
HOTKEY = 'a'
EXITKEY = 'q'
SAVE_AUDIO = False

# Construct an absolute path relative to exactly where this python file is located
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# MODEL_NAME = os.path.join(SCRIPT_DIR, "../../dependencies", "models", "faster-distil-whisper-large-v3.5-int8")
MODEL_NAME = os.path.join(SCRIPT_DIR, 
    "../../dependencies", 
    "models",
    "sherpa-onnx",
    "sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8/"
)


DEVICE = "cuda"                                    # 🛠️ FIXED: "gpu" changed to "cuda"
COMPUTE_TYPE = "auto"                      
FILENAME = "recorded_audio.wav"
SAMPLE_RATE = 16000

# --- STATE MANAGEMENT ---
is_recording = False
audio_buffers = []
stream = None

def audio_callback(indata, frames, time, status):
    if is_recording:
        audio_buffers.append(indata.copy())

def toggle_recording():
    global is_recording, audio_buffers, stream
    
    if not is_recording:
        is_recording = True
        audio_buffers = []
        stream = sd.InputStream(samplerate=SAMPLE_RATE, channels=1, callback=audio_callback)
        stream.start()
        print("\n[🔴] Recording started... Press 'a' again to stop.")
    else:
        is_recording = False
        if stream:
            stream.stop()
            stream.close()
        print("[⬜] Recording stopped. Processing audio...")
        
        audio_data = np.concatenate(audio_buffers, axis=0)
        sf.write(FILENAME, audio_data, SAMPLE_RATE)
        
        run_transcription_nemo()

def run_transcription_whisper():
    # 🛠️ FIXED: Catch path errors before they reach faster-whisper
    if not os.path.isdir(MODEL_NAME):
        print(f"\n❌ FATAL ERROR: Model directory not found!")
        print(f"Python looked exactly here: {MODEL_NAME}")
        print("Please fix the path. Exiting...\n")
        return

    print("🤖 Loading model and transcribing...")
    try:
        # 🛠️ FIXED: Removed local_files_only=True
        model = WhisperModel(
            MODEL_NAME, 
            device=DEVICE, 
            compute_type=COMPUTE_TYPE
        )
        begin_time = time.perf_counter_ns()
        segments, _ = model.transcribe(FILENAME, beam_size=1, language="en")
        segments = list(segments) 
        end_time = time.perf_counter_ns()

        print("\n--- TRANSCRIPT (Took {} ms) ---".format( (end_time - begin_time) * 1e-6))
        for segment in segments:
            print(segment.text)
        print("------------------\n")
        
    except Exception as e:
        print(f"Error during transcription: {e}")
        
    finally:
        if not SAVE_AUDIO and os.path.exists(FILENAME):
            os.remove(FILENAME)
            print("🧹 Temporary audio file deleted.")


def run_transcription_nemo():
    # Ensure these paths point to your extracted .onnx files
    encoder = MODEL_NAME + "encoder.int8.onnx"
    decoder = MODEL_NAME + "decoder.int8.onnx"
    joiner = MODEL_NAME + "joiner.int8.onnx"
    tokens = MODEL_NAME + "tokens.txt"
    # Parakeet TDT requires the 3-component transducer layout
    try:
        recognizer = sherpa_onnx.OfflineRecognizer.from_transducer(
            encoder,
            decoder,
            joiner,
            tokens,
            num_threads=8,
            provider="cpu",
            debug=True,
            decoding_method="greedy_search",
            model_type="nemo_transducer"
        )
    except Exception as e:
        print(f"Error during Model Loading: {e}")

    

    print("⚡ Transcribing with Parakeet TDT...")
    # Load audio
    audio_data, sample_rate = sf.read(FILENAME, dtype="float32")
    begin_time = time.perf_counter_ns()
    print("Stage 0")
    stream = recognizer.create_stream()
    print("Stage 1")
    stream.accept_waveform(sample_rate, audio_data)
    print("Stage 2")
    recognizer.decode_stream(stream)
    print("Stage 3")
    end_time = time.perf_counter_ns()
    
    print("\n--- TRANSCRIPT (Took {} ms) ---".format( (end_time - begin_time) * 1e-6))
    print(f"--- TRANSCRIPT ---\n{stream.result.text}\n------------------")
    
    if not SAVE_AUDIO and os.path.exists(FILENAME):
        os.remove(FILENAME)



def on_press(key):
    if hasattr(key, 'char') and key.char == EXITKEY:
        print("\n👋 Exiting cleanly...")
        if stream:
            stream.stop()
            stream.close(True) # 🛠️ FIXED: Removed "True" argument
            
        if not SAVE_AUDIO and os.path.exists(FILENAME):
            try: os.remove(FILENAME)
            except: pass
            
        sys.exit(0)

    try:
        if hasattr(key, 'char') and key.char == HOTKEY:
            toggle_recording()
    except Exception:
        pass


def signal_handler(sig, frame):
    # Route Ctrl+C directly to a clean exit
    print("\n👋 Exiting cleanly...")
    if stream:
        try:
            stream.stop()
            stream.close()
        except: pass
    sys.exit(0)

# Start Key Listener Loop
if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    print(f"Listening for key: '{HOTKEY}' to record, '{EXITKEY}' to exit.")
    with keyboard.Listener(on_press=on_press) as listener:
        listener.join()