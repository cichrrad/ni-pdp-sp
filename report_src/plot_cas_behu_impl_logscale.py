import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import numpy as np

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

# Načti a sjednoť data
dfs = []
for f, label in files.items():
    if not os.path.exists(f):
        print(f"Warning: Soubor {f} nenalezen, přeskočeno.")
        continue
    df = pd.read_csv(f)
    df['impl'] = label
    dfs.append(df)

# Sloučení všech dat
if not dfs:
    raise ValueError("Nebyl nalezen žádný validní CSV soubor.")

all_data = pd.concat(dfs, ignore_index=True)
all_data['time'] = all_data['time'].astype(float)
all_data['recursion calls'] = all_data['recursion calls'].astype(int)

# Vytvoř výstupní tabulku s pivotem (graf × implementace)
pivot_table = all_data.pivot_table(
    index=['file', 'a'],
    columns='impl',
    values='time'
).reset_index()

# Spočítej speedup vůči sekvenční verzi
if 'seq' in all_data['impl'].unique():
    for impl in all_data['impl'].unique():
        if impl != 'seq':
            pivot_table[f'speedup_{impl}'] = pivot_table['seq'] / pivot_table[impl]

# Ulož tabulku do souboru
pivot_table.to_csv("tabulka_vysledku.csv", index=False)
print("Výstupní tabulka uložena jako tabulka_vysledku.csv")

# Ukázkový graf: běhový čas pro vybrané grafy
plt.figure(figsize=(14, 6))
vzorky = ['graf_20_7.txt', 'graf_30_10.txt', 'graf_40_15.txt']
vzorek_data = all_data[all_data['file'].isin(vzorky)]

sns.barplot(data=vzorek_data, x='file', y='time', hue='impl')
plt.title('Porovnání běhových časů pro různé implementace')
plt.ylabel('Čas běhu [s]')
plt.xlabel('Vstupní graf')
plt.yscale('log')
plt.legend(title='Implementace')
plt.tight_layout()
plt.savefig("cas_behu_impl_logscale.png")

