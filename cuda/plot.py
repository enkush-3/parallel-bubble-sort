import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import os
import sys

OUTPUT_DIR = 'plots'
if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

# CSV файл эх хавтасанд байгаа тул директороор солихгүй
csv_file = 'results.csv'
if not os.path.exists(csv_file):
    print(f"❌ Алдаа: '{csv_file}' файл олдсонгүй. Эхлээд CUDA програмаа ажиллуулна уу.")
    sys.exit(1)

df = pd.read_csv(csv_file)

# Баганын нэрүүдийг шалгах
transfer_col = 'transfer' if 'transfer' in df.columns else 'transfer_total_sec'
required_cols = ['N', 'cpu_time', 'gpu_kernel', transfer_col, 'total_exec',
                 'cpu_gops', 'gpu_gops', 'speedup', 'eff_speedup']

missing_cols = [col for col in required_cols if col not in df.columns]
if missing_cols:
    print(f"❌ Анхаар: Дараах баганууд CSV-д байхгүй байна: {missing_cols}")
    print("Боломжтой баганууд:", list(df.columns))
    sys.exit(1)

# Графикийн ерөнхий тохиргоо
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams['font.size'] = 11
plt.rcParams['axes.titlesize'] = 14
plt.rcParams['axes.labelsize'] = 12
colors = plt.cm.Set1.colors

# Х тэнхлэгийн тоог мянгатаар таслах
formatter = ticker.FuncFormatter(lambda x, p: format(int(x), ','))

# =====================================================================
# 1. Хугацааны харьцуулалт
# =====================================================================
fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(df['N'], df['cpu_time'], 'o-', label='CPU Bubble Sort', color=colors[1], linewidth=2.5, markersize=8)
ax.plot(df['N'], df['total_exec'], '^-', label='GPU (Kernel + Transfer)', color=colors[0], linewidth=2.5, markersize=8)
ax.plot(df['N'], df['gpu_kernel'], 's--', label='GPU Kernel (Цэвэр бодолт)', color=colors[2], linewidth=2, markersize=7)

last_n = df['N'].iloc[-1]
ax.text(last_n, df['cpu_time'].iloc[-1], f" {df['cpu_time'].iloc[-1]:.2f}s", color=colors[1], va='bottom')
ax.text(last_n, df['total_exec'].iloc[-1], f" {df['total_exec'].iloc[-1]:.2f}s", color=colors[0], va='top')

ax.set_xlabel('Өгөгдлийн хэмжээ (N)')
ax.set_ylabel('Хугацаа (секунд)')
ax.set_title('CPU vs GPU Гүйцэтгэлийн хугацаа (Бага байх тусмаа сайн)')
ax.set_yscale('log')
ax.set_xscale('log')
ax.legend(frameon=True, shadow=True)
plt.tight_layout()
plt.savefig(os.path.join(OUTPUT_DIR, '1_time_comparison.png'), dpi=300, bbox_inches='tight')

# =====================================================================
# 2. SpeedUp харьцуулалт
# =====================================================================
fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(df['N'], df['speedup'], 'o-', label='Цэвэр SpeedUp (CPU / GPU Kernel)', color=colors[2], linewidth=2.5, markersize=8)
ax.plot(df['N'], df['eff_speedup'], 's-', label='Бодит SpeedUp (CPU / GPU Total)', color=colors[0], linewidth=2.5, markersize=8)

ax.axhline(y=1, color='red', linestyle='--', label='Хурдсаагүй (1x Baseline)', alpha=0.7)
ax.fill_between(df['N'], 1, df['eff_speedup'], color=colors[0], alpha=0.1)

max_idx = df['eff_speedup'].idxmax()
ax.annotate(f"Max: {df['eff_speedup'].max():.1f}x",
            xy=(df['N'].iloc[max_idx], df['eff_speedup'].max()),
            xytext=(-40, 20), textcoords='offset points',
            arrowprops=dict(arrowstyle="->", color=colors[0]))

ax.set_xlabel('Өгөгдлийн хэмжээ (N)')
ax.set_ylabel('SpeedUp (x удаа хурдсан)')
ax.set_title('GPU-ийн хурдсалт (Их байх тусмаа сайн)')
ax.set_xscale('log')
ax.legend(loc='upper left', frameon=True, shadow=True)
plt.tight_layout()
plt.savefig(os.path.join(OUTPUT_DIR, '2_speedup.png'), dpi=300, bbox_inches='tight')

# =====================================================================
# 3. Гүйцэтгэлийн бүтээмж (GOps/s)
# =====================================================================
fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(df['N'], df['cpu_gops'], 'o-', label='CPU Бүтээмж (GOps/s)', color=colors[1], linewidth=2.5, markersize=8)
ax.plot(df['N'], df['gpu_gops'], 's-', label='GPU Бүтээмж (GOps/s)', color=colors[0], linewidth=2.5, markersize=8)

ax.text(last_n, df['gpu_gops'].iloc[-1], f" {df['gpu_gops'].iloc[-1]:.2f}", color=colors[0], va='bottom')

ax.set_xlabel('Өгөгдлийн хэмжээ (N)')
ax.set_ylabel('Бүтээмж (GOps/s)')
ax.set_title('CPU ба GPU-ийн тооцооллын бүтээмж (Их байх тусмаа сайн)')
ax.set_xscale('log')
ax.legend(frameon=True, shadow=True)
plt.tight_layout()
plt.savefig(os.path.join(OUTPUT_DIR, '3_performance.png'), dpi=300, bbox_inches='tight')

# =====================================================================
# 4. GPU доторх хугацааны хуваарилалт (Stacked bar)
# =====================================================================
fig, ax = plt.subplots(figsize=(10, 6))

x_labels = [format(int(n), ',') for n in df['N']]
x_pos = np.arange(len(x_labels))

bars1 = ax.bar(x_pos, df['gpu_kernel'], label='Kernel Time (Цэвэр бодолт)', color='#3498db', edgecolor='black')
bars2 = ax.bar(x_pos, df[transfer_col], bottom=df['gpu_kernel'], label='Transfer Time (Санах ой)', color='#e74c3c', edgecolor='black')

for i in range(len(x_labels)):
    k_time = df['gpu_kernel'].iloc[i]
    t_time = df[transfer_col].iloc[i]
    total = k_time + t_time

    if k_time / total > 0.1:
        ax.text(i, k_time/2, f"{k_time:.3f}s", ha='center', va='center', color='white', fontweight='bold')
    if t_time / total > 0.1:
        ax.text(i, k_time + t_time/2, f"{t_time:.3f}s", ha='center', va='center', color='white', fontweight='bold')

    ax.text(i, total + (total*0.02), f"Нийт:\n{total:.2f}s", ha='center', va='bottom', color='black', fontsize=10)

ax.set_xticks(x_pos)
ax.set_xticklabels(x_labels)
ax.set_xlabel('Өгөгдлийн хэмжээ (N)')
ax.set_ylabel('Хугацаа (секунд)')
ax.set_title('GPU дотоод хугацааны зарцуулалтын задлагаа')
ax.legend(frameon=True, shadow=True)
ax.set_ylim(0, df['total_exec'].max() * 1.15)

plt.tight_layout()
plt.savefig(os.path.join(OUTPUT_DIR, '4_gpu_breakdown.png'), dpi=300, bbox_inches='tight')

print("\n✅ Бүх график амжилттай өндөр нягтралтай (300 DPI) хадгалагдлаа:")
print(" - plots/1_time_comparison.png")
print(" - plots/2_speedup.png")
print(" - plots/3_performance.png")
print(" - plots/4_gpu_breakdown.png")