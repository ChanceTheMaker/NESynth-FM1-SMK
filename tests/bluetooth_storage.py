"""Fault-inject the actual multi-game storage backend under sanitizers."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    exe=Path(directory)/'library-store'
    for flags in [[],['-DNP_LIBRARY_META=0x36000u','-DNP_LIBRARY_END=0x92000u']]:
     subprocess.run(['cc',*flags,'-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(root/'tests/library_store.c'),str(root/'experiments/bluetooth/rom_transfer.c'),'-o',str(exe)],check=True)
     subprocess.run([str(exe)],check=True)
