#!/usr/bin/env bash
# P10 基准框架（EmoPlay 解码计时）。
#
# 检测系统 ffmpeg / mpv，生成合成测试序列并对每个用例计时软解码，输出一段
# 可直接引用进 docs 的 Markdown 表格框架（docs/testing-report.md 的性能小节
# 引用它；实机跑分后把数字誊进测试报告）。
#
# 用法：
#   tools/benchmark.sh [输出目录] [样例媒体目录]
#     输出目录       生成的测试序列与 benchmark-results.md 的落点（默认
#                    ./bench-output；已存在的同名生成文件会被复用）
#     样例媒体目录   可选；目录下的视频/音频文件会作为额外用例计时
#                    （上限 5 个，按扩展名识别）
#
# 行为约定：
#   1. 系统缺 ffmpeg：跳过测试序列生成与 ffmpeg 解码计时（明确说明）。
#   2. 系统缺 mpv：跳过 mpv 解码计时（明确说明）。
#   3. 两项都缺：输出待填的表格框架并打印跳过说明，退出码 0（不算失败）。
#   4. 计时为软解码（ffmpeg 默认、mpv --hwdec=no），每用例跑 3 轮取最优；
#      墙钟计时（date +%s%N，不可用时回退 python3）。
#   5. 仅在 Linux（GNU coreutils/bash 4.4+）验证过；Windows/macOS 未验证。
set -euo pipefail

usage() {
    sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

case "${1:-}" in
    -h|--help) usage 0 ;;
esac

OUT_DIR="${1:-bench-output}"
SAMPLE_DIR="${2:-}"
RUNS=3
MAX_SAMPLES=5

mkdir -p "$OUT_DIR"
REPORT="$OUT_DIR/benchmark-results.md"

have() { command -v "$1" >/dev/null 2>&1; }

FFMPEG_BIN=""
MPV_BIN=""
have ffmpeg && FFMPEG_BIN="$(command -v ffmpeg)"
have mpv && MPV_BIN="$(command -v mpv)"

now_ns() {
    local t
    t="$(date +%s%N 2>/dev/null || true)"
    if [[ "$t" =~ ^[0-9]+$ ]]; then
        printf '%s' "$t"
        return 0
    fi
    # BSD/macOS date 没有 %N：回退 python3（两者皆无则计时不可用）。
    python3 -c 'import time; print(int(time.time() * 1e9))' 2>/dev/null
}

ns_to_s() {
    awk -v ns="$1" 'BEGIN { printf "%.2f", ns / 1000000000 }'
}

# best_time <命令...> —— 跑 $RUNS 轮，打印最小墙钟 ns；任一轮失败打印空串。
best_time() {
    local best="" i start end ns
    for ((i = 0; i < RUNS; i++)); do
        start="$(now_ns)" || return 0
        if ! "$@" >/dev/null 2>&1; then
            printf ''
            return 0
        fi
        end="$(now_ns)" || return 0
        ns=$((end - start))
        if [[ -z "$best" || "$ns" -lt "$best" ]]; then
            best="$ns"
        fi
    done
    printf '%s' "$best"
}

# 测试序列表：name|lavfi 源|秒|帧数|规格
SEQUENCES=(
    "bench_sd_360p30|testsrc2=size=640x360:rate=30|10|300|testsrc2 640x360@30"
    "bench_hd_1080p30|testsrc2=size=1920x1080:rate=30|10|300|testsrc2 1920x1080@30"
    "bench_hd_1080p60|testsrc2=size=1920x1080:rate=60|5|300|testsrc2 1920x1080@60"
    "bench_uhd_2160p30|testsrc2=size=3840x2160:rate=30|4|120|testsrc2 3840x2160@30"
)

echo "== EmoPlay 解码基准 =="
if [[ -n "$FFMPEG_BIN" ]]; then
    echo "ffmpeg: $($FFMPEG_BIN -version 2>/dev/null | head -n 1)"
else
    echo "系统未检测到 ffmpeg —— 跳过测试序列生成与 ffmpeg 解码计时。"
fi
if [[ -n "$MPV_BIN" ]]; then
    echo "mpv: $($MPV_BIN --version 2>/dev/null | head -n 1)"
else
    echo "系统未检测到 mpv —— 跳过 mpv 解码计时。"
fi
if [[ -z "$FFMPEG_BIN" && -z "$MPV_BIN" ]]; then
    echo "两项工具均缺失：未产生任何计时数据，仅输出待填表格框架（退出码 0）。"
fi

# --- 生成测试序列（仅当有 ffmpeg） ----------------------------------------
if [[ -n "$FFMPEG_BIN" ]]; then
    echo "生成测试序列（libx264 veryfast，yuv420p）→ $OUT_DIR/"
    for seq in "${SEQUENCES[@]}"; do
        IFS='|' read -r name source secs _frames _spec <<<"$seq"
        file="$OUT_DIR/$name.mkv"
        if [[ -s "$file" ]]; then
            echo "  复用已存在: $name"
        elif ! "$FFMPEG_BIN" -y -v error -nostdin -f lavfi -i "$source" \
            -t "$secs" -c:v libx264 -preset veryfast -pix_fmt yuv420p "$file"; then
            echo "  生成失败（编码器缺失等），该用例跳过: $name" >&2
            mv -f "$file" "$file.failed" 2>/dev/null || rm -f "$file" 2>/dev/null || true
        fi
    done
