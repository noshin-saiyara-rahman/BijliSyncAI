# Bijli-Sync-AI

**A Human-Centered Edge-AI IoT System for Explainable Household Power Monitoring, Prepaid Tariff Tracking, and Blackout-Risk Prediction**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/Hardware-ESP32%20%7C%20PZEM--004T-00979D.svg)](hardware/WIRING_GUIDE.md)
[![AI: XGBoost + SHAP](https://img.shields.io/badge/AI-XGBoost%20%2B%20SHAP-FF6F00.svg)](ai_pipeline/train_and_explain.py)

---

## Overview

**Bijli-Sync-AI** is an offline-first Edge-AI "Shadow Meter" engineered to solve billing opacity and unexpected power cutoffs in residential prepaid electricity grids (modeled on Bangladesh's DESCO 7-tier progressive residential tariff). Installed non-invasively at the main distribution board via a split-core Current Transformer (CT), the system mirrors utility metering in real time, persists cumulative energy states across power outages using wear-leveled Non-Volatile Storage (NVS), hosts a zero-internet local web dashboard over mDNS (`http://bijlisync.local`), and pairs Gradient Boosted Decision Trees (**XGBoost**) with **SHAP (SHapley Additive exPlanations)** to translate voltage sags and load spikes into plain-language blackout-risk advisories.

> **Academic Context:** Developed as an undergraduate Computer Science & Engineering thesis project at American International University-Bangladesh (AIUB).

---

## Key Features

- **Non-Invasive Sub-Second Metering:** Captures RMS Voltage ($V$), Current ($I$), Active Power ($W$), Cumulative Energy ($kWh$), Line Frequency ($Hz$), and Power Factor ($PF$) via **PZEM-004T v3.0** over isolated UART.
- **Exact 7-Tier Progressive Prepaid Tariff Engine:** Replicates DESCO residential slab rates (`0–75`, `76–200`, `201–300`, `301–400`, `401–600`, and `600+ kWh`) on-device to track real-time balance depletion in BDT.
- **Outage-Resilient Flash Persistence & Logging:** Implements wear-leveled **ESP32 NVS** commits alongside redundant **microSD CSV telemetry logging** so state is preserved during sudden grid blackouts.
- **Zero-Cloud Local Web Dashboard & OLED UI:** Streams live telemetry over **WebSockets** to a responsive local dashboard (`bijlisync.local`) and renders live load/balance metrics on a **1.3-inch I2C OLED** display.
- **Proactive SMTP Low-Balance Alerts:** Dispatches automated email warnings when remaining prepaid credit drops below user-defined BDT thresholds.
- **Explainable Blackout-Risk Forecasting (XGBoost + SHAP):** Forecasts short-horizon household demand and classifies grid instability/cutoff risk, converting top SHAP feature attributions into human-centered recommendations.

---

## Repository Structure

```text
Bijli-Sync-AI/
├── firmware/                  # ESP32 C++ (PlatformIO) firmware & embedded WebSocket UI
│   ├── include/
│   │   └── config.example.h   # Wi-Fi, SMTP, and tariff configuration template
│   ├── src/
│   │   ├── main.cpp           # Metering loop, 7-tier tariff logic, NVS, OLED, mDNS, SMTP
│   │   └── web_dashboard.h    # Self-contained HTML/CSS/JS live telemetry dashboard
│   └── platformio.ini         # Build configuration & library dependencies
├── ai_pipeline/               # Python time-series forecasting & SHAP explainability
│   ├── generate_synthetic_telemetry.py
│   ├── train_and_explain.py
│   ├── sample_telemetry.csv
│   └── requirements.txt
├── hardware/                  # Pinout diagrams, safety guidelines & BOM
│   ├── WIRING_GUIDE.md
│   └── BOM.md
├── docs/                      # Architecture specification & CV summary
│   ├── ARCHITECTURE.md
│   └── CV_SUMMARY.md
├── .gitignore
├── LICENSE
└── README.md
```

---

## Quick Start

### 1. Hardware Assembly
Follow the pin-to-pin connections in [`hardware/WIRING_GUIDE.md`](hardware/WIRING_GUIDE.md) to interface the **ESP32**, **PZEM-004T v3.0**, **1.3" SH1106 I2C OLED**, and **microSD SPI module**.

### 2. ESP32 Firmware Setup (PlatformIO)
```bash
cd firmware
cp include/config.example.h include/config.h
# Edit include/config.h with your Wi-Fi credentials, SMTP settings, and starting BDT balance
pio run --target upload
```
Once booted, open **`http://bijlisync.local`** on any device connected to the same local network to view the live WebSocket telemetry dashboard.

### 3. Explainable AI Pipeline (Python)
```bash
cd ai_pipeline
python -m venv .venv
source .venv/bin/activate        # On Windows: .venv\Scripts\activate
pip install -r requirements.txt

# Generate a benchmark dataset (or supply your own microSD telemetry CSV)
python generate_synthetic_telemetry.py

# Train XGBoost models and generate SHAP plain-language explanations
python train_and_explain.py
```

---

## Citation

If you reference this architecture or codebase in academic work, please cite:

```bibtex
@misc{nadim2026bijlisync,
  author       = {Nadim, Ragib},
  title        = {Bijli-Sync-AI: A Human-Centered AI System for Explainable Power Monitoring and Blackout-Risk Prediction},
  year         = {2026},
  howpublished = {GitHub Repository},
  url          = {https://github.com/RagibNadim/Bijli-Sync-AI}
}
@misc{noshin2026bijlisync,
  author       = {Rahman, Noshin Saiyara}
  title        = {Bijli-Sync-AI: A Human-Centered AI System for Explainable Power Monitoring and Blackout-Risk Prediction},
  year         = {2026},
  howpublished = {GitHub Repository},
  url          = {https://github.com/noshin-saiyara-rahman/BijliSyncAI.git}
}
```
