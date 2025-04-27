import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
from pathlib import Path
from typing import List

# Load the master timing table
timing_table = pd.read_csv("tabulka_vysledku.csv")

# Load recursion call CSVs
def load_recursion_data(file_path: str, label: str) -> pd.DataFrame:
    df = pd.read_csv(file_path)
    df["impl"] = label
    return df[["file", "recursion calls", "impl"]]

# Paths to recursion files
rec_paths = {
    "seq": "seq.csv",
    "omp_task": "mp_task.csv",
    "omp_data": "mp_data.csv",
    "mpi_1": "mpi_1.csv",
    "mpi_2": "mpi_2.csv",
    "mpi_3": "mpi_3.csv",
}

# Load and merge recursion data
rec_data = pd.concat([load_recursion_data(path, label) for label, path in rec_paths.items() if Path(path).exists()])

# Pivot recursion data for joining
rec_pivot = rec_data.groupby(["file", "impl"])["recursion calls"].min().unstack().reset_index()


# Merge recursion data into timing table
combined = timing_table.merge(rec_pivot, on="file", how="left", suffixes=('', '_rec'))

# Compute speedup columns
for col in ["omp_data", "omp_task", "mpi_1", "mpi_2", "mpi_3"]:
    combined[f"speedup_seq_vs_{col}"] = combined["seq"] / combined[col]
    combined[f"speedup_ref_vs_{col}"] = combined["ref_seq"].replace(0, np.nan) / combined[col]

# Compute speedup between OpenMP task vs data
combined["speedup_omp_data_vs_task"] = combined["omp_task"] / combined["omp_data"]

# Save processed comparison table
combined_path = "combined_speedup_recursion.csv"
combined.to_csv(combined_path, index=False)

# Plot speedup: seq vs all other implementations
impls = ["omp_task","omp_data", "mpi_1", "mpi_2", "mpi_3"]
melted = combined.melt(id_vars="file", value_vars=[f"speedup_seq_vs_{i}" for i in impls],
                       var_name="implementation", value_name="speedup")
melted["implementation"] = melted["implementation"].str.replace("speedup_seq_vs_", "")

plt.figure(figsize=(14, 7))
plt.yscale('log')
plt.ylim(1, 1000)
sns.barplot(data=melted, x="file", y="speedup", hue="implementation", errorbar=None)
plt.title("Kladný speedup (log) oproti sekvenční verzi")
plt.xticks(rotation=90)
plt.tight_layout()
plt.savefig("speedup_vs_seq.png")

# Plot OpenMP: task vs data
plt.figure(figsize=(10, 5))
sns.barplot(data=combined, x="file", y="speedup_omp_data_vs_task", errorbar=None)
plt.title("Speedup: OpenMP Task vs Data")
plt.xticks(rotation=90)
plt.tight_layout()
plt.savefig("speedup_omp_data_vs_task.png")

# Plot ref CPU vs moje CPU
for impl in impls:
    plt.figure(figsize=(14, 6))
    speedup_col = f"speedup_ref_vs_{impl}"
    if speedup_col in combined.columns:
        sns.barplot(data=combined, x="file", y=speedup_col,errorbar=None)
        plt.title(f"Speedup: Referenční CPU vs {impl}")
        plt.xticks(rotation=90)
        plt.tight_layout()
        plt.savefig(f"speedup_ref_vs_{impl}.png")

