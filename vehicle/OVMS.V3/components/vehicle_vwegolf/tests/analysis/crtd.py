"""Shared parser + analysis helpers for OVMS .crtd capture files.

Use from ad-hoc analysis scripts to keep them short and consistent.

Quick start:
    from crtd import load, byte_stats, trajectory_le16, le16
    cap = load("tests/candumps/can2-…-201238.crtd")    # single-bus file: auto
    cap = load("tests/candumps/multi.crtd", bus=3)      # multi-bus: specify
    print(cap.frame_count, len(cap.by_id), "IDs on bus", cap.bus)
    for t, b in cap.by_id["0B2"]:
        print(t, le16(b, 0))

DBC decode (uses cantools + docs/vwegolf.dbc as ground truth):
    from crtd import load, load_dbc
    cap = load("tests/candumps/can2-…-201238.crtd")
    dbc = load_dbc()                                # auto-finds docs/vwegolf.dbc
    for t, kmh in cap.decode(dbc, "0B2", "WheelSpeedFL"):
        print(t, kmh)

CRTD line format:
    <unix_ts> <N>R<W> <hex_id> <b0> <b1> ... <bN>
        N = bus number (1, 2, 3)
        W = ID width: 11 (standard) or 29 (extended)
    <unix_ts> CXX  …                                    # comments / metadata
    <unix_ts> <N>CER <flags>                            # CAN error records

Both 11-bit and 29-bit data frames are parsed. Error records are skipped.
"""

from __future__ import annotations

import os
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Any, Iterable, Iterator


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

    def decode(self, dbc, frame_id, signal: str | None = None,
               decode_choices: bool = True) -> list[tuple[float, Any]]:
        """Decode every frame matching `frame_id` using `dbc`.

        `frame_id` accepts hex string ("0B2", "0x131", "1A5554A8") or int.
        Returns `[(t, value), ...]` if `signal` is given, else `[(t, {sig: val, ...}), ...]`.
        Frames shorter than the DBC message length, or that raise during
        decode (sentinels, malformed), are silently skipped — same policy as
        the rest of this module.

        `decode_choices=False` returns raw numeric values for VAL_ enum signals.
        """
        hex_id, int_id = _normalize_frame_id(frame_id)
        msg = dbc.get_message_by_frame_id(int_id)
        out: list[tuple[float, Any]] = []
        for t, b in self.by_id.get(hex_id, []):
            if len(b) < msg.length:
                continue
            try:
                d = msg.decode(bytes(b[:msg.length]),
                               decode_choices=decode_choices)
            except Exception:
                continue
            out.append((t, d if signal is None else d[signal]))
        return out


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
# Edge / transition detection
# ---------------------------------------------------------------------------
#
# Used everywhere in `scratch/` for "find timestamps where byte X changed",
# either to pin a stimulus (`port 0x12 0x05 → 0x00`) or to feed into
# correlation / diff helpers. mask= and shift= let you isolate a nibble or
# a single bit without copying the records first.
# ---------------------------------------------------------------------------

def transitions(records: Iterable[tuple[float, list[int]]],
                off: int,
                mask: int = 0xFF,
                shift: int = 0) -> list[tuple[float, int, int]]:
    """Return `[(t, prev, curr), ...]` whenever `(b[off] & mask) >> shift` changes.

    Only emits on change. The first frame seeds the prior value silently;
    if you also want it in the output, prepend manually. Short frames
    (`len(b) <= off`) are skipped. Stable use cases:

        transitions(cap.by_id["12"], 0)                 # port byte 0
        transitions(cap.by_id["0B4"], 3, 0x0F)          # low nibble
        transitions(cap.by_id["1A5554A8"], 0, 0xF0, 4)  # mux selector
    """
    out: list[tuple[float, int, int]] = []
    prev: int | None = None
    for t, b in records:
        if len(b) <= off:
            continue
        v = (b[off] & mask) >> shift
        if prev is not None and v != prev:
            out.append((t, prev, v))
        prev = v
    return out


