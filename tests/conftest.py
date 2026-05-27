"""Make the rackbus_client package importable in tests."""

import sys
from pathlib import Path

# Add repo root (parent of `tests/`) to sys.path so `rackbus_client`
# (which sits in the repo root) can be imported.
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))