fi

# --- 组装用例列表：file|规格|时长|帧数 -------------------------------------
CASES=()
for seq in "${SEQUENCES[@]}"; do
    IFS='|' read -r name _source secs frames spec <<<"$seq"
    file="$OUT_DIR/$name.mkv"
    [[ -s "$file" ]] && CASES+=("$file|$spec|$secs|$frames")
done
if [[ -n "$SAMPLE_DIR" && -d "$SAMPLE_DIR" ]]; then
    count=0
    for sample in "$SAMPLE_DIR"/*; do
        [[ -f "$sample" ]] || continue
        case "${sample,,}" in
            *.mp4|*.mkv|*.mov|*.avi|*.webm|*.m4v|*.ts|*.flv|*.wmv|*.mp3|*.flac|*.m4a|*.wav|*.ogg|*.opus)
                base="$(basename "$sample")"
                CASES+=("$sample|样例媒体 $base|-|-")
                count=$((count + 1))
                [[ "$count" -ge "$MAX_SAMPLES" ]] && break
                ;;
        esac
    done
fi

# --- 计时并写报告 ----------------------------------------------------------
{
    echo "# EmoPlay 解码基准结果"
    echo
    echo "- 生成时间：$(date '+%Y-%m-%d %H:%M:%S %Z')"
    echo "- 运行环境：$(uname -srm)；每用例 ${RUNS} 轮取最优墙钟；软解码（mpv --hwdec=no）。"
    if [[ -n "$FFMPEG_BIN" ]]; then
        echo "- ffmpeg：$($FFMPEG_BIN -version 2>/dev/null | head -n 1)"
    else
        echo "- ffmpeg：未检测到（生成与 ffmpeg 计时全部跳过）"
    fi
    if [[ -n "$MPV_BIN" ]]; then
        echo "- mpv：$($MPV_BIN --version 2>/dev/null | head -n 1)"
    else
        echo "- mpv：未检测到（mpv 计时全部跳过）"
    fi
    echo
    echo "| 用例 | 规格 | 时长(s) | 帧数 | ffmpeg 墙钟(s,最优) | ffmpeg 速率(fps) | mpv 墙钟(s,最优) | mpv 速率(fps) | 备注 |"
    echo "|---|---|---|---|---|---|---|---|---|"

    if [[ ${#CASES[@]} -eq 0 ]]; then
        echo "| （无可用用例：系统缺 ffmpeg 或序列生成失败） | - | - | - | skipped | skipped | skipped | skipped | 待填 |"
    fi

    for case_spec in "${CASES[@]}"; do
        IFS='|' read -r file spec secs frames <<<"$case_spec"
        name="$(basename "$file")"

        ff_ns=""
        if [[ -n "$FFMPEG_BIN" ]]; then
            ff_ns="$(best_time "$FFMPEG_BIN" -v error -nostdin -i "$file" -f null -)"
        fi
        mpv_ns=""
        if [[ -n "$MPV_BIN" ]]; then
            mpv_ns="$(best_time "$MPV_BIN" --vo=null --ao=null --hwdec=no \
                --untimed --really-quiet --no-terminal "$file")"
        fi

        ff_s="skipped（无 ffmpeg）"
        ff_rate="-"
        if [[ -n "$FFMPEG_BIN" ]]; then
            if [[ -n "$ff_ns" ]]; then
                ff_s="$(ns_to_s "$ff_ns")"
                if [[ "$frames" != "-" ]]; then
                    ff_rate="$(awk -v f="$frames" -v s="$ff_ns" \
                        'BEGIN { printf "%.1f", f / (s / 1000000000) }')"
                fi
            else
                ff_s="failed"
            fi
        fi

        mpv_s="skipped（无 mpv）"
        mpv_rate="-"
        if [[ -n "$MPV_BIN" ]]; then
            if [[ -n "$mpv_ns" ]]; then
                mpv_s="$(ns_to_s "$mpv_ns")"
                if [[ "$frames" != "-" ]]; then
                    mpv_rate="$(awk -v f="$frames" -v s="$mpv_ns" \
                        'BEGIN { printf "%.1f", f / (s / 1000000000) }')"
                fi
            else
                mpv_s="failed"
            fi
        fi

        note=""
        [[ -n "$ff_ns" && -n "$mpv_ns" && "$ff_ns" -gt 0 && "$mpv_ns" -gt 0 ]] \
            && note="mpv/ffmpeg = $(awk -v a="$mpv_ns" -v b="$ff_ns" \
                'BEGIN { printf "%.2f", a / b }')"
        echo "| $name | $spec | $secs | $frames | $ff_s | $ff_rate | $mpv_s | $mpv_rate | $note |"
    done

    echo
    echo "> 引用说明：本表由 tools/benchmark.sh 生成（P10 基准框架）。实机跑分后"
    echo "> 把本表复制进 docs/testing-report.md 的性能小节并注明环境（CPU/内存/"
    echo "> 存储/GPU 驱动）；样例媒体行无固定帧数，速率列留空。"
} | tee "$REPORT"

echo
echo "报告已写入 $REPORT"
exit 0
