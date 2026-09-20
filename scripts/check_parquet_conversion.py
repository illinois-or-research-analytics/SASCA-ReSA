import pandas as pd

def check_pair(csv_path, parquet_path):
    csv_df = pd.read_csv(csv_path)
    parquet_df = pd.read_parquet(parquet_path)

    print("Checking:")
    print(csv_path)
    print(parquet_path)

    print("CSV shape:", csv_df.shape)
    print("Parquet shape:", parquet_df.shape)

    assert csv_df.shape == parquet_df.shape
    assert list(csv_df.columns) == list(parquet_df.columns)

    csv_compare = csv_df.astype(str)
    parquet_compare = parquet_df.astype(str)

    assert csv_compare.equals(parquet_compare)

    print("Passed")
    print()

check_pair(
    "tests/parquet_toy/toy_nodelist.csv",
    "tests/parquet_toy/toy_nodelist.parquet",
)

check_pair(
    "tests/parquet_toy/toy_edgelist.csv",
    "tests/parquet_toy/toy_edgelist.parquet",
)

print("All conversion tests passed.")