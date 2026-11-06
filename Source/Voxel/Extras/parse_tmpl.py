"""
解析 UE Voxel 项目的 .tmpl 模板文件，输出结构化信息。
用法: python parse_tmpl.py <file.tmpl> [file2.tmpl ...]
不传文件则扫描 Saved/Templates 目录下所有 .tmpl。
"""
import os
import sys
import struct
import glob
from datetime import datetime, timedelta, timezone

# UE 二进制 archive 是小端
MAGIC = 0x4C505456  # "VTPL"

# UE 的 .tmpl 用 FBufferArchive 序列化 FString
# 实测：UE 5.x 在 binary archive 中默认用 ANSI (1 byte/char) 编码 FString
# （仅当字符串含非 ASCII 字符时才会自动转 UTF-16，并加负数 Num 标记）
TCHAR_SIZE = 1


class Reader:
    def __init__(self, data: bytes):
        self.buf = data
        self.pos = 0

    def remain(self) -> int:
        return len(self.buf) - self.pos

    def read(self, n: int) -> bytes:
        if self.pos + n > len(self.buf):
            raise EOFError(f"need {n} bytes at offset {self.pos}, only {self.remain()} left")
        chunk = self.buf[self.pos:self.pos + n]
        self.pos += n
        return chunk

    def u32(self) -> int:
        return struct.unpack("<I", self.read(4))[0]

    def i32(self) -> int:
        return struct.unpack("<i", self.read(4))[0]

    def i64(self) -> int:
        return struct.unpack("<q", self.read(8))[0]

    def u8(self) -> int:
        return self.read(1)[0]

    def guid(self) -> str:
        a, b, c, d = struct.unpack("<IIII", self.read(16))
        return f"{a:08X}{b:08X}{c:08X}{d:08X}"

    def int_vector(self):
        x, y, z = struct.unpack("<iii", self.read(12))
        return (x, y, z)

    def fstring(self) -> str:
        """UE FString binary 序列化：int32 Num(含 null) + Num*TCHAR bytes
        Num < 0  → 空字符串
        Num == 0 → 不读数据（极少见）
        Num > 0  → 读 Num * TCHAR_SIZE bytes，去掉末尾 null
        """
        num = self.i32()
        if num < 0:
            return ""
        if num == 0:
            return ""
        nbytes = num * TCHAR_SIZE
        raw = self.read(nbytes)
        # ANSI (1 byte/char)，去掉末尾 1 个 null byte
        try:
            text = raw[:-TCHAR_SIZE].decode("utf-8")
        except UnicodeDecodeError:
            # 回退：UTF-16LE (2 bytes/char)
            try:
                text = raw[:-2].decode("utf-16-le")
            except UnicodeDecodeError:
                text = raw[:-TCHAR_SIZE].decode("latin-1", errors="replace")
        return text


def ticks_to_datetime(ticks: int) -> str:
    """UE FDateTime ticks: 100ns 间隔，从 0001-01-01 起。"""
    try:
        # .NET epoch: 0001-01-01 00:00:00 UTC
        epoch = datetime(1, 1, 1, tzinfo=timezone.utc)
        dt = epoch + timedelta(microseconds=ticks / 10)
        return dt.astimezone().strftime("%Y-%m-%d %H:%M:%S")
    except (OverflowError, OSError):
        return f"ticks={ticks}"


