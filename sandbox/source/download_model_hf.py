from ctypes import ArgumentError
import os
import multiprocessing
import argparse
from pathlib import Path
import numpy as np
from huggingface_hub import HfApi
from huggingface_hub.utils.tqdm import enable_progress_bars



def estimate_download_size(repo_id) -> float:
    api = HfApi()
    # Fetch model metadata (only a few KB of data)
    models = api.model_info(repo_id, files_metadata=True)
    # print(models.author, models.cardData, models.downloads)
    
    # Sum the sizes of all files in the repository
    total_size_bytes = 0
    if models.siblings is not None:
        for f in models.siblings:
            currfileSize = f.size if f.size is not None else 0
            total_size_bytes += currfileSize
    
    # Convert to human-readable format
    # total_gb = total_size_bytes / (1024**2)
    # print(f"Model: {repo_id}")
    # print(f"Total Download Size: {total_gb:.2f} MiB")
    return total_size_bytes


def format_size(size_bytes):
    if size_bytes == 0:
        return "0B"
    size_name = ("B", "KB", "MB", "GB", "TB")
    i = int(np.floor(np.emath.logn(1024, size_bytes)))
    p = np.pow(1024, i)
    s = round(size_bytes / p, 2)
    return f"{s} {size_name[i]}"


def simulate_dry_run(repo_id):
    api = HfApi()
    
    # Fetch metadata with file sizes included
    print(f"Fetching metadata for {repo_id}...\n")
    try:
        info = api.model_info(repo_id, files_metadata=True)
    except Exception as e:
        print(f"Error: Could not find repository. {e}")
        return

    # Header for the "table"
    print(f"{'File Name':<50} | {'Size':<10}")
    print("-" * 63)

    total_bytes = 0
    file_count = 0

    # Iterate through files (siblings)
    if info.siblings is None:
        return
    
    for file in sorted(info.siblings, key=lambda x: x.rfilename):
        size = file.size if file.size is not None else 0
        total_bytes += size
        file_count += 1
        
        # Truncate long filenames for clean printing
        display_name = (file.rfilename[:47] + '..') if len(file.rfilename) > 50 else file.rfilename
        print(f"{display_name:<50} | {format_size(size):<10}")

    print("-" * 63)
    print(f"Total Files: {file_count}")
    print(f"Total Download Size: {format_size(total_bytes)}")
    return


def download_repo_hf(repo_id, output_folder_path, TestOnly=False):
    api = HfApi()

    # Test if repo exists and exit early if not
    info = None
    try:
        info = api.model_info(repo_id, files_metadata=True)
    except Exception as e:
        print(f"Error: Could not find repository -> {e}")
        return
    

    local_outpath = Path(os.getcwd()) / output_folder_path
    cache_outpath = local_outpath / 'cache'

    local_outpath.mkdir(parents=True, exist_ok=True)
    cache_outpath.mkdir(parents=True, exist_ok=True) 
    # actually try to download
    try:
        print(f"Downloading {repo_id}...\nOutput directory: '{str(local_outpath)}'\nCache  Directory: '{str(cache_outpath)}'")
        print(f"Estimated Repository Size is { format_size(estimate_download_size(repo_id)) }")
        api.snapshot_download(repo_id, 
            local_dir=str(local_outpath),
            cache_dir=str(cache_outpath),
            max_workers=int(multiprocessing.cpu_count() * 3 / 2),
            dry_run=TestOnly
        )
    except Exception as e:
        print(f"Error Downloading Repository {repo_id} -> {e}")
        return


    print(f"\nFinished Download Successfully!\n")
    return


if __name__ == "__main__":
    # add option to select quantizations if available
    parser = argparse.ArgumentParser()
    args   = argparse.Namespace()
    parser.add_argument('-m', "--modelname", 
        help="" \
        "   Name of the model to download from the HuggingFace Repository. Example:\n" \
        "       download_model_hf.py --modelname=ivrit-ai/whisper-large-v3",
        type=str
    )
    parser.add_argument('-o', "--outputpath", 
        help="" \
        "   The output directory. Files will be placed in ${CWD}/OUTPUTPATH. Example:\n" \
        "       download_model_hf.py --modelname=ivrit-ai/whisper-large-v3 --outputpath=models/whisper_largev3",
        type=str
    )

    model_to_download = ''
    output_path = ''
    try:
        args = parser.parse_args()
    except Exception as e:
        print(f"Error while processing command line arguments: {e}")

    if args.modelname is None or args.modelname == '':
        print(f"Modelname not specified/empty, received str '{args.modelname}'. Stopping")
        exit()


    model_to_download = args.modelname
    outputpath        = args.outputpath
    # repo = "ivrit-ai/whisper-large-v3-ct2"
    enable_progress_bars()
    download_repo_hf(model_to_download, outputpath, TestOnly=False)