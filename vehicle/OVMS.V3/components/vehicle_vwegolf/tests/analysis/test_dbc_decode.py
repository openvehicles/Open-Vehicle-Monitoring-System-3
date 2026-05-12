"""Regression tests: vwegolf.dbc decodes match per-capture ground truth.

Each test pins a numeric assertion from a `tests/candumps/*.md` annotation.
If a DBC edit silently breaks a signal that previously worked, a test here
fails and tells you which capture-driven claim no longer holds.

Run with:
    .venv/bin/pytest vehicle/OVMS.V3/components/vehicle_vwegolf/tests/analysis/

Ground truth references live in each capture's `.md` file. When adding a
new assertion, cite the .md section in the test docstring so future-you
can find the line of evidence without re-running the capture.
"""

from __future__ import annotations

import os
from statistics import mean

import pytest

from crtd import load, load_dbc

HERE = os.path.dirname(os.path.abspath(__file__))
CANDUMPS = os.path.normpath(os.path.join(HERE, "..", "candumps"))

# Cap 17b — short B-mode drive, 30 km/h cruise. FCAN single-bus.
CAP_DRIVE_30KMH = os.path.join(
    CANDUMPS,
    "can2-3.3.005-800-gc60d79ed-dirty_ota_1_edge-20260412-201238.crtd")

# Cap 21 — Hälla→OKQ8 drive. "all-bus" mode that only emitted FCAN.
CAP_DRIVE_LONG = os.path.join(
    CANDUMPS,
    "all-dc583be4a-dirty_ota_0_edge-20260503-143106.crtd")


@pytest.fixture(scope="module")
def dbc():
    return load_dbc()


@pytest.fixture(scope="module")
def cap_30kmh():
    return load(CAP_DRIVE_30KMH, bus=2)


@pytest.fixture(scope="module")
def cap_long():
    return load(CAP_DRIVE_LONG, bus=2)


# ---------------------------------------------------------------------------
# WheelSpeeds_0B2 — confirms 1/128 km/h per bit scale across all four pairs.
# Ground truth: 201238.md "Key decode results / 0x0B2".
# ---------------------------------------------------------------------------

WHEEL_SIGS = ("WheelSpeedFL", "WheelSpeedFR", "WheelSpeedRL", "WheelSpeedRR")


def test_wheelspeed_zero_at_rest(dbc, cap_30kmh):
    """0.0–1.5 s stationary phase: all four wheels decode to 0.0 km/h."""
    for sig in WHEEL_SIGS:
        rest = [v for t, v in cap_30kmh.decode(dbc, "0B2", sig)
                if 0.0 <= t <= 1.5]
        assert rest, f"no {sig} samples in rest window"
        assert all(v == 0.0 for v in rest), \
            f"{sig} not zero at rest: {set(rest)}"


def test_wheelspeed_30kmh_cruise_plateau(dbc, cap_30kmh):
    """14–17 s plateau decodes to ~30 km/h on all four pairs (.md target)."""
    means = {}
    for sig in WHEEL_SIGS:
        plateau = [v for t, v in cap_30kmh.decode(dbc, "0B2", sig)
                   if 14.0 <= t <= 17.0]
        assert len(plateau) > 20, f"{sig} thin plateau ({len(plateau)} pts)"
        means[sig] = mean(plateau)
        # .md observed 30.05; allow ±1.5 for the wider plateau window.
        assert 28.5 <= means[sig] <= 31.5, \
            f"{sig} plateau {means[sig]:.2f} km/h not ~30"
    # All four wheels within ±0.5 km/h of each other (.md says ±0.2 at the
    # exact plateau row; widen for the 14-17 s averaged window).
    spread = max(means.values()) - min(means.values())
    assert spread < 0.5, f"wheel spread {spread:.2f} km/h too high: {means}"


# ---------------------------------------------------------------------------
# GearSelector_187 — VAL_ table maps raw nibble to gear name.
# Ground truth: 201238.md "Gear timeline" → B-mode (raw 6) throughout 0–26.1 s.
# ---------------------------------------------------------------------------

def test_gear_b_mode_throughout_drive(dbc, cap_30kmh):
    """Whole drive (0–26 s) decodes to GearPosition='B_mode' (raw 6)."""
    gears = cap_30kmh.decode(dbc, "187", "GearPosition")
    motion = [str(g) for t, g in gears if 0.0 <= t <= 26.0]
    assert motion, "no GearSelector frames in motion window"
    assert set(motion) == {"B_mode"}, \
        f"unexpected gears: {set(motion)}"