def parse_file(path: str) -> None:
    print("=" * 80)
    print(f"File: {path}")
    print(f"Size: {os.path.getsize(path)} bytes")
    with open(path, "rb") as f:
        data = f.read()

    r = Reader(data)
    try:
        magic = r.u32()
        if magic != MAGIC:
            print(f"  ! Magic mismatch: 0x{magic:08X} (expected 0x{MAGIC:08X})")
            return
        print(f"  Magic: VTPL OK")

        fmt_ver = r.fstring()
        guid = r.guid()
        version = r.i32()
        name = r.fstring()
        desc = r.fstring()
        author = r.fstring()
        tag = r.fstring()
        size = r.int_vector()
        anchor = r.int_vector()

        print(f"  FormatVersion: {fmt_ver}")
        print(f"  TemplateGuid: {guid}")
        print(f"  Version: {version}")
        print(f"  Name: {name}")
        print(f"  Description: {desc}")
        print(f"  Author: {author}")
        print(f"  ItemTag: {tag}")
        print(f"  Size: {size}")
        print(f"  Anchor: {anchor}")

        entry_count = r.i32()
        print(f"  EntryCount: {entry_count}")

        # v1.1+：Entry 持久化用 FullId（FString）代替 ItemId（int32）
        # 统计方块/家具分布
        full_id_counter = {}  # FullId -> 数量
        direction_counter = {}  # Dir -> 数量
        relpos_min = None
        relpos_max = None
        entries_preview = []

        for i in range(entry_count):
            pos = r.int_vector()
            entry_full_id = r.fstring()
            direction = r.u8()
            label = r.fstring()

            full_id_counter[entry_full_id] = full_id_counter.get(entry_full_id, 0) + 1
            direction_counter[direction] = direction_counter.get(direction, 0) + 1

            if relpos_min is None:
                relpos_min = list(pos)
                relpos_max = list(pos)
            else:
                for k in range(3):
                    if pos[k] < relpos_min[k]:
                        relpos_min[k] = pos[k]
                    if pos[k] > relpos_max[k]:
                        relpos_max[k] = pos[k]

            # 前 5 个 entry 详细打印
            if i < 5:
                entries_preview.append((i, pos, entry_full_id, direction, label))

        print(f"\n  --- 前 5 个 Entry ---")
        for i, pos, fid, dir, label in entries_preview:
            print(f"    [{i}] Pos={pos} FullId='{fid}' Dir={dir} Label='{label}'")

        print(f"\n  --- 统计 ---")
        print(f"  RelPos 范围: Min={tuple(relpos_min) if relpos_min else None}, "
              f"Max={tuple(relpos_max) if relpos_max else None}")
        print(f"  FullId 分布 (按数量降序):")
        for fid, cnt in sorted(full_id_counter.items(), key=lambda x: -x[1]):
            print(f"    FullId='{fid}'  数量={cnt}")

        print(f"  Direction 分布:")
        dir_names = {0: "Forward", 1: "Right", 2: "Backward", 3: "Left"}
        for d, cnt in sorted(direction_counter.items()):
            print(f"    Dir={d} ({dir_names.get(d, '?')}): {cnt}")

        tag_count = r.i32()
        print(f"\n  TagCount: {tag_count}")
        for i in range(tag_count):
            t = r.fstring()
            print(f"    Tag[{i}]: {t}")

        created = r.i64()
        modified = r.i64()
        print(f"\n  CreatedTime: {ticks_to_datetime(created)} (ticks={created})")
        print(f"  ModifiedTime: {ticks_to_datetime(modified)} (ticks={modified})")

        remaining = r.remain()
        print(f"\n  解析后剩余字节: {remaining}")
        if remaining > 0:
            print(f"    剩余内容(hex): {r.read(min(remaining, 64)).hex()}")

    except EOFError as e:
        print(f"  ! 解析失败: {e}")
    except Exception as e:
        print(f"  ! 异常: {type(e).__name__}: {e}")


def main():
    if len(sys.argv) > 1:
        files = sys.argv[1:]
    else:
        # 默认扫描 Saved/Templates
        script_dir = os.path.dirname(os.path.abspath(__file__))
        # 脚本放在 Source/Voxel/Extras/ 下，往上一级是 Source/Voxel，再往上一级是 Source，再上一级是项目根
        # 但用户可能放任意位置，这里多找几个候选
        candidates = [
            os.path.join(script_dir, "..", "..", "..", "Saved", "Templates"),
            os.path.join(script_dir, "..", "..", "Saved", "Templates"),
            os.path.join(script_dir, "..", "Saved", "Templates"),
            os.path.join("f:\\UEProject\\Voxel\\Saved\\Templates"),
        ]
        files = []
        for c in candidates:
            c = os.path.normpath(c)
            if os.path.isdir(c):
                files = sorted(glob.glob(os.path.join(c, "*.tmpl")))
                print(f"(扫描目录: {c}, 找到 {len(files)} 个 .tmpl 文件)")
                break
        if not files:
            print("未指定文件且未找到 Saved/Templates 目录")
            return

    for f in files:
        parse_file(f)


if __name__ == "__main__":
    main()