# ---------------------------------------------------------------------------
# Frame liveness / rate
# ---------------------------------------------------------------------------
#
# Catches "is this ID actually live in this window?" without rolling the
# arithmetic per script. `0x191 silent during CCS DC` (Capture 20) and
# stale-rebroadcast hazards both show up here as long gaps relative to
# the expected period.
# ---------------------------------------------------------------------------

@dataclass
class FrameRate:
    count: int
    span_s: float          # time from first frame to last
    mean_hz: float
    median_gap_s: float
    max_gap_s: float
    long_gaps: list[tuple[float, float, float]] = field(default_factory=list)
    # (gap_start_t, gap_end_t, gap_duration_s) for gaps > threshold * median


def frame_rate(records: list[tuple[float, list[int]]],
               long_gap_ratio: float = 5.0) -> FrameRate:
    """Compute rate stats for one ID's records.

    `long_gap_ratio`: gaps wider than `ratio * median_gap` are flagged as
    suspicious (bus dropout, ECU sleep, rebroadcast pause).
    """
    n = len(records)
    if n == 0:
        return FrameRate(0, 0.0, 0.0, 0.0, 0.0)
    if n == 1:
        return FrameRate(1, 0.0, 0.0, 0.0, 0.0)
    times = [t for t, _ in records]
    span = times[-1] - times[0]
    gaps = [times[i] - times[i - 1] for i in range(1, n)]
    sg = sorted(gaps)
    median = sg[len(sg) // 2]
    long: list[tuple[float, float, float]] = []
    if median > 0:
        threshold = long_gap_ratio * median
        for i, g in enumerate(gaps):
            if g > threshold:
                long.append((times[i], times[i + 1], g))
    return FrameRate(
        count=n,
        span_s=span,
        mean_hz=(n - 1) / span if span > 0 else 0.0,
        median_gap_s=median,
        max_gap_s=max(gaps),
        long_gaps=long,
    )


# ---------------------------------------------------------------------------
# Stimulus diff — what changed across an event boundary
# ---------------------------------------------------------------------------
#
# Replaces the pattern in apr7_*/cap_*/ack_check_recent: take an event
# time (BAP send, port toggle, gear shift), slice the capture into pre/post
# windows, compute byte_stats on each side per ID, list which IDs/bytes
# changed character. Output is ranked by "interestingness" (newly-emerged
# IDs first, then IDs with the most-changed-bytes).
# ---------------------------------------------------------------------------

@dataclass
class DiffEntry:
    hex_id: str
    pre: list[ByteStat]
    post: list[ByteStat]
    changed_bytes: list[int]   # indices where (min, max, unique) differ
    emerged: bool              # True if zero frames in pre, >0 in post
    vanished: bool             # True if >0 frames in pre, zero in post


def diff_window(cap: Capture,
                t_event: float,
                pre: float = 2.0,
                post: float = 2.0,
                dlc: int = 8,
                ids: Iterable[str] | None = None) -> list[DiffEntry]:
    """Per-ID byte_stats comparison across an event boundary.

    Returns a list of `DiffEntry` ranked: emerged > vanished > most-changed.
    Window is `[t_event - pre, t_event)` vs `[t_event, t_event + post]`.
    Pass `ids=` to restrict (e.g. only KCAN port frames); else all IDs.
    """
    t_pre0, t_post1 = t_event - pre, t_event + post
    target_ids = set(ids) if ids is not None else set(cap.by_id.keys())
    entries: list[DiffEntry] = []
    for cid in sorted(target_ids):
        recs = cap.by_id.get(cid, [])
        pre_recs = [(t, b) for t, b in recs if t_pre0 <= t < t_event]
        post_recs = [(t, b) for t, b in recs if t_event <= t <= t_post1]
        if not pre_recs and not post_recs:
            continue
        pre_stats = byte_stats(pre_recs, dlc=dlc)
        post_stats = byte_stats(post_recs, dlc=dlc)
        changed: list[int] = []
        for i in range(dlc):
            ps, qs = pre_stats[i], post_stats[i]
            if (ps.min, ps.max, ps.unique) != (qs.min, qs.max, qs.unique):
                changed.append(i)
        emerged = not pre_recs and bool(post_recs)
        vanished = bool(pre_recs) and not post_recs
        if not (changed or emerged or vanished):
            continue
        entries.append(DiffEntry(cid, pre_stats, post_stats,
                                 changed, emerged, vanished))
    # Rank: emerged/vanished first, then most-changed-bytes desc.
    entries.sort(key=lambda e: (
        not (e.emerged or e.vanished),
        -len(e.changed_bytes),
        e.hex_id,
    ))
    return entries


# ---------------------------------------------------------------------------
# Multiplexed frames
# ---------------------------------------------------------------------------
#
# 0x1A5554A8 OBC telemetry is the standing case (b0 upper nibble selects
# mux: 0x5 = telemetry, 0x6 = static header). Generic helper because every
# multiplexed VW frame follows the same pattern.
# ---------------------------------------------------------------------------

def mux_split(records: list[tuple[float, list[int]]],
              off: int = 0,
              mask: int = 0xFF,
              shift: int = 0) -> dict[int, list[tuple[float, list[int]]]]:
    """Group `records` by `(b[off] & mask) >> shift`. Skips short frames.

    For 0x1A5554A8 the mux selector is byte-0 upper nibble:

        mux_split(cap.by_id["1A5554A8"], off=0, mask=0xF0, shift=4)
        → {0x5: [...telemetry frames...], 0x6: [...static header frames...]}
    """
    out: dict[int, list[tuple[float, list[int]]]] = defaultdict(list)
    for t, b in records:
        if len(b) <= off:
            continue
        out[(b[off] & mask) >> shift].append((t, b))
    return dict(out)


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


# ---------------------------------------------------------------------------
# DBC integration (cantools)
# ---------------------------------------------------------------------------
#
# vwegolf.dbc is the single source of truth for frame layouts. Decoding via
# cantools instead of hand-rolled byte arithmetic means analysis scripts and
# regression tests can't drift from the DBC — change the DBC, every consumer
# follows. Loaded with strict=False because ChargeManagement intentionally
# overlaps ChargePort/ChargeType (same bits, different VAL_ tables).
# ---------------------------------------------------------------------------

_DEFAULT_DBC = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "docs", "vwegolf.dbc"))


