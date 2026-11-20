"""
Explainable Load Forecasting & Blackout-Risk Prediction Pipeline (XGBoost + SHAP).

Trains:
1. XGBRegressor to forecast next-interval household power consumption (W).
2. XGBClassifier to predict imminent blackout or prepaid cutoff risk.
3. SHAP TreeExplainer to translate top feature drivers into human-centered
   plain-language advisories for non-technical household users.
"""

import os
import pandas as pd
import numpy as np
import xgboost as xgb
import shap
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, f1_score, classification_report

FEATURE_COLS = [
    "voltage_v",
    "current_a",
    "power_w",
    "frequency_hz",
    "power_factor",
    "balance_bdt",
    "hour",
    "voltage_roll_mean_3",
    "voltage_drop_rate",
    "power_roll_mean_3",
]

HUMAN_READABLE_FEATURES = {
    "voltage_v": "instantaneous grid voltage sag",
    "voltage_roll_mean_3": "sustained low feeder voltage",
    "voltage_drop_rate": "sudden voltage drop",
    "power_w": "high active household load",
    "power_roll_mean_3": "sustained heavy appliance usage",
    "current_a": "elevated line current draw",
    "power_factor": "inductive motor/AC power factor drop",
    "balance_bdt": "critically low prepaid credit balance",
    "frequency_hz": "grid frequency deviation",
    "hour": "evening peak grid demand window",
}


def load_or_prepare_data() -> pd.DataFrame:
    if os.path.exists("benchmark_telemetry.csv"):
        df = pd.read_csv("benchmark_telemetry.csv")
    else:
        df = pd.read_csv("sample_telemetry.csv")
        df["hour"] = pd.to_datetime(df["timestamp"]).dt.hour
        df["voltage_roll_mean_3"] = df["voltage_v"].rolling(3, min_periods=1).mean()
        df["voltage_drop_rate"] = df["voltage_v"].diff().fillna(0.0)
        df["power_roll_mean_3"] = df["power_w"].rolling(3, min_periods=1).mean()
        df["target_next_power_w"] = df["power_w"].shift(-1).bfill()
        next_v = df["voltage_v"].shift(-1).bfill()
        df["blackout_risk"] = (
            (next_v < 194.0) | (df["voltage_drop_rate"] < -12.0) | (df["balance_bdt"] < 100.0)
        ).astype(int)
    return df


def generate_plain_language_advisory(row: pd.Series, shap_vals: np.ndarray, risk_prob: float) -> str:
    ranked_indices = np.argsort(np.abs(shap_vals))[::-1]
    top_drivers = []
    for idx in ranked_indices:
        if shap_vals[idx] > 0:
            fname = FEATURE_COLS[idx]
            top_drivers.append(f"{HUMAN_READABLE_FEATURES[fname]} ({fname}={row[fname]:.2f})")
        if len(top_drivers) == 2:
            break

    status = "HIGH RISK" if risk_prob >= 0.5 else "NORMAL"
    if not top_drivers:
        return f"[{status} | p={risk_prob:.2f}] Grid and prepaid parameters are stable."

    drivers_str = " and ".join(top_drivers)
    return (
        f"[{status} | p={risk_prob:.2f}] Elevated interruption risk driven primarily by "
        f"{drivers_str}. Recommended action: Reduce high-wattage inductive loads or top up prepaid credit."
    )


def main():
    df = load_or_prepare_data()
    X = df[FEATURE_COLS]
    y_load = df["target_next_power_w"]
    y_risk = df["blackout_risk"]

    X_train, X_test, y_load_train, y_load_test, y_risk_train, y_risk_test = train_test_split(
        X, y_load, y_risk, test_size=0.2, shuffle=False
    )

    # 1. Load Forecasting Regressor
    load_model = xgb.XGBRegressor(
        n_estimators=150, max_depth=4, learning_rate=0.06, random_state=42
    )
    load_model.fit(X_train, y_load_train)
    load_preds = load_model.predict(X_test)
    mae = mean_absolute_error(y_load_test, load_preds)
    print(f"Load Forecaster Test MAE: {mae:.2f} W")

    # 2. Blackout / Cutoff Risk Classifier
    risk_model = xgb.XGBClassifier(
        n_estimators=150, max_depth=4, learning_rate=0.06, eval_metric="logloss", random_state=42
    )
    risk_model.fit(X_train, y_risk_train)
    risk_probs = risk_model.predict_proba(X_test)[:, 1]
    risk_preds = (risk_probs >= 0.5).astype(int)
    print(f"Blackout-Risk Classifier Test F1: {f1_score(y_risk_test, risk_preds, zero_division=0):.3f}")
    print(classification_report(y_risk_test, risk_preds, zero_division=0))

    # 3. SHAP Explainability -> Human-Centered Plain-Language Advisories
    explainer = shap.TreeExplainer(risk_model)
    shap_values = explainer.shap_values(X_test)

    print("--- Sample Plain-Language Explainable Advisories ---")
    high_risk_indices = np.where(risk_preds == 1)[0]
    sample_indices = high_risk_indices[:3] if len(high_risk_indices) > 0 else range(min(3, len(X_test)))
    for i in sample_indices:
        advisory = generate_plain_language_advisory(X_test.iloc[i], shap_values[i], risk_probs[i])
        print(advisory)


if __name__ == "__main__":
    main()
