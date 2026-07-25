#!/usr/bin/env python3
"""Cut the M5Stamp S3 USB-C opening into the Dendy top shell."""

from pathlib import Path

import trimesh


HERE = Path(__file__).resolve().parent
SOURCE = HERE / "thing-3357677/files/Dendy_top.stl"
OUTPUT = HERE / "thing-3357677/files/Dendy_top_usb.stl"

# STL coordinates, millimetres.  The PCB origin maps to:
#   stl_x = pcb_x - 45
#   stl_y = 11.5 - pcb_y
# M1 is at PCB (78.9178, 20.32), so its USB-C centre is at STL y=-8.82.
USB_CENTRE_Y = -8.82
OPENING_WIDTH_Y = 11.0
OPENING_HEIGHT_Z = 5.5


def main() -> None:
    shell = trimesh.load_mesh(SOURCE, process=True)

    # Cut from the inner right wall through the exterior.  The opening meets the
    # shell seam at z=9.5 and extends downward with print/plug clearance.
    cutter = trimesh.creation.box(
        extents=(8.0, OPENING_WIDTH_Y, OPENING_HEIGHT_Z + 1.0)
    )
    cutter.apply_translation(
        (51.0, USB_CENTRE_Y, 9.5 - OPENING_HEIGHT_Z / 2 + 0.5)
    )

    modified = trimesh.boolean.difference(
        (shell, cutter), engine="manifold", check_volume=False
    )
    if modified is None or not modified.is_watertight:
        raise RuntimeError("USB opening boolean did not produce a watertight mesh")

    modified.export(OUTPUT)
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
