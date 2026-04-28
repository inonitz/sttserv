import json
import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as tick
from pathlib import Path


def parse_benchmark_files(file_paths: list[str]):
    all_results = []
    
    # Regex to extract: operation_name, type, and N
    # Matches: BM_GenericRecursiveAVLTreeInsertion<u64>/1024/repeats:2
    #          BM_StdUnorderedMapInsertion<uint64_t>/1024/repeats:2
    #          BM_StdSetDeletion<uint64_t>/1024/repeats:2
    pattern = re.compile(r"BM_(?P<op>\w+)<(?P<type>.*?)>/(?P<n>\d+)")


    # Check if file paths given are incorrect
    # print failure to the screen before starting any work.
    countNotFiles = 0
    for file_path in file_paths:
        file_path = Path(file_path)
        if not file_path.is_file() or file_path.suffixes[-1] != '.json':
            print("'{}' was not found. Either the Path is incorrect, the File doesn't exist or just not a .json file".format(file_path))
            countNotFiles += 1
        
    if countNotFiles > 0:
        print("Some filepaths were incorrect/not found. stopping execution")
        return None


    for file_path in file_paths:
        with open(file_path, 'r') as f:
            # print(file_path)
            data = json.load(f)
            
            for run in data.get('benchmarks', []):
                # We only want 'mean' aggregates to avoid plotting raw repetitions
                if run.get('run_type') == 'aggregate' and run.get('aggregate_name') == 'mean':
                    match = pattern.search(run['name'])
                    if match:                        
                        all_results.append({ # type: ignore
                            'Operation': match.group('op'),
                            'TestType': match.group('op'),
                            'Type': match.group('type'),
                            'N': int(match.group('n')),
                            'RealTime': run['real_time'],
                            'TimeUnit': run['time_unit'],
                            'Source': Path(file_path).stem
                        })
    
    return pd.DataFrame(all_results)


def plot_results(df: pd.DataFrame, graph_and_show_plot_to_user=False, save_graphs_to_files=False):
    df['TestType'] = df['Operation'].str.extract('(Insertion|Deletion|Search)', expand=False)
    data_types = df['Type'].unique()
    test_types = df['TestType'].unique()

    # print(data_types)
    # print(test_types)
    # print(df)
    # print(df.to_string())
    for dtype in data_types:
        for ttype in test_types:
            subset = df[(df['Type'] == dtype) & (df['TestType'] == ttype)]
            plt.figure(figsize=(10, 6))
            
            # Group by operation and source file
            for label, group in subset.groupby(['TestType', 'Source']):
                group = group.sort_values('N')
                plt.plot(group['N'], group['RealTime'], marker='o', label=f"{label[1]}")


            # Setup Plot
            timeUnitStr = subset['TimeUnit'].iloc[0]
            plt.title(f"Performance Benchmark - TestType: {ttype} DataType: {dtype}", loc='left')
            plt.xlabel("Data Structure Size (Node Count) (N)")
            plt.ylabel(f"Real Time ({timeUnitStr})")
            plt.xscale('log')
            plt.yscale('log')
            plt.grid(visible=True, which="both", ls="-", alpha=0.5)
            plt.legend()
            plt.tight_layout()

            # Setup formatting for time measurements & data-structure-size (y & x axes)
            axes = plt.gcf().gca()
            xaxis = axes.xaxis
            yaxis = axes.yaxis

            # Format y axis to get better reading on performance measurements
            yaxis.set_major_formatter(tick.FormatStrFormatter('%4.1f' + timeUnitStr))
            yaxis.set_minor_formatter(tick.FormatStrFormatter('%4.1f' + timeUnitStr))
            yaxis.set_major_locator(tick.LogLocator(base=10, numticks=12))
            yaxis.set_minor_locator(tick.LogLocator(base=10, subs='auto', numticks=12))
            yaxis.set_tick_params(which='minor', labelsize=8, colors='gray')
            
            # # format xaxis to understand better where bottlenecks occur
            # # Needs a big plot to not clutter the graph
            # xaxis.set_minor_locator(tick.LogLocator(base=10, subs='auto', numticks=5))
            # xaxis.set_minor_formatter(tick.LogFormatterExponent(
            #     base=10, 
            #     labelOnlyBase=False,
            #     minor_thresholds=(np.inf, np.inf)
            #     )
            # )
            # xaxis.set_tick_params(which='minor', labelsize=5, colors='gray')

            if save_graphs_to_files:
                plt.savefig("plot_{}_{}.svg".format(dtype, ttype), format="svg")

            # this clears the canvas, so it's done last.
            if graph_and_show_plot_to_user:
                plt.show()



if __name__ == "__main__":
    JsonDirToScan = Path('iterations/3')
    ShowPlotsFlag = False
    SavePlotsFlag = True
    fileList = []

    # fileList = [ f for f in JsonDirToScan.iterdir() if (f.is_file() and f.with_suffix(".json")) ]
    for file in JsonDirToScan.iterdir():
        if file.is_file() and file.suffixes[-1] == '.json':
            fileList.append(file)


    # [print(file) for file in fileList]
    df = parse_benchmark_files(fileList)
    if df is None:
        print("Error Processing File List")
    else:
        plot_results(df, ShowPlotsFlag, SavePlotsFlag)