"""Shared parser + analysis helpers for OVMS .crtd capture files.

Use from ad-hoc analysis scripts to keep them short and consistent.

Quick start:
    from crtd import load, byte_stats, trajectory_le16, le16
    cap = load("tests/candumps/can2-…-201238.crtd")    # single-bus file: auto
    cap = load("tests/candumps/multi.crtd", bus=3)      # multi-bus: specify
    print(cap.frame_count, len(cap.by_id), "IDs on bus", cap.bus)
    for t, b in cap.by_id["0B2"]:
        print(t, le16(b, 0))

CRTD line format:
    <unix_ts> <N>R<W> <hex_id> <b0> <b1> ... <bN>
        N = bus number (1, 2, 3)
        W = ID width: 11 (standard) or 29 (extended)
    <unix_ts> CXX  …                                    # comments / metadata
    <unix_ts> <N>CER <flags>                            # CAN error records

Both 11-bit and 29-bit data frames are parsed. Error records are skipped.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass, field
from typing import Iterable, Iterator


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

_VALID_TYPES = {f"{b}R{w}" for b in (1, 2, 3) for w in (11, 29)}


@dataclass
class Capture:
    """Parsed CRTD file: frames grouped by hex CAN ID, time relative to t0.

    A Capture always represents a *single* bus. Use `load_multi` for
    multi-bus files that you need to split apart.
    """
    path: str
    t0_unix: float                                  # absolute time of first frame
    bus: int                                        # 1, 2, or 3
    buses_seen: set[int] = field(default_factory=set)   # all buses present in the source file
    by_id: dict[str, list[tuple[float, list[int]]]] = field(default_factory=dict)

    @property
    def frame_count(self) -> int:
        return sum(len(v) for v in self.by_id.values())

    @property
    def duration(self) -> float:
        last = 0.0
        for recs in self.by_id.values():
            if recs and recs[-1][0] > last:
                last = recs[-1][0]
        return last


def iter_frames(path: str) -> Iterator[tuple[float, int, str, list[int]]]:
    """Yield (rel_t, bus, can_id_hex, data_bytes) for every data frame.

    Handles both 11-bit and 29-bit IDs on any of the three buses.
    `rel_t` is seconds from the first data frame in the file.
    """
    t0: float | None = None
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 3:
                continue
            typ = parts[1]
            if typ not in _VALID_TYPES:
                continue
            try:
                ts = float(parts[0])
            except ValueError:
                continue
            try:
                data = [int(b, 16) for b in parts[3:11]]
            except ValueError:
                continue
            if t0 is None:
                t0 = ts
            yield (ts - t0, int(typ[0]), parts[2].upper(), data)


def iter_crtd(path: str) -> Iterator[tuple[float, str, list[int]]]:
    """Backward-compat stream: (rel_t, can_id_hex, data_bytes), all buses merged.

    Prefer `iter_frames` in new code — it exposes the bus number.
    """
    for t, _bus, cid, data in iter_frames(path):
        yield (t, cid, data)


def scan_buses(path: str) -> set[int]:
    """Return the set of bus numbers present in a capture file."""
    seen: set[int] = set()
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 3:
                continue
            typ = parts[1]
            if typ in _VALID_TYPES:
                seen.add(int(typ[0]))
    return seen


def load(path: str, bus: int | None = None) -> Capture:
    """Read a CRTD file into a Capture for a single bus.

    - Single-bus file + `bus=None` → auto-pick that bus.
    - Multi-bus file + `bus=None` → ValueError (force caller to be explicit).
    - `bus=N` → filter to bus N (even if only bus present; cheap sanity check).

    Use `load_multi` when you want every bus at once.
    """
    by_id: dict[str, list[tuple[float, list[int]]]] = defaultdict(list)
    t0_unix = 0.0
    first = True
    buses_seen: set[int] = set()

    with open(path) as f:
        lines = f.readlines()

    # First pass: discover buses (cheap — just parts[1]).
    for line in lines:
        parts = line.split()
        if len(parts) >= 2 and parts[1] in _VALID_TYPES:
            buses_seen.add(int(parts[1][0]))

    if not buses_seen:
        raise ValueError(f"{path}: no data frames found")

    if bus is None:
        if len(buses_seen) > 1:
            raise ValueError(
                f"{path}: multi-bus capture (buses {sorted(buses_seen)}); "
                f"pass bus=N or use load_multi()"
            )
        bus = next(iter(buses_seen))
    elif bus not in buses_seen:
        raise ValueError(
            f"{path}: bus {bus} not present (buses seen: {sorted(buses_seen)})"
        )

    wanted_prefix = str(bus)
    for line in lines:
        parts = line.split()
        if len(parts) < 3:
            continue
        typ = parts[1]
        if typ not in _VALID_TYPES or typ[0] != wanted_prefix:
            continue
        try:
            ts = float(parts[0])
        except ValueError:
            continue
        try:
            data = [int(b, 16) for b in parts[3:11]]
        except ValueError:
            continue
        if first:
            t0_unix = ts
            first = False
        by_id[parts[2].upper()].append((ts - t0_unix, data))

    return Capture(
        path=path,
        t0_unix=t0_unix,
        bus=bus,
        buses_seen=buses_seen,
        by_id=dict(by_id),
    )


def load_multi(path: str) -> dict[int, Capture]:
    """Read a CRTD file and return one Capture per bus present.

    Each Capture has its own `t0_unix` (first frame on *that* bus). If you
    need a shared zero across buses, re-base timestamps yourself.
    """
    buses = scan_buses(path)
    if not buses:
        raise ValueError(f"{path}: no data frames found")
    return {b: load(path, bus=b) for b in sorted(buses)}


# ---------------------------------------------------------------------------
# Byte / pair extraction
# ---------------------------------------------------------------------------

def le16(b: list[int], off: int) -> int:
    """Little-endian 16-bit at byte offset `off`."""
    return b[off] | (b[off + 1] << 8)


def be16(b: list[int], off: int) -> int:
    """Big-endian 16-bit at byte offset `off`."""
    return (b[off] << 8) | b[off + 1]


def trajectory(records: Iterable[tuple[float, list[int]]],
               extractor) -> list[tuple[float, int]]:
    """Apply `extractor(bytes)` to each record. Returns [(t, value), ...]."""
    return [(t, extractor(b)) for t, b in records]


def trajectory_byte(records, off: int) -> list[tuple[float, int]]:
    return [(t, b[off]) for t, b in records if len(b) > off]


def trajectory_le16(records, off: int) -> list[tuple[float, int]]:
    return [(t, le16(b, off)) for t, b in records if len(b) > off + 1]


def trajectory_be16(records, off: int) -> list[tuple[float, int]]:
    return [(t, be16(b, off)) for t, b in records if len(b) > off + 1]


# ---------------------------------------------------------------------------
# Stats
# ---------------------------------------------------------------------------

@dataclass
class ByteStat:
    min: int | None
    max: int | None
    unique: int

    @property
    def static(self) -> bool:
        return self.unique == 1

    @property
    def varying(self) -> bool:
        return self.unique > 1


def byte_stats(records: Iterable[tuple[float, list[int]]],
               dlc: int = 8) -> list[ByteStat]:
    """Per-byte (min, max, unique) across the records. Skips short frames."""
    cols: list[list[int]] = [[] for _ in range(dlc)]
    for _, b in records:
        for i in range(min(dlc, len(b))):
            cols[i].append(b[i])
    out: list[ByteStat] = []
    for c in cols:
        if not c:
            out.append(ByteStat(None, None, 0))
        else:
            out.append(ByteStat(min(c), max(c), len(set(c))))
    return out


# ---------------------------------------------------------------------------
# Sampling / display
# ---------------------------------------------------------------------------

def downsample(seq: list, n: int = 25) -> list:
    """Even-stride downsample preserving order. Returns first `n` strides."""
    if len(seq) <= n:
        return list(seq)
    step = max(1, len(seq) // n)
    return list(seq[::step][:n])


def hexdump_window(records, dlc: int = 8, n: int = 25) -> str:
    """Pretty multi-line dump: t  b0 b1 .. b{dlc-1}. Downsampled to ~n rows."""
    rows = downsample(records, n)
    head = "    t/s  " + "  ".join(f"b{i}" for i in range(dlc))
    lines = [head]
    for t, b in rows:
        lines.append(f"  {t:5.1f} " + " ".join(f"{x:3d}" for x in b[:dlc]))
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# VW e-Golf specific helpers (live here because every drive-state analysis
# needs the gear timeline; cheap and small enough to ship in the parser)
# ---------------------------------------------------------------------------

GEAR_NAMES = {1: "?1", 2: "P", 3: "R", 4: "N", 5: "D", 6: "B"}


def gear_timeline(cap: Capture) -> list[tuple[float, int]]:
    """0x187 byte 2 low nibble — confirmed gear position from Cap 15."""
    return [(t, b[2] & 0x0F) for t, b in cap.by_id.get("187", [])]


def gear_windows(timeline: list[tuple[float, int]]
                 ) -> dict[int, list[tuple[float, float]]]:
    """Collapse a gear timeline into per-gear (start, end) windows."""
    if not timeline:
        return {}
    wins: dict[int, list[tuple[float, float]]] = defaultdict(list)
    cur_g = timeline[0][1]
    cur_start = timeline[0][0]
    last_t = timeline[0][0]
    for t, g in timeline[1:]:
        if g != cur_g:
            wins[cur_g].append((cur_start, last_t))
            cur_g = g
            cur_start = t
        last_t = t
    wins[cur_g].append((cur_start, last_t))
    return dict(wins)


def in_window(t: float, windows: list[tuple[float, float]]) -> bool:
    return any(s <= t <= e for s, e in windows)
