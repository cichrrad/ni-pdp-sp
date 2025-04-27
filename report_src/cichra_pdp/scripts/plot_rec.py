import pandas as pd
import matplotlib.pyplot as plt

# Načti všechny csv soubory
seq = pd.read_csv("seq.csv")
mp_task = pd.read_csv("mp_task.csv")
mp_data = pd.read_csv("mp_data.csv")
mpi_1 = pd.read_csv("mpi_1.csv")
mpi_2 = pd.read_csv("mpi_2.csv")
mpi_3 = pd.read_csv("mpi_3.csv")


# Přidej jména implementací
seq["impl"] = "seq"
mp_task["impl"] = "omp_task"
mp_data["impl"] = "omp_data"
mpi_1["impl"] = "mpi_1"
mpi_2["impl"] = "mpi_2"
mpi_3["impl"] = "mpi_3"

# Spojíme všechny dohromady
all_data = pd.concat([seq, mp_task, mp_data, mpi_1, mpi_2, mpi_3])

# Připrav správné sloupce
all_data = all_data[["file", "a", "impl", "recursion calls"]]

# Pivot podle (file, a)
pivot = all_data.pivot(index=["file", "a"], columns="impl", values="recursion calls").reset_index()

# Spočítáme relativní recursion calls oproti sekvenční
for col in ["omp_task", "omp_data", "mpi_1", "mpi_2", "mpi_3"]:
    if col in pivot.columns:
        pivot[f"{col}_rel_seq"] = pivot[col] / pivot["seq"]

# --- Graf ---
impls = ["omp_task_rel_seq", "omp_data_rel_seq", "mpi_1_rel_seq", "mpi_2_rel_seq", "mpi_3_rel_seq"]
labels = ["OpenMP Task", "OpenMP Data", "MPI 1", "MPI 2", "MPI 3"]

plt.figure(figsize=(16,8))

x = range(len(pivot))
for i, impl in enumerate(impls):
    if impl in pivot.columns:
        plt.bar([pos + i*0.15 for pos in x], pivot[impl], width=0.15, label=labels[i])

xtick_labels = [f"{row['file'].replace('.txt', '')}\na={row['a']}" for _, row in pivot.iterrows()]
plt.xticks([pos + 0.3 for pos in x], xtick_labels, rotation=90)

plt.yscale('log')
plt.ylabel("Relativní počet rekurzivních volání (oproti sekvenční implementaci)")
plt.xlabel("Graf + a")
plt.title("Poměr rekurzivních volání implementací vůči sekvenční verzi")
plt.legend()
plt.grid(axis="y")
plt.tight_layout()
plt.savefig("recursion_ratio.png")

pivot_table = pivot[["file"] + impls]
print("\nShrnutí poměrů počtu volání:\n")
print(pivot_table.to_markdown(index=False))


impls_abs = ["seq", "omp_task", "omp_data", "mpi_1", "mpi_2", "mpi_3"]
labels_abs = ["Sekvenční", "OpenMP Task", "OpenMP Data", "MPI 1", "MPI 2", "MPI 3"]

plt.figure(figsize=(16,8))
x = range(len(pivot))
for i, impl in enumerate(impls_abs):
    if impl in pivot.columns:
        plt.bar([pos + i*0.15 for pos in x], pivot[impl], width=0.15, label=labels_abs[i])

xtick_labels = [f"{row['file'].replace('.txt', '')}\na={row['a']}" for _, row in pivot.iterrows()]
plt.xticks([pos + 0.3 for pos in x], xtick_labels, rotation=90)

plt.yscale('log')
plt.ylabel("Počet rekurzivních volání (absolutní)")
plt.xlabel("Graf + a")
plt.title("Absolutní rekurzivní volání podle implementace")
plt.legend()
plt.grid(axis="y")
plt.tight_layout()
plt.savefig("recursion_abs.png")

# --- Tabulka ---
pivot_table = pivot[["file", "a"] + impls + impls_abs]
print("\nShrnutí recursion calls (relativní i absolutní):\n")
print(pivot_table.to_markdown(index=False))
