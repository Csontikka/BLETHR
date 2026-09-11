#!/usr/bin/env python3
"""Read and configure a BLETHR repeater over BLE, from the command line.

BLETHR answers on service 0xFFE0, characteristic 0xFFE1, and unlike the thermometer firmware
it replies by notification rather than by a read of the same characteristic. Every exchange is
a write of one opcode byte plus its payload, and a notification that echoes the opcode back.

Six byte addresses travel little endian on the wire, so they read backwards against the usual
text form. Scan windows are in units of 8 microseconds, which is 1/125 of a millisecond; the
interval is plain milliseconds.

Usage:
    python blethr.py --mac A4:C1:38:9A:A6:EA read
    python blethr.py --mac A4:C1:38:9A:A6:EA set-source A4:C1:38:67:50:2F
    python blethr.py --mac A4:C1:38:9A:A6:EA set-cfg --interval-ms 5000
    python blethr.py --mac A4:C1:38:9A:A6:EA set-cfg --win-min-ms 6 --win-max-ms 14
    python blethr.py --mac A4:C1:38:9A:A6:EA reboot
"""

from __future__ import annotations

import argparse
import asyncio
import json
import struct
import sys

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover
    BleakClient = BleakScanner = None

SERVICE = "0000ffe0-0000-1000-8000-00805f9b34fb"
CHAR = "0000ffe1-0000-1000-8000-00805f9b34fb"
DIS_SW_REV = "00002a28-0000-1000-8000-00805f9b34fb"

CMD_DEV_ID = 0x00
CMD_MAC = 0x10
CMD_TIME = 0x23
CMD_CFG = 0x55
CMD_EXT_MAC = 0x58
CMD_EXT_BIND_KEY = 0x5C
CMD_REBOOT = 0x72

TICKS_PER_MS = 125  # scan windows are in 8 us units
INTERVAL_MS_MIN, INTERVAL_MS_MAX = 3000, 10000
WINDOW_MS_MIN, WINDOW_MS_MAX = 5.0, 50.0


def mac_from_le(raw: bytes) -> str | None:
    if len(raw) < 6:
        return None
    return ":".join(format(b, "02X") for b in reversed(raw[:6]))


def mac_to_le(text: str) -> bytes:
    parts = text.replace("-", ":").split(":")
    if len(parts) != 6:
        raise ValueError("a MAC address has six octets: " + text)
    return bytes(int(p, 16) for p in reversed(parts))


class Session:
    """One connection's worth of command exchanges. Replies arrive as notifications."""

    def __init__(self, client):
        self.client = client
        self.waiters: dict[int, asyncio.Future] = {}

    def _on_notify(self, _sender, data: bytearray) -> None:
        if not data:
            return
        fut = self.waiters.pop(data[0], None)
        if fut is not None and not fut.done():
            fut.set_result(bytes(data))

    async def __aenter__(self):
        await self.client.start_notify(CHAR, self._on_notify)
        # The subscription is not live the instant start_notify returns: without this pause the
        # first command of a session reliably loses its reply and times out, while every command
        # after it succeeds.
        await asyncio.sleep(0.4)
        return self

    async def __aexit__(self, *_):
        try:
            await self.client.stop_notify(CHAR)
        except Exception:  # noqa: BLE001 - the link may already be gone
            pass

    async def cmd(self, opcode: int, payload: bytes = b"", timeout: float = 6.0,
                  tries: int = 2) -> bytes:
        last: Exception | None = None
        for _ in range(tries):
            loop = asyncio.get_running_loop()
            fut: asyncio.Future = loop.create_future()
            self.waiters[opcode] = fut
            await self.client.write_gatt_char(CHAR, bytes([opcode]) + payload, response=True)
            try:
                return await asyncio.wait_for(fut, timeout)
            except asyncio.TimeoutError as err:
                last = err
            finally:
                self.waiters.pop(opcode, None)
        raise last if last else RuntimeError("no reply")


