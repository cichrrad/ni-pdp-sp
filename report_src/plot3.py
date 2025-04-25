import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os

# CSV soubory a jejich labely
files = {
    'ref_seq.csv': 'ref_seq',
    'seq.csv': 'seq',
    'mp_task.csv': 'omp_task',
    'mp_data.csv': 'omp_data',
    'mpi_1.csv': 'mpi_1',
    'mpi_2.csv': 'mpi_2',
    'mpi_3.csv': 'mpi_3'
}

# Načtení a sloučení
dfs = []
for f, label in files.items():
    df = pd.read_csv(f)
    df['impl'] = label
    dfs.append(df)

all_data = pd.concat(dfs, ignore_index=True)

# Převod sloupců na správné typy
all_data['time'] = all_data['time'].astype(float)
all_data['recursion calls'] = all_data['recursion calls'].astype(int)

# -- G1: Čas běhu vs n --
plt.figure()
sns.lineplot(data=all_data, x='n', y='time', hue='impl', marker='o')
plt.yscale('log')
plt.title('Čas běhu vs velikost grafu (n)')
plt.xlabel('Počet uzlů (n)')
plt.ylabel('Čas [s]')
plt.grid(True)
plt.tight_layout()
plt.savefig('g1_time_vs_n.png')

# -- G2: Speedup vůči vlastní sekvenční verzi --
speedup_df = all_data.pivot_table(index=['file', 'n', 'a'], columns='impl', values='time').reset_index()
for impl in files.values():
    if impl != 'seq':
        speedup_df[f'speedup_{impl}'] = speedup_df['seq'] / speedup_df[impl]

# Graf speedup
speedup_melted = speedup_df.melt(id_vars=['file', 'n', 'a'], value_vars=[col for col in speedup_df.columns if col.startswith('speedup_')],
                                 var_name='impl', value_name='speedup')
speedup_melted['impl'] = speedup_melted['impl'].str.replace('speedup_', '')

plt.figure()
sns.barplot(data=speedup_melted, x='impl', y='speedup')
plt.title('Speedup vůči sekvenční implementaci')
plt.ylabel('Speedup')
plt.tight_layout()
plt.savefig('g2_speedup.png')

# -- G3: Rekurzivní volání --
plt.figure(figsize=(10, 6))
sns.boxplot(data=all_data, x='impl', y='recursion calls')
plt.yscale('log')
plt.title('Počet rekurzivních volání (log scale)')
plt.tight_layout()
plt.savefig('g3_rec_calls.png')

# -- G4: Srovnání všech implementací na jednotlivých grafech --
plt.figure(figsize=(14, 6))
sns.barplot(data=all_data, x='file', y='time', hue='impl')
plt.xticks(rotation=90)
plt.title('Srovnání časů běhu všech implementací')
plt.ylabel('Čas [s]')
plt.tight_layout()
plt.savefig('g4_time_all_files.png')

# -- Markdown-ready tabulky --
grouped = all_data.pivot_table(index=['file', 'n', 'a'], columns='impl', values='time').reset_index()
grouped.to_csv('comparison_table.csv', index=False)

