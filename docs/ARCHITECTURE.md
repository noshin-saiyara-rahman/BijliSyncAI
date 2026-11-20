# System Architecture & Tariff Specification

## 1. Three-Layer Edge-AI Architecture

1. **Non-Invasive Edge Sensing Layer (`PZEM-004T v3.0` + `ESP32`):**
   - Polls electrical parameters at 1 Hz over hardware UART2 (`GPIO 16/17`).
   - Computes incremental energy consumption $\Delta E_t = E_t - E_{t-1}$ (in kWh) while filtering transient sensor spikes.

2. **Offline-First Billing & State Persistence Layer:**
   - **Wear-Leveled NVS:** Commits cumulative monthly kWh and remaining prepaid balance (BDT) to ESP32 Non-Volatile Storage (`Preferences`) only when $\Delta E \ge 0.05\text{ kWh}$ or 10 minutes have elapsed, preventing flash wearout while bounding worst-case blackout state loss to $< 0.45\text{ BDT}$.
   - **Zero-Cloud Local Web Server:** Advertises `bijlisync.local` via mDNS and pushes JSON telemetry frames over WebSockets (`/ws`) to connected smartphones/laptops without requiring internet connectivity.
   - **Dual-Redundant Logging:** Appends 1-minute telemetry snapshots to `/telemetry.csv` on a local microSD card for offline model retraining.

3. **Human-Centered Explainable AI Layer (`XGBoost` + `SHAP`):**
   - Ingests logged electrical time-series features (`voltage_v`, `voltage_roll_mean_3`, `voltage_drop_rate`, `power_w`, `power_factor`, `balance_bdt`, `hour`).
   - Forecasts short-term household load (`XGBRegressor`) and classifies imminent blackout/cutoff probability (`XGBClassifier`).
   - Uses `shap.TreeExplainer` to rank top positive risk contributors and maps them to plain-language household advisories.

---

## 2. DESCO 7-Tier Progressive Prepaid Tariff Model

For every incremental energy delta $\Delta E$ (kWh), the marginal rate $R(E_{\text{month}})$ is evaluated across Bangladesh's progressive residential slabs:

| Slab Tier | Monthly Consumption Range | Marginal Rate (BDT/kWh) |
| :--- | :--- | :---: |
| Lifeline | `0 – 50 kWh` | `4.63` |
| Tier 1 | `0 – 75 kWh` (non-lifeline) | `5.26` |
| Tier 2 | `76 – 200 kWh` | `7.20` |
| Tier 3 | `201 – 300 kWh` | `7.59` |
| Tier 4 | `301 – 400 kWh` | `8.02` |
| Tier 5 | `401 – 600 kWh` | `12.67` |
| Tier 6 | `Above 600 kWh` | `14.61` |

Total balance deduction per interval includes statutory 5% VAT:
$$\Delta B_t = \Delta E_t \times R(E_{\text{month}}) \times 1.05$$