def parse_cfg(raw: bytes) -> dict:
    """[0x55] flg u8, rf_tx_power u8, interval u16 ms, win_min u16, win_max u16."""
    if len(raw) < 9:
        return {"cfg_raw": raw.hex()}
    flg, rf, interval, win_min, win_max = struct.unpack("<BBHHH", raw[1:9])
    return {
        "cfg_raw": raw.hex(),
        "temp_F": bool(flg & 0x01),
        "rf_tx_power": rf,
        "scan_interval_ms": interval,
        "scanning": interval != 0,
        "win_min_raw": win_min,
        "win_max_raw": win_max,
        "win_min_ms": round(win_min / TICKS_PER_MS, 3),
        "win_max_ms": round(win_max / TICKS_PER_MS, 3),
    }


def build_cfg(current: dict, changes: dict) -> bytes:
    """The eight byte config payload: current values with changes applied, validated.

    The config is written as one struct, so a field nobody is changing still has to be sent.
    Refusing when the current value is unknown is deliberate: sending a zero would switch
    scanning off, which looks like a working device that quietly repeats nothing.
    """
    merged = dict(current)
    merged.update(changes)
    for key in ("temp_F", "rf_tx_power", "scan_interval_ms", "win_min_ms", "win_max_ms"):
        if merged.get(key) is None:
            raise ValueError("current configuration unknown; read the device first")

    interval = int(merged["scan_interval_ms"])
    if interval and not INTERVAL_MS_MIN <= interval <= INTERVAL_MS_MAX:
        raise ValueError(
            "scan interval must be 0 or " + str(INTERVAL_MS_MIN) + ".." + str(INTERVAL_MS_MAX)
            + " ms, got " + str(interval)
        )
    win_min = float(merged["win_min_ms"])
    win_max = float(merged["win_max_ms"])
    for name, val in (("win_min", win_min), ("win_max", win_max)):
        if not WINDOW_MS_MIN <= val <= WINDOW_MS_MAX:
            raise ValueError(
                name + " must be " + str(WINDOW_MS_MIN) + ".." + str(WINDOW_MS_MAX)
                + " ms, got " + str(val)
            )
    if win_max < win_min:
        raise ValueError("win_max (" + str(win_max) + ") is below win_min (" + str(win_min) + ")")

    return struct.pack(
        "<BBHHH",
        0x01 if merged["temp_F"] else 0x00,
        int(merged["rf_tx_power"]),
        interval,
        int(round(win_min * TICKS_PER_MS)),
        int(round(win_max * TICKS_PER_MS)),
    )


async def connect(mac: str, attempts: int, timeout: float):
    """A repeater that is busy scanning turns the first attempts away. Retry."""
    last = None
    for attempt in range(1, attempts + 1):
        try:
            target = await BleakScanner.find_device_by_address(mac.upper(), timeout=timeout)
            if target is None:
                raise RuntimeError("not advertising, or out of range")
            client = BleakClient(target, timeout=timeout)
            await client.connect()
            if client.services.get_service(SERVICE) is None:
                await client.disconnect()
                raise RuntimeError("no BLETHR service; this is not a repeater")
            return client
        except Exception as err:  # noqa: BLE001 - report and retry, whatever it was
            last = err
            if attempt < attempts:
                await asyncio.sleep(2.0)
    raise RuntimeError("could not reach " + mac + ": " + str(last))