def default_dbc_path() -> str:
    """Absolute path to the project's `docs/vwegolf.dbc`."""
    return _DEFAULT_DBC


def load_dbc(path: str | None = None):
    """Load a DBC via cantools. Defaults to `docs/vwegolf.dbc`.

    Raises `ImportError` with a hint if cantools isn't installed.
    Uses `strict=False` to allow intentional signal overlap (see module note).
    """
    try:
        import cantools  # noqa: F401  (import for the side effect of a clean error)
        from cantools.database import load_file
    except ImportError as e:
        raise ImportError(
            "cantools is required for DBC decode. Install in .venv: "
            "`.venv/bin/pip install cantools`"
        ) from e
    return load_file(path or _DEFAULT_DBC, strict=False)


def _normalize_frame_id(fid) -> tuple[str, int]:
    """Accept hex str ('0B2', '0x131', '1A5554A8') or int. Returns (hex, int).

    Hex string matches the CRTD on-disk format used as `Capture.by_id` keys:
    3 hex chars zero-padded for 11-bit IDs (<= 0x7FF), 8 chars for extended.
    For extended IDs pass the raw 29-bit value — do NOT set bit 31; cantools
    resolves extended vs standard via message metadata.
    """
    if isinstance(fid, int):
        i = fid
    else:
        s = fid.strip().lower()
        if s.startswith("0x"):
            s = s[2:]
        i = int(s, 16)
    hex_id = f"{i:03X}" if i <= 0x7FF else f"{i:08X}"
    return hex_id, i
