import argparse
import os
import logging
import multiprocessing
import numpy as np
import keyboard as kb
import sounddevice as sd
from tqdm import tqdm
from sounddevice import InputStream
from faster_whisper import WhisperModel
# from faster_whisper import available_models
# from transformers import WhisperProcessor, WhisperForConditionalGeneration


# Note about pathlib:
# https://switowski.com/blog/pathlib/
# mkdir -p in python:
# https://stackoverflow.com/a/273227


def print_sound(indata, outdata, frames, time, status):
    volume_norm = np.linalg.norm(indata)*10
    print ("|" * int(volume_norm))


def listen_to_audio(stop=False):
    longTime = 1000000000
    with sd.Stream(callback=print_sound, ):
        sd.sleep(longTime)


def record_audio(sampleRate, duration):
    audio = []
    return audio


def load_model(model_path):
    return


def transcribe_audio(audio_buf, loaded_model):
    return ''


if __name__ == "__main__":
    os.environ["HF_HUB_OFFLINE"] = "1" # Make sure no download is happening
    logging.basicConfig()
    logging.getLogger("faster_whisper").setLevel(logging.DEBUG)

    audio_sampleRate  = 16000 
    audio_durationSec = 5
    threads_compute = int(multiprocessing.cpu_count() * 3 / 2)

    # Had the best Success so far with this model - 
    # Takes a couple of seconds on a 16 thread machine with good accuracy
    model_path = "./local_models/ivrit_ai_whisper_turbo"
    model = WhisperModel(model_path, 
        device="auto", 
        cpu_threads=threads_compute,
        num_workers=threads_compute,
        compute_type="int8"
    )
    # model_path = "./local_models/amitkot/whisper-tiny-he-acft"
    # model = WhisperModel(model_path, 
    #     device="auto", 
    #     cpu_threads=threads_compute,
    #     num_workers=threads_compute,
    #     compute_type="int8"
    # )
    # model_path = "./local_models/whisper-tiny-he-acft-ct2-cpu"
    # model = WhisperModel(model_path, 
    #     device="auto", 
    #     cpu_threads=threads_compute,
    #     num_workers=threads_compute,
    #     compute_type="int8"
    # )
    # processor = WhisperProcessor.from_pretrained("amitkot/whisper-tiny-he-acft")
    # model = WhisperForConditionalGeneration.from_pretrained("amitkot/whisper-tiny-he-acft")


    print("Done Loading Local Whisper Model")
    print("Waiting for Keypress 's' to start recording")
    
    kb.wait('s')


    print("Recording started... speak now!")
    audioRecord = sd.rec(
        audio_sampleRate * audio_durationSec, 
        samplerate=audio_sampleRate, 
        channels=1, 
        blocking=False,
        dtype='float32'
    )
    sd.wait()
    print("Recording finished.")

    audioRecord = audioRecord.flatten()
    print(f"audio buffer is of shape {audioRecord.shape}") # Should be (80000,)

    segments, info = model.transcribe(
        audioRecord, 
        language='he',
        log_progress=True,
        vad_filter=True,
        beam_size=5
    )
    with tqdm(total=info.duration, unit="sec", desc="Transcribing") as progressbar:
        for segment in segments:
            print(f"\n[Fixed] {segment.text}")
            progressbar.update(segment.end - progressbar.n) # Update bar based on timestamp

    for segment in segments:
        print(f"Segment [{segment.start}s -> {segment.end}s] Text: {segment.text}")