async def read_all(client) -> dict:
    out: dict = {}
    try:
        raw = await client.read_gatt_char(DIS_SW_REV)
        out["sw_revision"] = bytes(raw).decode("utf-8", "replace").strip("\x00").strip()
    except Exception:  # noqa: BLE001 - informational only
        out["sw_revision"] = None
    async with Session(client) as s:
        try:
            out.update(parse_cfg(await s.cmd(CMD_CFG)))
        except Exception as err:  # noqa: BLE001
            out["cfg_error"] = repr(err)
        # 0x58 puts the six address bytes straight after the opcode; 0x10 puts a length byte
        # in front of them, because its payload can also carry the two random address bytes.
        for key, opcode, offset in (("ext_mac", CMD_EXT_MAC, 1), ("mac", CMD_MAC, 2)):
            try:
                reply = await s.cmd(opcode)
                out[key] = mac_from_le(reply[offset:])
            except Exception as err:  # noqa: BLE001
                out[key + "_error"] = repr(err)
        try:
            reply = await s.cmd(CMD_EXT_BIND_KEY)
            key_bytes = reply[1:17]
            out["ext_bind_key_set"] = len(key_bytes) == 16 and any(key_bytes)
        except Exception as err:  # noqa: BLE001
            out["ext_bind_key_error"] = repr(err)
        try:
            reply = await s.cmd(CMD_TIME)
            out["device_time"] = struct.unpack("<I", reply[1:5])[0] if len(reply) >= 5 else None
        except Exception as err:  # noqa: BLE001
            out["device_time_error"] = repr(err)
    return out


async def run(args) -> int:
    client = await connect(args.mac, args.attempts, args.timeout)
    try:
        if args.action == "read":
            print(json.dumps(await read_all(client), indent=2))
            return 0

        if args.action == "reboot":
            async with Session(client) as s:
                try:
                    await s.cmd(CMD_REBOOT, timeout=3.0)
                except asyncio.TimeoutError:
                    pass  # the device may reboot before it answers
            print("reboot requested")
            return 0

        current = await read_all(client)
        if args.action == "set-source":
            before = current.get("ext_mac")
            async with Session(client) as s:
                await s.cmd(CMD_EXT_MAC, mac_to_le(args.source))
            after = await read_all(client)
            print("ext_mac " + str(before) + " -> " + str(after.get("ext_mac")))
            return 0 if (after.get("ext_mac") or "").upper() == args.source.upper() else 1

        if args.action == "set-cfg":
            changes = {}
            if args.interval_ms is not None:
                changes["scan_interval_ms"] = args.interval_ms
            if args.win_min_ms is not None:
                changes["win_min_ms"] = args.win_min_ms
            if args.win_max_ms is not None:
                changes["win_max_ms"] = args.win_max_ms
            if args.rf_tx_power is not None:
                changes["rf_tx_power"] = args.rf_tx_power
            if not changes:
                print("nothing to change")
                return 1
            payload = build_cfg(current, changes)
            async with Session(client) as s:
                await s.cmd(CMD_CFG, payload)
            after = await read_all(client)
            for key in ("scan_interval_ms", "win_min_ms", "win_max_ms", "rf_tx_power"):
                if current.get(key) != after.get(key):
                    print(key + " " + str(current.get(key)) + " -> " + str(after.get(key)))
            return 0
    finally:
        try:
            await client.disconnect()
        except Exception:  # noqa: BLE001
            pass
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Read and configure a BLETHR repeater over BLE")
    ap.add_argument("--mac", required=True)
    ap.add_argument("--attempts", type=int, default=6)
    ap.add_argument("--timeout", type=float, default=20.0)
    sub = ap.add_subparsers(dest="action", required=True)
    sub.add_parser("read")
    sub.add_parser("reboot")
    p = sub.add_parser("set-source")
    p.add_argument("source", help="address of the thermometer to repeat")
    p = sub.add_parser("set-cfg")
    p.add_argument("--interval-ms", type=int, help="source beacon period, 0 disables scanning")
    p.add_argument("--win-min-ms", type=float)
    p.add_argument("--win-max-ms", type=float)
    p.add_argument("--rf-tx-power", type=int)
    args = ap.parse_args()

    if BleakClient is None:
        print("error: bleak is not installed: pip install bleak")
        return 1
    try:
        return asyncio.run(run(args))
    except Exception as err:  # noqa: BLE001 - one clear line beats a traceback here
        print("error: " + str(err))
        return 1


if __name__ == "__main__":
    sys.exit(main())
