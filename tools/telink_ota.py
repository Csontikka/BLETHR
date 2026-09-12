#!/usr/bin/env python3
"""Flash a Telink TLSR8258 device over BLE, from the command line.

The vendor's flasher is a Web Bluetooth page, which needs a browser and a click. This does
the same exchange over `bleak`, so a device can be updated from a script, from a machine
with no display, or as part of a test that has to reflash between runs.

The protocol is the stock Telink OTA one, taken from the vendor page rather than guessed:

  start      write 0xff00 (version), then 0xff01 (start), then wait
  each block write 20 bytes: [u16 index][16 bytes firmware][u16 CRC of the first 18]
             every eighth block, read the characteristic back before continuing
  end        write [0xff02][u16 last index][u16 complement of last index]

All three 16 bit fields are little endian and the CRC is CRC-16/MODBUS. The reads are the
only flow control there is: the characteristic takes writes without a response, so nothing
else stops a host from outrunning the flash writes on the device.

Usage:
    python telink_ota.py --mac A4:C1:38:9A:A6:EA --file ATC_bthr_v12c.bin
    python telink_ota.py --check --file fw.bin        validate the image, touch no device
"""

from __future__ import annotations

import argparse
import asyncio
import struct
import sys
import time

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover
    BleakClient = BleakScanner = None

OTA_SERVICE = "00010203-0405-0607-0809-0a0b0c0d1912"
OTA_CHAR = "00010203-0405-0607-0809-0a0b0c0d2b12"
DIS_SW_REV = "00002a28-0000-1000-8000-00805f9b34fb"

CMD_FW_VERSION = 0xFF00
CMD_START = 0xFF01
CMD_END = 0xFF02

BLOCK = 16  # firmware bytes per packet
READ_EVERY = 8  # read the characteristic back after this many blocks
TELINK_MAGIC = b"KNLT"  # present at offset 8 of every Telink firmware image
MAGIC_OFFSET = 8


