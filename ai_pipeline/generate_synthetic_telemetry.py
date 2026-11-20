"""
Synthetic Household Power & Grid Instability Telemetry Generator for Bijli-Sync-AI.

Generates realistic minute-level electrical telemetry modeled on urban Dhaka
residential load profiles (peak evening cooling/appliance demand, grid voltage
sags during feeder overload, and prepaid balance depletion).
"""

import numpy as np
import pandas as pd
from datetime import datetime, timedelta


def generate_dataset(num_days: int = 14, freq_minutes: int = 5, seed: int = 42) -> pd.DataFrame:
    rng = np.random.default_rng(seed)
    steps = int((num_days * 24 * 60) / freq_minutes)
    start_time = datetime(2026, 5, 1, 0, 0, 0)

    timestamps = [start_time + timedelta(minutes=i * freq_minutes) for i in range(steps)]
    hours = np.array([ts.hour + ts.minute / 60.0 for ts in timestamps])

    # Diurnal load curve: morning bump (8-10 AM) + heavy evening peak (19-23 PM)
    base_load = 220.0 + 90.0 * np.sin((hours - 6.0) * np.pi / 12.0) ** 2
    evening_peak = 950.0 * np.exp(-0.5 * ((hours - 21.0) / 1.8) ** 2)
    ac_cycling = rng.choice([0.0, 1150.0], size=steps, p=[0.72, 0.28]) * (
        (hours >= 13.0) & (hours <= 23.5)
    )

    power_w = np.clip(base_load + evening_peak + ac_cycling + rng.normal(0, 45, steps), 85.0, 3400.0)

    # Grid voltage inversely correlated with neighborhood peak feeder stress + random sags
    feeder_stress = 11.0 * np.exp(-0.5 * ((hours - 20.5) / 2.2) ** 2)
    sag_events = rng.choice([0.0, -24.0, -35.0], size=steps, p=[0.93, 0.05, 0.02])
    voltage_v = np.clip(226.0 - feeder_stress + sag_events + rng.normal(0, 2.2, steps), 172.0, 242.0)

    power_factor = np.clip(0.96 - 0.08 * (ac_cycling > 0) + rng.normal(0, 0.015, steps), 0.72, 0.99)
    current_a = power_w / (voltage_v * power_factor)
    frequency_hz = np.clip(50.0 + rng.normal(0, 0.08, steps) - 0.15 * (voltage_v < 195.0), 48.8, 50.5)

    # Cumulative energy & prepaid balance simulation
    delta_kwh = (power_w / 1000.0) * (freq_minutes / 60.0)
    monthly_kwh = np.cumsum(delta_kwh)
    balance_bdt = np.maximum(0.0, 1800.0 - monthly_kwh * 8.25)

    df = pd.DataFrame({
        "timestamp": timestamps,
        "voltage_v": np.round(voltage_v, 2),
        "current_a": np.round(current_a, 3),
        "power_w": np.round(power_w, 2),
        "frequency_hz": np.round(frequency_hz, 2),
        "power_factor": np.round(power_factor, 3),
        "monthly_kwh": np.round(monthly_kwh, 4),
        "balance_bdt": np.round(balance_bdt, 2),
    })

    # Engineer rolling time-series features
    df["hour"] = [ts.hour for ts in timestamps]
    df["voltage_roll_mean_3"] = df["voltage_v"].rolling(3, min_periods=1).mean()
    df["voltage_drop_rate"] = df["voltage_v"].diff().fillna(0.0)
    df["power_roll_mean_3"] = df["power_w"].rolling(3, min_periods=1).mean()

    # Target 1: Next-step Load Forecast (W)
    df["target_next_power_w"] = df["power_w"].shift(-1).bfill()

    # Target 2: Imminent Blackout / Cutoff Risk (1 if severe sag < 194V, voltage drop > 12V, or balance < 100 BDT)
    next_voltage = df["voltage_v"].shift(-1).bfill()
    df["blackout_risk"] = (
        (next_voltage < 194.0)
        | (df["voltage_drop_rate"] < -12.0)
        | (df["balance_bdt"] < 100.0)
    ).astype(int)

    return df


if __name__ == "__main__":
    dataset = generate_dataset()
    output_path = "benchmark_telemetry.csv"
    dataset.to_csv(output_path, index=False)
    print(f"Generated {len(dataset)} telemetry rows -> {output_path}")
    print(f"Blackout/Cutoff risk positive rate: {dataset['blackout_risk'].mean():.2%}")
