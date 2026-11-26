"""
Test 2: Verify live encoder readings from STM32 (ENC? -> ENC L=<int> R=<int>).

How it interacts with the firmware:
  Sends "ENC?\\n" over USART1 (115200 baud) -> UART_Poll() in Core/Src/uart.c
  reads Encoder_GetCountLeft() and Encoder_GetCountRight() from Core/Src/encoder.c
  and replies with "ENC L=<left> R=<right>\\n". Repeated 5 times to verify stability.
"""

import os
import re
import time
import pytest
import serial

PORT = os.environ.get("STM32_PORT", "COM3")
BAUD = 115200


@pytest.fixture(scope="module")
def uart():
    with serial.Serial(PORT, BAUD, timeout=1) as ser:
        time.sleep(0.2)
        ser.reset_input_buffer()
        yield ser


@pytest.mark.parametrize("run", range(5))
def test_encoder_counts(uart, run):
    uart.write(b"ENC?\n")
    response = uart.readline().decode("ascii").strip()

    match = re.match(r"^ENC L=(-?\d+) R=(-?\d+)$", response)
    assert match is not None, f"Run {run + 1}: unexpected response '{response}'"
