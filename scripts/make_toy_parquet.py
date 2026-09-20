"""Convert the committed toy CSV inputs to Parquet for the MWE."""
from pathlib import Path
import pandas as pd

ROOT = Path(__file__).resolve().parents[1]
TOY = ROOT / "tests" / "parquet_toy"
for name in ("toy_nodelist", "toy_edgelist"):
    pd.read_csv(TOY / f"{name}.csv").to_parquet(TOY / f"{name}.parquet", index=False)
