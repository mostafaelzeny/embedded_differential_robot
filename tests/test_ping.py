"""
Test 1: Verify UART communication with STM32 (PING -> PONG).

How it interacts with the firmware:
  Sends "PING\\n" over USART1 (115200 baud) -> UART_Poll() in Core/Src/uart.c
  replies with "PONG\\n". Repeated 5 times to verify stability.
"""

import os
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
def test_ping_pong(uart, run):
    uart.write(b"PING\n")
    response = uart.readline().decode("ascii").strip()

    assert response == "PONG", f"Run {run + 1}: expected 'PONG', got '{response}'"
