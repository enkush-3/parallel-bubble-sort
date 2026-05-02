import pandas as pd
import matplotlib.pyplot as plt
import os

OUTPUT_DIR = 'plots'
if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

# CSV файлыг унших (C++ кодноос гарсан файлын нэр: thread_results.csv)
csv_file = 'thread_results.csv'
if not os.path.exists(csv_file):
    print(f"Алдаа: '{csv_file}' файл олдсонгүй. Эхлээд C++ програмаа ажиллуулна уу.")
    exit(1)

df = pd.read_csv(csv_file)

# Баганын нэрүүдийг шалгах
required_cols = ['N', 'seq_time_ms', 'par_time_ms', 'comp_time_p', 'transfer_time_ms',
                 'mops_s', 'mops_p', 'speedup', 'efficiency_%', 'threads']
for col in required_cols:
    if col not in df.columns:
        print(f"Анхаар: '{col}' багана CSV-д байхгүй байна.")
        print("Боломжтой баганууд:", list(df.columns))
        exit(1)

# Хугацааг секунд руу хөрвүүлэх (графикт илүү ойлгомжтой харагдуулахын тулд)
df['seq_time_s'] = df['seq_time_ms'] / 1000.0
df['par_time_s'] = df['par_time_ms'] / 1000.0

# Графикийн стиль
plt.style.use('seaborn-v0_8-darkgrid')
colors = plt.cm.tab10.colors

# =========================================================
# 1. Хугацааны харьцуулалт (Log масштаб)
# =========================================================
fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(df['N'], df['seq_time_s'], 'o-', label='Sequential Time (Хуучин)', color=colors[0], linewidth=2, markersize=8)
ax.plot(df['N'], df['par_time_s'], 's-', label=f"Parallel Time ({df['threads'].iloc[0]} threads)", color=colors[2], linewidth=2, markersize=8)

ax.set_xlabel('Өгөгдлийн хэмжээ (N)', fontsize=12)
ax.set_ylabel('Хугацаа (секунд)', fontsize=12)
ax.set_title('Sequential vs Parallel Гүйцэтгэлийн хугацаа (Бага байх тусмаа сайн)', fontsize=14)
ax.set_yscale('log')
ax.set_xscale('log')
ax.legend()
ax.grid(True, which='both', linestyle='--', linewidth=0.5)
plt.tight_layout()
plt.savefig(f'{OUTPUT_DIR}/time_comparison.png', dpi=150)

# =========================================================
# 2. SpeedUp болон Efficiency харьцуулалт (Хос Y тэнхлэгтэй)
# =========================================================
fig, ax1 = plt.subplots(figsize=(10, 6))

# Зүүн Y тэнхлэг (SpeedUp)
color1 = colors[3]
ax1.plot(df['N'], df['speedup'], 'o-', label='SpeedUp (Хурдсалт)', color=color1, linewidth=2, markersize=8)
ax1.set_xlabel('Өгөгдлийн хэмжээ (N)', fontsize=12)
ax1.set_ylabel('SpeedUp (Дахин хурдссан)', fontsize=12, color=color1)
ax1.tick_params(axis='y', labelcolor=color1)
ax1.axhline(y=1, color='r', linestyle=':', label='No SpeedUp (1x)')
ax1.set_xscale('log')

# Баруун Y тэнхлэг (Efficiency %)
ax2 = ax1.twinx()  
color2 = colors[4]
ax2.plot(df['N'], df['efficiency_%'], 's--', label='Efficiency (Үр ашиг %)', color=color2, linewidth=2, markersize=8)
ax2.set_ylabel('Үр ашиг (%)', fontsize=12, color=color2)
ax2.tick_params(axis='y', labelcolor=color2)
ax2.set_ylim(0, max(100, df['efficiency_%'].max() + 10)) # 0-ээс 100+ хүртэл

plt.title(f"SpeedUp болон CPU Үр ашиг ({df['threads'].iloc[0]} threads)", fontsize=14)
fig.tight_layout()
plt.savefig(f'{OUTPUT_DIR}/speedup_efficiency.png', dpi=150)

# =========================================================
# 3. Гүйцэтгэлийн бүтээмж (MOPS - Million Operations Per Second)
# =========================================================
fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(df['N'], df['mops_s'], 'o-', label='Sequential (MOPS)', color=colors[5], linewidth=2, markersize=8)
ax.plot(df['N'], df['mops_p'], 's-', label='Parallel (MOPS)', color=colors[6], linewidth=2, markersize=8)

ax.set_xlabel('Өгөгдлийн хэмжээ (N)', fontsize=12)
ax.set_ylabel('Бүтээмж (MOPS)', fontsize=12)
ax.set_title('CPU Тооцооллын бүтээмж (Их байх тусмаа сайн)', fontsize=14)
ax.set_xscale('log')
ax.legend()
ax.grid(True, which='both', linestyle='--', linewidth=0.5)
plt.tight_layout()
plt.savefig(f'{OUTPUT_DIR}/performance_mops.png', dpi=150)

# =========================================================
# 4. Parallel хугацааны дотоод задлагаа (Stacked bar)
# =========================================================
fig, ax = plt.subplots(figsize=(10, 6))

x_labels = df['N'].astype(str)
# Баганууд нь миллисекундээр байгаа
comp_time = df['comp_time_p']
transfer_time = df['transfer_time_ms']

ax.bar(x_labels, comp_time, label='Computation Time (Цэвэр бодолт)', color=colors[7])
ax.bar(x_labels, transfer_time, bottom=comp_time, label='Memory Transfer (Санах ой руу хандах хугацаа)', color=colors[8])

ax.set_xlabel('Өгөгдлийн хэмжээ (N)', fontsize=12)
ax.set_ylabel('Хугацаа (Миллисекунд - ms)', fontsize=12)
ax.set_title('Parallel хугацааны дотоод зарцуулалтын задлагаа', fontsize=14)
ax.legend()
ax.grid(True, axis='y')
plt.tight_layout()
plt.savefig(f'{OUTPUT_DIR}/parallel_time_breakdown.png', dpi=150)

# =========================================================
# Үр дүнг хэвлэх
print("\n[АМЖИЛТТАЙ] Бүх график дараах нэрээр хадгалагдлаа:")
print("  1. time_comparison.png       (Хугацааны харьцуулалт)")
print("  2. speedup_efficiency.png    (Хурдсалт болон үр ашиг)")
print("  3. performance_mops.png      (MOPS үзүүлэлт)")
print("  4. parallel_time_breakdown.png(Дотоод хугацааны задлагаа)\n")