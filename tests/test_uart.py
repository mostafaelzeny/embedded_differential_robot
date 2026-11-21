"""
Automated Hardware-in-the-Loop (HIL) UART tests for STM32F103C8T6 Blue Pill.

How this test interacts with the firmware:
  1. PySerial opens a real COM/tty serial connection to USART1 (PA9/PA10)
     at 115200 baud, 8N1.
  2. Each test sends a newline-terminated ASCII command (e.g. "PING\n").
  3. On the STM32, UART_Poll() in Core/Src/uart.c receives the command,
     executes it against the live MCU peripherals (e.g. Encoder_GetCountLeft/Right),
     and writes back a newline-terminated ASCII response.
  4. Pytest validates the exact response format and values.

Usage:
  Set STM32_PORT to your USB-UART adapter port, then run pytest:
    PowerShell:  $env:STM32_PORT="COM3"; pytest -v
    Linux/macOS: STM32_PORT=/dev/ttyUSB0 pytest -v
"""

import os
import re
import time

import pytest
import serial

SERIAL_PORT = os.environ.get("STM32_PORT", "COM3")
BAUD_RATE = int(os.environ.get("STM32_BAUD", "115200"))
TIMEOUT_SEC = 1.0

# Matches firmware output from Core/Src/uart.c: "ENC L=<int> R=<int>"
ENC_RESPONSE_PATTERN = re.compile(r"^ENC L=(-?\d+) R=(-?\d+)$")


def send_command(ser: serial.Serial, command: str) -> str:
    """Send a single ASCII command to the STM32 and return the stripped response."""
    # Clear any stale bytes so previous runs cannot pollute this assertion
    ser.reset_input_buffer()
    ser.write((command + "\n").encode("ascii"))
    ser.flush()

    raw_line = ser.readline()
    return raw_line.decode("ascii", errors="replace").strip()


@pytest.fixture(scope="module")
def stm32_uart():
    """Open the real serial port once for the test session and close it cleanly."""
    try:
        ser = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=TIMEOUT_SEC,
        )
    except serial.SerialException as exc:
        pytest.fail(
            f"Could not open serial port '{SERIAL_PORT}' at {BAUD_RATE} baud: {exc}\n"
            "Check your USB-UART connection and set STM32_PORT (e.g. COM3 or /dev/ttyUSB0)."
        )

    # Give the USB-UART bridge a brief moment to settle, then flush buffers
    time.sleep(0.2)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    yield ser

    ser.close()


def test_ping_pong(stm32_uart):
    """Verify basic UART communication and MCU responsiveness."""
    response = send_command(stm32_uart, "PING")
    assert response == "PONG", f"Expected 'PONG', got '{response}'"


def test_ping_repeatable(stm32_uart):
    """Verify repeated commands succeed reliably without buffer corruption."""
    for i in range(5):
        response = send_command(stm32_uart, "PING")
        assert response == "PONG", f"Iteration {i + 1}: expected 'PONG', got '{response}'"


def test_encoder_query(stm32_uart):
    """Verify ENC? returns valid signed 32-bit left and right encoder counts."""
    response = send_command(stm32_uart, "ENC?")
    match = ENC_RESPONSE_PATTERN.match(response)
    assert match is not None, (
        f"Unexpected ENC? response format: '{response}' "
        "(expected 'ENC L=<int> R=<int>')"
    )

    left_count = int(match.group(1))
    right_count = int(match.group(2))

    # Firmware stores counts in int32_t (Core/Src/encoder.c)
    assert -(2**31) <= left_count <= (2**31 - 1)
    assert -(2**31) <= right_count <= (2**31 - 1)


def test_unknown_command_returns_err(stm32_uart):
    """Verify invalid commands are rejected cleanly and the MCU stays responsive."""
    assert send_command(stm32_uart, "UNKNOWN") == "ERR"
    assert send_command(stm32_uart, "PING") == "PONG"