def test_gear_full_trip_envelope(dbc, cap_long):
    """Cap 21 .md trip envelope: P→R→N→D→B at ~5.8 s, then N→R→P at ~583 s."""
    gears = cap_long.decode(dbc, "187", "GearPosition")
    seen = {str(g) for _, g in gears}
    # All five real gears must appear somewhere in the trip
    assert {"Park", "Reverse", "Neutral", "Drive", "B_mode"} <= seen, \
        f"missing gears: {seen}"


# ---------------------------------------------------------------------------
# BMS_SoC_131 — factor 0.5 %/LSB, raw 0xFE = startup sentinel (filter).
# Ground truth: 143106.md "0x131 BMS_SoC" → 99.0% → 95.0% monotonic-ish.
# ---------------------------------------------------------------------------

def test_soc_range_during_drive(dbc, cap_long):
    """SoC decodes within 95–99 % across the trip (.md: 99→95)."""
    samples = cap_long.decode(dbc, "131", "SoC")
    # Filter the 0xFE startup sentinel (127.0 %) AND fault-state 0 %.
    valid = [(t, v) for t, v in samples if 50.0 < v < 127.0]
    assert len(valid) > 1000, f"too few valid SoC: {len(valid)}"
    vals = [v for _, v in valid]
    assert 94.5 <= min(vals) <= 95.5, f"min SoC {min(vals)} not ~95"
    assert 98.5 <= max(vals) <= 99.5, f"max SoC {max(vals)} not ~99"


def test_soc_decreases_over_trip(dbc, cap_long):
    """Net trip consumption: SoC at start > SoC at end."""
    samples = cap_long.decode(dbc, "131", "SoC")
    valid = [(t, v) for t, v in samples if 50.0 < v < 127.0]
    early = mean(v for t, v in valid if t < 30.0)
    late = mean(v for t, v in valid if t > 550.0)
    assert early - late >= 3.5, \
        f"SoC drop {early - late:.2f} pp too small (expected ~4 pp)"


# ---------------------------------------------------------------------------
# BMS_PowerBus_191 — BatCurrent + BatVoltage.
# Ground truth: 143106.md "0x191 BMS_PowerBus":
#   current range -307 A draw / +63 A regen
#   voltage range 343.0 V / 360.8 V (excluding sentinels)
#   power range -105.5 kW peak draw / +22.5 kW peak regen
#
# DBC comment claims "positive = discharge"; observed data contradicts it
# (peak draw is negative current at sag voltage). Test asserts what the
# decode physically produces — fix the DBC comment separately.
# ---------------------------------------------------------------------------

def _filter_191(records):
    """Drop sentinel/idle frames where decode hits the rail."""
    # Voltage sentinel: raw 0xFFF = 1023.75 V. Current rail: raw 0xFFF = 2047 A.
    return [(t, v) for t, v in records if abs(v) < 800]


def test_battery_current_range(dbc, cap_long):
    """Filtered BatCurrent stays within physical envelope (~-310..+65 A)."""
    cur = _filter_191(cap_long.decode(dbc, "191", "BatCurrent"))
    assert cur, "no valid BatCurrent samples"
    vals = [v for _, v in cur]
    # .md: -307 / +63. Allow ±20 A noise band.
    assert -320 <= min(vals) <= -280, f"draw peak {min(vals)} A off"
    assert 40 <= max(vals) <= 80, f"regen peak {max(vals)} A off"


def test_battery_voltage_range(dbc, cap_long):
    """Filtered BatVoltage stays within HV pack envelope (~340..365 V)."""
    volt = _filter_191(cap_long.decode(dbc, "191", "BatVoltage"))
    assert volt, "no valid BatVoltage samples"
    vals = [v for _, v in volt]
    assert 340 <= min(vals) <= 350, f"min V {min(vals)} off"
    assert 355 <= max(vals) <= 370, f"max V {max(vals)} off"


def test_battery_peak_power(dbc, cap_long):
    """Per-frame P = I*V matches .md peaks within ±10 kW.

    DBC raw convention: discharge current is negative on this pack, so peak
    draw is the most-negative power and peak regen the most-positive. .md
    quotes the OVMS-firmware-negated user-facing sign (-105.5 kW draw).
    """
    cur = dict(_filter_191(cap_long.decode(dbc, "191", "BatCurrent")))
    volt = dict(_filter_191(cap_long.decode(dbc, "191", "BatVoltage")))
    # BatCurrent and BatVoltage are signals from the same frame, so
    # timestamps align exactly. Compute power on the intersection.
    common = set(cur) & set(volt)
    assert len(common) > 1000, f"too few common samples: {len(common)}"
    powers = [cur[t] * volt[t] / 1000.0 for t in common]
    peak_draw = min(powers)   # most negative
    peak_regen = max(powers)  # most positive
    assert -115 <= peak_draw <= -95, f"peak draw {peak_draw:.1f} kW off"
    assert 15 <= peak_regen <= 30, f"peak regen {peak_regen:.1f} kW off"
