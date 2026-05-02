import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# CSV файлыг унших
csv_file = 'results.csv'
if not os.path.exists(csv_file):
    print(f"Алдаа: {csv_file} файл олдсонгүй. Эхлээд CUDA програмаа ажиллуулна уу.")
    exit(1)

df = pd.read_csv(csv_file)

# Баганын нэрүүдийг шалгах (таны гаргасан CSV баганатай таарч байх)
required_cols = ['N', 'cpu_time', 'gpu_kernel', 'transfer', 'total_exec',
                 'cpu_gops', 'gpu_gops', 'speedup', 'eff_speedup']
for col in required_cols:
    if col not in df.columns:
        print(f"Анхаар: '{col}' багана CSV-д байхгүй байна.")
        # Боломжтой баганыг хэвлэх
        print("Боломжтой баганууд:", list(df.columns))
        exit(1)

# Графикийн стиль
plt.style.use('seaborn-v0_8-darkgrid')
colors = plt.cm.tab10.colors

# 1. Хугацааны харьцуулалт (лог масштаб)
fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(df['N'], df['cpu_time'], 'o-', label='CPU Bubble Sort', color=colors[0], linewidth=2, markersize=8)
ax.plot(df['N'], df['gpu_kernel'], 's-', label='GPU Kernel (Odd-Even)', color=colors[1], linewidth=2, markersize=8)
ax.plot(df['N'], df['total_exec'], '^-', label='GPU Kernel + Transfer', color=colors[2], linewidth=2, markersize=8)

ax.set_xlabel('Өгөгдлийн хэмжээ (N)', fontsize=12)
ax.set_ylabel('Хугацаа (секунд)', fontsize=12)
ax.set_title('CPU vs GPU Гүйцэтгэлийн хугацаа', fontsize=14)
ax.set_yscale('log')
ax.set_xscale('log')
ax.legend()
ax.grid(True, which='both', linestyle='--', linewidth=0.5)
plt.tight_layout()
plt.savefig('time_comparison.png', dpi=150)

# 2. SpeedUp харьцуулалт
fig, ax = plt.subplots(figsize=(8, 5))
ax.plot(df['N'], df['speedup'], 'o-', label='SpeedUp (CPU / GPU Kernel)', color=colors[3], linewidth=2, markersize=8)
ax.plot(df['N'], df['eff_speedup'], 's--', label='Effective SpeedUp (incl. transfer)', color=colors[4], linewidth=2, markersize=8)

ax.axhline(y=1, color='r', linestyle=':', label='No SpeedUp (1x)')
ax.set_xlabel('Өгөгдлийн хэмжээ (N)', fontsize=12)
ax.set_ylabel('SpeedUp (x удаа хурдсан)', fontsize=12)
ax.set_title('GPU-ийн SpeedUp (CPU-тай харьцуулахад)', fontsize=14)
ax.set_xscale('log')
ax.legend()
ax.grid(True)
plt.tight_layout()
plt.savefig('speedup.png', dpi=150)

# 3. Гүйцэтгэлийн бүтээмж (GOps/s)
fig, ax = plt.subplots(figsize=(8, 5))
ax.plot(df['N'], df['cpu_gops'], 'o-', label='CPU (GOps/s)', color=colors[5], linewidth=2, markersize=8)
ax.plot(df['N'], df['gpu_gops'], 's-', label='GPU (GOps/s)', color=colors[6], linewidth=2, markersize=8)

ax.set_xlabel('Өгөгдлийн хэмжээ (N)', fontsize=12)
ax.set_ylabel('Бүтээмж (GOps/s)', fontsize=12)
ax.set_title('CPU ба GPU-ийн тооцооллын бүтээмж', fontsize=14)
ax.set_xscale('log')
ax.legend()
ax.grid(True)
plt.tight_layout()
plt.savefig('performance.png', dpi=150)

# 4. GPU доторх хугацааны хуваарилалт (stacked bar)
fig, ax = plt.subplots(figsize=(8, 5))
# transfer багана байгаа эсэх шалгах (таны CSV-д transfer_total_sec эсвэл transfer байж болно)
transfer_col = 'transfer' if 'transfer' in df.columns else 'transfer_total_sec'
if transfer_col not in df.columns:
    print(f"'{transfer_col}' багана олдсонгүй, transfer хугацааг харуулах боломжгүй.")
else:
    ax.bar(df['N'].astype(str), df['gpu_kernel'], label='Kernel Time', color=colors[7])
    ax.bar(df['N'].astype(str), df[transfer_col], bottom=df['gpu_kernel'], label='Transfer Time (H2D+D2H)', color=colors[8])
    ax.set_xlabel('Өгөгдлийн хэмжээ (N)', fontsize=12)
    ax.set_ylabel('Хугацаа (секунд)', fontsize=12)
    ax.set_title('GPU доторх kernel болон дамжуулалтын хугацаа', fontsize=14)
    ax.legend()
    ax.grid(True, axis='y')
    plt.tight_layout()
    plt.savefig('gpu_breakdown.png', dpi=150)
    plt.show()

print("\nБүх график амжилттай хадгалагдлаа:")
print("  - time_comparison.png")
print("  - speedup.png")
print("  - performance.png")
if transfer_col in df.columns:
    print("  - gpu_breakdown.png")