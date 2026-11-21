# Embedded Differential-Drive Robot (STM32F103C8T6)

A bare-metal / CMSIS C firmware project for an **STM32F103C8T6 (Blue Pill)** differential-drive robot that drives in a straight line using **wheel encoders**, an **MPU6050 IMU**, and **PID control**, paired with an **automated Python Hardware-in-the-Loop (HIL) test suite** over UART.

---

## How It Works

- **System Clock:** 8 MHz external crystal (HSE) × PLL 9 = **72 MHz** (`APB1 = 36 MHz`, `APB2 = 72 MHz`).
- **Motor Control (`motor.c`):** Dual DC motors driven by **TIM3 PWM** (1 kHz, 0–1000 resolution) + GPIO direction pins.
- **Wheel Encoders (`encoder.c`):** Quadrature decoding on **EXTI** interrupts (`EXTI1`, `EXTI2`, `EXTI9_5`) to measure speed and count ticks.
- **IMU & Straight-Line PID (`imu.c`, `pid.c`, `differential_drive.c`):** Reads yaw rate from an **MPU6050** over **I2C1** (100 kHz), calibrates gyro bias on startup, and runs a **100 Hz** loop combining two **Speed PIDs** (left/right wheels) with a **Heading PID** to keep the robot straight.
- **Automated HIL Testing (`uart.c`, `tests/test_uart.py`):** Exposes a simple **USART1 (115200 baud)** command interface so a PC running `pytest` + `pyserial` can test real MCU responsiveness and encoder reads automatically.

---

## Project Structure

```text
├── Core/
│   ├── Inc/
│   │   ├── differential_drive.h   # High-level straight-line controller API & PID gains
│   │   ├── encoder.h              # Dual quadrature encoder API
│   │   ├── imu.h                  # MPU6050 I2C driver API
│   │   ├── motor.h                # Dual DC motor PWM API
│   │   ├── pid.h                  # Generic PID controller API
│   │   └── uart.h                 # USART1 test command interface
│   └── Src/
│       ├── main.c                 # 72 MHz clock setup, mode switch, 100 Hz loop & UART polling
│       ├── differential_drive.c   # Speed + heading PID mixing
│       ├── encoder.c              # EXTI quadrature encoder & ISRs
│       ├── imu.c                  # MPU6050 I2C1 driver & gyro calibration
│       ├── motor.c                # TIM3 PWM + H-bridge direction control
│       ├── pid.c                  # PID with anti-windup & derivative-on-measurement
│       └── uart.c                 # Non-blocking USART1 command parser (PING, ENC?)
├── tests/
│   └── test_uart.py               # Automated pytest + PySerial HIL tests
├── requirements.txt               # Python test dependencies
└── embedded_differential_robot.ioc
```

---

## Hardware Pinout

| Module | STM32 Pin | Function / Connection |
|---|---|---|
| **Left Motor** | `PA6` | TIM3_CH1 PWM (1 kHz) |
| **Left Motor** | `PA4`, `PA5` | Direction IN1, IN2 |
| **Right Motor** | `PA7` | TIM3_CH2 PWM (1 kHz) |
| **Right Motor** | `PB0`, `PB1` | Direction IN1, IN2 |
| **Left Encoder** | `PA1`, `PA2` | EXTI1 (A), EXTI2 (B) with internal pull-up |
| **Right Encoder** | `PB8`, `PB9` | EXTI8 (A), EXTI9 (B) with internal pull-up |
| **IMU (MPU6050)** | `PB6`, `PB7` | I2C1_SCL, I2C1_SDA (100 kHz, address `0x68`) |
| **USB-TTL Adapter** | `PA9` → RX, `PA10` → TX, `GND` → GND | USART1 (115200 baud, **3.3 V logic only**) |

---

## Firmware Modes (`Core/Src/main.c`)

`main.c` includes a simple compile-time switch:

- **`#define DRIVE_ENABLED 0` (default — Bench / HIL Test Mode):** Keeps motors safely stopped while running `USART1` and both encoders for automated PC testing.
- **`#define DRIVE_ENABLED 1` (Robot Drive Mode):** Initializes the IMU, calibrates gyro bias (~1 s, keep robot still), and drives straight forward at `TARGET_FORWARD_SPEED`.

Build and flash using **STM32CubeIDE**.

---

## Automated UART Testing (HIL)

### Protocol (115200 baud, 8N1, `\n` terminated)

| Command (PC → STM32) | Response (STM32 → PC) | What It Verifies |
|---|---|---|
| `PING` | `PONG` | Serial wiring, baud rate, and MCU responsiveness |
| `ENC?` | `ENC L=<int> R=<int>` | Live left/right encoder peripheral integration |
| *(unknown)* | `ERR` | Graceful error handling without locking up |

### Run the Tests

1. Flash firmware with `DRIVE_ENABLED 0`.
2. Connect the 3.3 V USB-to-TTL adapter (`PA9`, `PA10`, `GND`) and check its COM port in Device Manager.
3. Run from the project root:

**Windows (PowerShell):**
```powershell
python -m pip install -r requirements.txt
$env:STM32_PORT="COM3"
python -m pytest -v
```

**Linux / macOS:**
```bash
python3 -m pip install -r requirements.txt
STM32_PORT=/dev/ttyUSB0 python3 -m pytest -v
```

### Automated vs. Manual Checks

- **Automated (`tests/test_uart.py`):** Verifies `PING` → `PONG` (single and 5× repeated), `ENC?` response format and valid `int32_t` counts, and `ERR` on invalid commands.
- **Manual:** Verify 3.3 V UART wiring, confirm motors stay stopped in test mode, and spin each wheel by hand between `ENC?` reads to confirm encoder counts change in the expected direction.
