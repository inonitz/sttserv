import requests
import matplotlib.pyplot as plt
import time

def fetch_advanced_stats_patiently(repo_owner, repo_name, top_n=15):
    url = f"https://api.github.com/repos/{repo_owner}/{repo_name}/stats/contributors"
    
    max_retries = 10
    retry_delay = 10  # seconds
    data = None

    print(f"Requesting advanced stats for {repo_owner}/{repo_name}...")
    
    for attempt in range(1, max_retries + 1):
        response = requests.get(url)
        
        # Check if GitHub is still building the cache
        if response.status_code == 202 or (response.status_code == 200 and not response.json()):
            print(f"[{attempt}/{max_retries}] GitHub is compiling data in the background. Retrying in {retry_delay}s...")
            time.sleep(retry_delay)
        elif response.status_code == 200:
            data = response.json()
            print("Success! Data retrieved.")
            break
        else:
            print(f"Error: Server returned status code {response.status_code}")
            return

    if not data:
        print("\n[Timeout] GitHub is taking too long to compute statistics.")
        print("Workaround: Open this link in your browser to force-warm the cache, then run this script again:")
        print(f"https://github.com/{repo_owner}/{repo_name}/graphs/contributors-data")
        return

    # --- (The rest of the graphing logic remains the same) ---
    data.reverse()
    top_data = data[:top_n]

    logins, additions, deletions = [], [], []
    for item in top_data:
        logins.append(item['author']['login'])
        additions.append(sum(week['a'] for week in item['weeks']))
        deletions.append(-sum(week['d'] for week in item['weeks'])) 

    plt.figure(figsize=(14, 7))
    bars_add = plt.bar(logins, additions, color='#2ea043', label='Additions (+)')
    bars_del = plt.bar(logins, deletions, color='#f85149', label='Deletions (-)')
    
    plt.title(f"Lines Added vs Removed: Top {top_n} Contributors to {repo_owner}/{repo_name}", fontsize=14, pad=15)
    plt.xlabel("GitHub Username", fontsize=12)
    plt.ylabel("Lines of Code", fontsize=12)
    plt.axhline(0, color='black', linewidth=1)
    plt.xticks(rotation=45, ha='right')
    plt.legend()

    locs, labels = plt.yticks()
    plt.yticks(locs, [str(abs(int(loc))) for loc in locs])
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    # fetch_advanced_stats_patiently("k2-fsa", "sherpa-onnx", top_n=15)
    fetch_advanced_stats_patiently("mawwalker", "stt-server", top_n=5)