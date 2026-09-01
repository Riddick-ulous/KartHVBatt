#!/usr/bin/env python3
"""Validate PackController DBC/Fault contracts; optionally require strict cantools parsing."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass, field
from pathlib import Path


LEGAL_CAN_FD_LENGTHS = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64}


@dataclass
class Message:
    can_id: int
    name: str
    length: int
    signals: dict[str, set[int]] = field(default_factory=dict)


def read_fault_entries(path: Path) -> list[tuple[int, str, int]]:
    entries: list[tuple[int, str, int]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        match = re.match(r"\|\s*(\d+)\s*\|\s*`([^`]+)`\s*\|", line)
        if match:
            entries.append((int(match.group(1)), match.group(2), line_number))
    return entries


def validate_fault_matrix(path: Path) -> list[str]:
    errors: list[str] = []
    ids: list[int] = []
    names: set[str] = set()

    for fault_id, name, line_number in read_fault_entries(path):
        if fault_id in ids:
            errors.append(f"{path}:{line_number}: duplicate FaultId {fault_id}")
        if name in names:
            errors.append(f"{path}:{line_number}: duplicate FaultId name {name}")
        ids.append(fault_id)
        names.add(name)

    if not ids:
        return [f"{path}: no FaultId rows found"]
    expected = list(range(0, max(ids) + 1))
    if ids != expected:
        errors.append(f"{path}: IDs must be ordered and contiguous 0..{max(ids)}")
    if max(ids) > 127:
        errors.append(f"{path}: FaultId {max(ids)} exceeds the 128-bit DBC bitmap")
    return errors


def validate_primary_fault_value_table(dbc_path: Path, fault_path: Path) -> list[str]:
    errors: list[str] = []
    matching_lines = [
        (line_number, line)
        for line_number, line in enumerate(
            dbc_path.read_text(encoding="utf-8").splitlines(), 1
        )
        if re.match(r"VAL_\s+272\s+PrimaryFaultId\s+", line)
    ]
    if len(matching_lines) != 1:
        return [
            f"{dbc_path}: expected exactly one BMS_Status.PrimaryFaultId value table"
        ]

    line_number, line = matching_lines[0]
    actual_pairs = re.findall(r'(\d+)\s+"([^"]+)"', line)
    actual = {int(value): name for value, name in actual_pairs}
    expected = {
        fault_id: name for fault_id, name, _line_number in read_fault_entries(fault_path)
    }
    if actual != expected:
        missing = sorted(set(expected.items()) - set(actual.items()))
        unexpected = sorted(set(actual.items()) - set(expected.items()))
        if missing:
            errors.append(
                f"{dbc_path}:{line_number}: PrimaryFaultId is missing or renames {missing}"
            )
        if unexpected:
            errors.append(
                f"{dbc_path}:{line_number}: PrimaryFaultId has unexpected choices {unexpected}"
            )
    return errors


def validate_dbc(path: Path) -> tuple[list[str], dict[int, Message]]:
    errors: list[str] = []
    messages: dict[int, Message] = {}
    vframe_formats: dict[int, int] = {}
    canfd_brs: dict[int, int] = {}
    signal_comments: set[tuple[int, str]] = set()
    current: Message | None = None
    lines = path.read_text(encoding="utf-8").splitlines()

    for line_number, line in enumerate(lines, 1):
        message_match = re.match(r"BO_\s+(\d+)\s+(\w+):\s+(\d+)\s+(\w+)", line)
        if message_match:
            can_id, name, length, _sender = message_match.groups()
            can_id_int = int(can_id)
            length_int = int(length)
            if can_id_int in messages:
                errors.append(f"{path}:{line_number}: duplicate CAN ID {can_id_int}")
            if can_id_int > 0x7FF:
                errors.append(f"{path}:{line_number}: {name} is not an 11-bit CAN ID")
            if length_int not in LEGAL_CAN_FD_LENGTHS:
                errors.append(f"{path}:{line_number}: illegal CAN-FD length {length_int}")
            current = Message(can_id_int, name, length_int)
            messages[can_id_int] = current
            continue

        signal_match = re.match(
            r"\s*SG_\s+(\w+)(?:\s+[mM]\d*)?\s*:\s*(\d+)\|(\d+)@([01])([+-])",
            line,
        )
        if not signal_match:
            continue
        if current is None:
            errors.append(f"{path}:{line_number}: signal outside a message")
            continue

        name, start, width, byte_order, _signed = signal_match.groups()
        start_int = int(start)
        width_int = int(width)
        if name in current.signals:
            errors.append(f"{path}:{line_number}: duplicate signal {current.name}.{name}")
            continue
        if byte_order != "1":
            errors.append(f"{path}:{line_number}: only Intel byte order is allowed")
            continue
        if width_int < 1 or start_int + width_int > current.length * 8:
            errors.append(f"{path}:{line_number}: {current.name}.{name} exceeds DLC")
            continue

        bits = set(range(start_int, start_int + width_int))
        for previous_name, previous_bits in current.signals.items():
            overlap = sorted(bits & previous_bits)
            if overlap:
                errors.append(
                    f"{path}:{line_number}: {current.name}.{name} overlaps "
                    f"{previous_name} at bits {overlap}"
                )
        current.signals[name] = bits

    vframe_definition_found = False
    brs_definition_found = False
    bus_type_can_fd = False

    for line_number, line in enumerate(lines, 1):
        vframe_definition = re.match(
            r'BA_DEF_\s+BO_\s+"VFrameFormat"\s+ENUM\s+(.+);', line
        )
        if vframe_definition:
            choices = re.findall(r'"([^"]*)"', vframe_definition.group(1))
            vframe_definition_found = True
            if len(choices) < 16 or choices[14:16] != ["StandardCAN_FD", "ExtendedCAN_FD"]:
                errors.append(
                    f"{path}:{line_number}: Vector VFrameFormat must define "
                    "StandardCAN_FD/ExtendedCAN_FD at enum values 14/15"
                )

        if re.match(
            r'BA_DEF_\s+BO_\s+"CANFD_BRS"\s+ENUM\s+"0"\s*,\s*"1"\s*;',
            line,
        ):
            brs_definition_found = True

        if re.match(r'BA_\s+"BusType"\s+"CAN FD"\s*;', line):
            bus_type_can_fd = True

        vframe_match = re.match(r'BA_\s+"VFrameFormat"\s+BO_\s+(\d+)\s+(\d+)\s*;', line)
        if vframe_match:
            can_id, value = map(int, vframe_match.groups())
            vframe_formats[can_id] = value

        brs_match = re.match(r'BA_\s+"CANFD_BRS"\s+BO_\s+(\d+)\s+(\d+)\s*;', line)
        if brs_match:
            can_id, value = map(int, brs_match.groups())
            canfd_brs[can_id] = value

        attr_match = re.match(
            r'BA_\s+"(?:VFrameFormat|CANFD_BRS|GenMsgCycleTime)"\s+BO_\s+(\d+)\s+',
            line,
        )
        value_match = re.match(r"VAL_\s+(\d+)\s+(\w+)\s+", line)
        comment_match = re.match(r'CM_\s+SG_\s+(\d+)\s+(\w+)\s+"(.+)"\s*;', line)
        if attr_match and int(attr_match.group(1)) not in messages:
            errors.append(f"{path}:{line_number}: attribute references unknown CAN ID")
        if value_match:
            can_id = int(value_match.group(1))
            signal_name = value_match.group(2)
            if can_id not in messages:
                errors.append(f"{path}:{line_number}: value table references unknown CAN ID")
            elif signal_name not in messages[can_id].signals:
                errors.append(
                    f"{path}:{line_number}: value table references unknown signal "
                    f"{messages[can_id].name}.{signal_name}"
                )
            else:
                enum_values = [int(value) for value in re.findall(r'(-?\d+)\s+"[^"]*"', line)]
                if len(enum_values) != len(set(enum_values)):
                    errors.append(f"{path}:{line_number}: duplicate enum value")
                width = len(messages[can_id].signals[signal_name])
                if any(value < 0 or value >= (1 << width) for value in enum_values):
                    errors.append(
                        f"{path}:{line_number}: enum value does not fit "
                        f"{messages[can_id].name}.{signal_name} ({width} bit)"
                    )
        if comment_match:
            can_id = int(comment_match.group(1))
            signal_name = comment_match.group(2)
            key = (can_id, signal_name)
            if can_id not in messages:
                errors.append(f"{path}:{line_number}: comment references unknown CAN ID")
            elif signal_name not in messages[can_id].signals:
                errors.append(
                    f"{path}:{line_number}: comment references unknown signal "
                    f"{messages[can_id].name}.{signal_name}"
                )
            elif key in signal_comments:
                errors.append(
                    f"{path}:{line_number}: duplicate signal comment for "
                    f"{messages[can_id].name}.{signal_name}"
                )
            else:
                signal_comments.add(key)

    if not vframe_definition_found:
        errors.append(f"{path}: Vector-compatible VFrameFormat ENUM definition missing")
    if not brs_definition_found:
        errors.append(f"{path}: Vector-compatible CANFD_BRS ENUM definition missing")
    if not bus_type_can_fd:
        errors.append(f'{path}: global BusType must be "CAN FD"')

    for can_id, message in messages.items():
        if vframe_formats.get(can_id) != 14:
            errors.append(
                f"{path}: {message.name} must use VFrameFormat=14 (StandardCAN_FD)"
            )
        if canfd_brs.get(can_id) != 1:
            errors.append(f"{path}: {message.name} must use CANFD_BRS=1")
        for signal_name in message.signals:
            if (can_id, signal_name) not in signal_comments:
                errors.append(
                    f"{path}: missing signal comment for "
                    f"{message.name}.{signal_name}"
                )

    required = {
        0x100: ("VCU_BMS_Control", 16),
        0x101: ("PackCurrentStatus", 16),
        0x102: ("InverterStatus", 16),
        0x110: ("BMS_Status", 64),
        0x111: ("BMS_SafetyDiag", 64),
        0x112: ("BMS_Analog", 64),
        0x118: ("BMS_CellPage", 64),
        0x119: ("BMS_TemperaturePage", 64),
        0x120: ("BMS_ServiceRequest", 32),
        0x121: ("BMS_ServiceResponse", 32),
    }
    for can_id, expected in required.items():
        actual = messages.get(can_id)
        if actual is None:
            errors.append(f"{path}: required CAN ID 0x{can_id:03X} missing")
        elif (actual.name, actual.length) != expected:
            errors.append(
                f"{path}: 0x{can_id:03X} expected {expected}, "
                f"got {(actual.name, actual.length)}"
            )

    cell_slots = sum(name.startswith("CellVoltageSlot") for name in messages.get(0x118, Message(0, "", 0)).signals)
    temperature_slots = sum(
        name.startswith("TemperatureSlot")
        for name in messages.get(0x119, Message(0, "", 0)).signals
    )
    if cell_slots != 24 or 7 * cell_slots < 162:
        errors.append(f"{path}: cell paging must cover 162 groups with 7 x 24 slots")
    if temperature_slots != 24 or 9 * temperature_slots != 216:
        errors.append(f"{path}: temperature paging must cover exactly 216 NTCs")

    return errors, messages


def validate_with_cantools(path: Path, required: bool) -> list[str]:
    try:
        import cantools
    except ImportError:
        if required:
            return ["cantools is required but not installed"]
        return []

    try:
        database = cantools.database.load_file(path, strict=True)
    except Exception as error:  # cantools exposes multiple parse-error classes
        return [f"{path}: cantools strict parse failed: {error}"]

    errors: list[str] = []
    for message in database.messages:
        if not message.is_fd:
            errors.append(f"{path}: cantools does not recognize {message.name} as CAN FD")
        attributes = message.dbc.attributes
        if attributes["VFrameFormat"].value != 14:
            errors.append(f"{path}: cantools reads invalid VFrameFormat for {message.name}")
        if attributes["CANFD_BRS"].value != 1:
            errors.append(f"{path}: cantools reads invalid CANFD_BRS for {message.name}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dbc", type=Path, required=True)
    parser.add_argument("--fault-matrix", type=Path, required=True)
    parser.add_argument("--require-cantools", action="store_true")
    args = parser.parse_args()

    errors = validate_fault_matrix(args.fault_matrix)
    errors.extend(validate_primary_fault_value_table(args.dbc, args.fault_matrix))
    dbc_errors, messages = validate_dbc(args.dbc)
    errors.extend(dbc_errors)
    errors.extend(validate_with_cantools(args.dbc, args.require_cantools))

    if errors:
        print("contract validation failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    signal_count = sum(len(message.signals) for message in messages.values())
    print(f"OK: {len(messages)} CAN-FD messages, {signal_count} signals")
    print("OK: all messages are Vector StandardCAN_FD with BRS")
    print("OK: all DBC signals have non-empty comments")
    if args.require_cantools:
        print("OK: cantools strict parse")
    print("OK: FaultId range is contiguous and fits the 128-bit fault bitmap")
    print("OK: cell pages cover 162 groups; temperature pages cover 216 NTCs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
