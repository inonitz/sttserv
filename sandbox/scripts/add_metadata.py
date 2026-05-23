import onnx
from concurrent.futures import ProcessPoolExecutor, as_completed

def fix_sherpa_metadata(input_path, output_path):
    print(f"Loading: {input_path}...")
    try:
        model = onnx.load(input_path)
        
        # Validated explicitly against sherpa-onnx/scripts/whisper/export-onnx.py
        meta_data = {
            "model_type": "whisper",
            "vocab_size": "51865",
            "n_vocab": "51865",
            "is_multilingual": "1",
            "n_mels": "128",            
            "n_audio_ctx": "1500",
            "n_audio_state": "1280",
            "n_audio_head": "20",
            "n_audio_layer": "32",
            "n_text_ctx": "448",
            "n_text_state": "1280",
            "n_text_head": "20",
            "n_text_layer": "4",
            "sot_prev": "50361",
            "sot_lm": "50360",
            "blank_id": "220",
            "no_timestamps": "50363",
            "version": "1",
            "sot_sequence": "50258,50259,50359"
        }
        
        while len(model.metadata_props):
            model.metadata_props.pop()
            
        for key, value in meta_data.items():
            meta = model.metadata_props.add()
            meta.key = key
            meta.value = str(value)
            
        print(f"Saving to: {output_path}...")
        onnx.save(model, output_path)
        return f"Successfully processed: {output_path}"
        
    except Exception as e:
        return f"ERROR on file {input_path}: {str(e)}"

if __name__ == "__main__":
    files = [
        (
            "../../dependencies/models/sherpa-onnx/sherpa-onnx-whisper-distil-large-v3.5/distil-large-v3.5-encoder.int8.onnx",
            "../../dependencies/models/sherpa-onnx/sherpa-onnx-whisper-distil-large-v3.5/whisper-encoder.onnx",
        ),
        (
            "../../dependencies/models/sherpa-onnx/sherpa-onnx-whisper-distil-large-v3.5/distil-large-v3.5-decoder.int8.onnx",
            "../../dependencies/models/sherpa-onnx/sherpa-onnx-whisper-distil-large-v3.5/whisper-decoder.onnx"
        )
    ]
    
    with ProcessPoolExecutor(max_workers=2) as executor:
        futures = [executor.submit(fix_sherpa_metadata, inp, outp) for inp, outp in files]
        for future in as_completed(futures):
            print(future.result())