def crc16_modbus(data: bytes) -> int:
    """CRC-16/MODBUS: init 0xFFFF, reflected polynomial 0xA001, no final xor."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def load_firmware(path: str) -> tuple[bytes, int]:
    """Read an image, refuse anything that is not a Telink one, pad it to a whole block."""
    with open(path, "rb") as fh:
        fw = fh.read()
    if len(fw) < MAGIC_OFFSET + len(TELINK_MAGIC):
        raise ValueError(path + ": too short to be a firmware image")
    magic = fw[MAGIC_OFFSET : MAGIC_OFFSET + len(TELINK_MAGIC)]
    if magic != TELINK_MAGIC:
        raise ValueError(
            path + ": not a Telink firmware image. Expected " + repr(TELINK_MAGIC)
            + " at offset " + str(MAGIC_OFFSET) + ", found " + repr(magic)
            + ". Refusing to write it."
        )
    original = len(fw)
    if len(fw) % BLOCK:
        fw += b"\xff" * (BLOCK - len(fw) % BLOCK)
    return fw, original


def packet(index: int, chunk: bytes) -> bytes:
    body = struct.pack("<H", index) + chunk
    return body + struct.pack("<H", crc16_modbus(body))


async def connect(mac: str, attempts: int, timeout: float):
    """Connect, retrying: a device busy scanning often refuses the first attempts."""
    last = None
    for attempt in range(1, attempts + 1):
        try:
            target = await BleakScanner.find_device_by_address(mac.upper(), timeout=timeout)
            if target is None:
                raise RuntimeError("not advertising, or out of range")
            client = BleakClient(target, timeout=timeout)
            await client.connect()
            print("  connected on attempt " + str(attempt))
            return client
        except Exception as err:  # noqa: BLE001 - report and retry, whatever it was
            last = err
            print("  attempt " + str(attempt) + "/" + str(attempts) + " failed: " + str(err))
            await asyncio.sleep(2.0)
    raise RuntimeError("could not connect to " + mac + ": " + str(last))


async def read_sw_revision(client) -> str | None:
    try:
        raw = await client.read_gatt_char(DIS_SW_REV)
        return bytes(raw).decode("utf-8", "replace").strip("\x00").strip()
    except Exception:  # noqa: BLE001 - purely informational
        return None


async def flash(mac: str, path: str, attempts: int, timeout: float) -> int:
    fw, original = load_firmware(path)
    blocks = len(fw) // BLOCK
    print("image    " + path)
    print("         " + str(original) + " bytes, padded to " + str(len(fw))
          + ", " + str(blocks) + " blocks")

    print("connect  " + mac)
    client = await connect(mac, attempts, timeout)
    try:
        if client.services.get_service(OTA_SERVICE) is None:
            raise RuntimeError(
                "the device does not expose the Telink OTA service; it is either running "
                "different firmware or the cached service table is stale"
            )
        char = client.services.get_characteristic(OTA_CHAR)
        if char is None:
            raise RuntimeError("the OTA service is present but its characteristic is not")
        # The characteristic takes writes without a response; using them is what makes the
        # transfer fast enough to finish inside the OTA timeout on the device.
        no_response = "write-without-response" in char.properties

        before = await read_sw_revision(client)
        if before:
            print("         running " + before)

        print("start    0xff00, 0xff01")
        await asyncio.sleep(0.5)
        await client.write_gatt_char(char, struct.pack("<H", CMD_FW_VERSION), response=False)
        await client.write_gatt_char(char, struct.pack("<H", CMD_START), response=False)
        await asyncio.sleep(0.3)

        print("send     " + str(blocks) + " blocks", flush=True)
        began = time.monotonic()
        for index in range(blocks):
            chunk = fw[index * BLOCK : (index + 1) * BLOCK]
            await client.write_gatt_char(char, packet(index, chunk), response=not no_response)
            if (index + 1) % READ_EVERY == 0:
                # The only throttle the protocol has: reading back gives the device time to
                # commit what it was sent before the next burst arrives.
                await client.read_gatt_char(char)
            if index % 256 == 0 or index == blocks - 1:
                pct = (index + 1) / blocks * 100
                print("         " + str(index + 1).rjust(5) + "/" + str(blocks)
                      + "  " + format(pct, "5.1f") + "%", flush=True)

        last = blocks - 1
        await client.write_gatt_char(
            char, struct.pack("<HHH", CMD_END, last, (~last) & 0xFFFF), response=False
        )
        print("done     " + format(time.monotonic() - began, ".1f") + " s")
    finally:
        try:
            await client.disconnect()
        except Exception:  # noqa: BLE001 - the device reboots on a successful update
            pass

    print("verify   waiting for the device to come back")
    await asyncio.sleep(8.0)
    for attempt in range(1, 6):
        try:
            # A device that has just come back advertises on its own schedule, which can be ten
            # seconds, so a short scan misses it and reports a failure for an update that worked.
            client = await connect(mac, attempts=2, timeout=max(timeout, 45.0))
            after = await read_sw_revision(client)
            await client.disconnect()
            print("         now running " + str(after))
            return 0
        except Exception as err:  # noqa: BLE001
            print("         not back yet (" + str(attempt) + "/5): " + str(err))
            await asyncio.sleep(5.0)
    print("         could not read the version back. The update may still have succeeded;")
    print("         look at the device before reflashing it.")
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Flash a Telink device over BLE")
    ap.add_argument("--mac", help="device address, e.g. A4:C1:38:9A:A6:EA")
    ap.add_argument("--file", required=True, help="firmware .bin to write")
    ap.add_argument("--attempts", type=int, default=6, help="connection attempts (default 6)")
    ap.add_argument("--timeout", type=float, default=20.0, help="per attempt, seconds")
    ap.add_argument("--check", action="store_true",
                    help="validate the image and exit without touching any device")
    args = ap.parse_args()

    if args.check:
        try:
            fw, original = load_firmware(args.file)
        except ValueError as err:
            print("error: " + str(err))
            return 1
        print(args.file + ": valid Telink image, " + str(original) + " bytes, "
              + str(len(fw) // BLOCK) + " blocks after padding")
        return 0

    if BleakClient is None:
        print("error: bleak is not installed: pip install bleak")
        return 1
    if not args.mac:
        print("error: --mac is required unless --check is given")
        return 1

    try:
        return asyncio.run(flash(args.mac, args.file, args.attempts, args.timeout))
    except KeyboardInterrupt:
        print("\ninterrupted. If a transfer was in progress the device still holds the old "
              "firmware; it only switches banks on a complete update.")
        return 130
    except Exception as err:  # noqa: BLE001 - one clear line beats a traceback here
        print("error: " + str(err))
        return 1


if __name__ == "__main__":
    sys.exit(main())
