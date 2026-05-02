"""
bus_provenance.py — for each CAN ID in a CRTD file, show which physical bus(es)
it was recorded on.  Reveals IDs mislabeled by the old firmware routing bug
(FCAN frames appearing as bus-3 records in KCAN captures).

Usage:
    python bus_provenance.py <file.crtd> [<file2.crtd> ...]

Flags:
  --compare   Side-by-side: given exactly two files, print IDs unique to each
              and IDs that appear on the same bus in both (good) or different
              buses (suspicious).
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from crtd import iter_frames

def provenance(path: str) -> dict[str, set[int]]:
    """Return {can_id_hex: set_of_bus_numbers} for every frame in the file."""
    result: dict[str, set[int]] = {}
    for rel_t, bus, can_id, data in iter_frames(path):
        result.setdefault(can_id, set()).add(bus)
    return result

def summarize(path: str, prov: dict[str, set[int]]) -> None:
    name = os.path.basename(path)
    buses = sorted({b for buses in prov.values() for b in buses})
    print(f"\n{name}")
    print(f"  buses in file: {buses}  unique IDs: {len(prov)}")

    # IDs that appear on more than one bus — the smoking gun
    multi = {cid: sorted(bs) for cid, bs in prov.items() if len(bs) > 1}
    if multi:
        print(f"  *** IDs on MULTIPLE buses (bridging / routing bug?) ***")
        for cid, bs in sorted(multi.items()):
            print(f"    0x{cid}: buses {bs}")
    else:
        print(f"  OK: every ID on exactly one bus")

    # Per-bus breakdown
    for b in buses:
        ids = sorted(cid for cid, bs in prov.items() if b in bs)
        ext = [cid for cid in ids if len(cid) > 3]
        std = [cid for cid in ids if len(cid) <= 3]
        print(f"  bus {b} ({len(ids)} IDs):")
        print(f"    std: {' '.join(std)}")
        if ext:
            print(f"    ext: {' '.join(ext)}")

def compare(path_a: str, path_b: str) -> None:
    pa = provenance(path_a)
    pb = provenance(path_b)
    na, nb = os.path.basename(path_a), os.path.basename(path_b)

    only_a  = sorted(k for k in pa if k not in pb)
    only_b  = sorted(k for k in pb if k not in pa)
    in_both = sorted(k for k in pa if k in pb)

    print(f"\n=== Compare ===")
    print(f"  A: {na}  ({len(pa)} IDs)")
    print(f"  B: {nb}  ({len(pb)} IDs)")

    print(f"\nOnly in A ({len(only_a)}): {' '.join(only_a) if only_a else '(none)'}")
    print(f"Only in B ({len(only_b)}): {' '.join(only_b) if only_b else '(none)'}")

    if in_both:
        print(f"\nIn both ({len(in_both)}):")
        for cid in in_both:
            ba, bb = sorted(pa[cid]), sorted(pb[cid])
            flag = "" if ba == bb else "  *** BUS MISMATCH ***"
            print(f"  0x{cid}: A=bus{ba} B=bus{bb}{flag}")

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    do_compare = "--compare" in sys.argv

    if not args:
        print(__doc__)
        sys.exit(1)

    provs = {p: provenance(p) for p in args}
    for p, prov in provs.items():
        summarize(p, prov)

    if do_compare and len(args) == 2:
        compare(args[0], args[1])
    elif do_compare and len(args) != 2:
        print("--compare needs exactly 2 files")
