# CanSat Indoor Data Logger


### 日本語版Readme

日本語版Readme: [README_jp.md](./README_jp.md)



A CanSat model built with a Wio Terminal and a pressure sensor, ground-tested using an elevator to verify descent detection and data integrity during communication loss.

Rather than an outdoor launch-and-fall test with a real CanSat, this project uses a repeatable indoor elevator ride to implement sensor acquisition, state transitions, and data integrity, along with post-experiment visualization and verification.

This repository is currently at **Phase 1.0**, covering pressure-based state detection, SD card logging, LCD display, and field testing with visualization.

---

## What This CanSat Model Does

Treating the microcontroller as a stand-in for a CanSat or small spacecraft, the system infers vehicle state from sensor values and saves the data in a form that can be verified afterward.

Specifically, it does the following:

- Distinguishes "descending" from "stationary" based on pressure changes
- Treats a stop at an intermediate floor as a pause rather than concluding landing immediately
- Judges landing from a sustained period of stable pressure
- Continuously saves sensor data and inferred state to an SD card
- Identifies multiple measurement runs within the same CSV file
- Visualizes pressure changes and state transitions in Python after the experiment

---

## System Overview

### Hardware

| Component | Purpose |
| --- | --- |
| Seeed Studio Wio Terminal | Control, button input, LCD display, SD card logging |
| BMP280 pressure sensor | Pressure acquisition |
| microSD card | CSV log storage |
| Wio Terminal battery base | Power supply during movement |

### Software

| Area | Technology |
| --- | --- |
| Firmware | C++ / Arduino Framework |
| Build environment | PlatformIO |
| Target board | Seeed Wio Terminal |
| Sensor control | Adafruit BMP280 Library |
| Storage | Seeed Arduino FS / Seeed SD |
| LCD | Seeed Arduino LCD / TFT_eSPI |
| Experiment data analysis | Python / pandas / matplotlib / Visual Studio Code (Jupyter Notebook extension) |

---

## Processing Flow

At startup, the BMP280 and SD card are initialized, and the next run ID is determined from the existing log on the SD card. Pressure is then sampled at roughly 1-second intervals, and descent or landing is judged against each threshold.

```text
Power on
   |
   +-- Initialize BMP280
   +-- Initialize SD card
   +-- Prepare data.csv
   +-- Retrieve next run_id
   |
   v
 IDLE
   |
   | Button C pressed
   v
 ARMED
   |
   | Pressure rises more than 0.3 hPa from arming value
   v
 DESCENDING
   |
   | Difference from moving average stays within ±0.05 hPa for 5 samples
   v
 PAUSED
   |\
   | \ Pressure change re-detected
   |  +--------------------------> DESCENDING
   |
   | Stable state continues for 60 samples
   v
 LANDED
   |
   | Show landing on LCD, auto-return after 5 seconds
   v
 IDLE
```

Descending in an elevator lowers altitude, which raises pressure. Descent is therefore detected as a positive change in pressure relative to the value recorded at the moment of arming.

---

## State Transitions

| State | Description | SD logging | Button C action |
| --- | --- | --- | --- |
| `IDLE` | Waiting state | None | Transitions to `ARMED` |
| `ARMED` | Monitoring for descent onset | Yes | Returns to `IDLE` |
| `DESCENDING` | Descent detected | Yes | Aborts measurement and returns to `IDLE` |
| `PAUSED` | Monitoring pressure stability mid-descent or after arrival | Yes | Aborts measurement and returns to `IDLE` |
| `LANDED` | Landing confirmed | Yes | No action; automatically returns to `IDLE` |

Only `IDLE` is excluded from SD logging. This conserves battery and SD card capacity, and makes button C an explicit trigger marking the start of a measurement (`IDLE` → `ARMED`).

### Detection Parameters

| Parameter | Current value | Purpose |
| --- | ---: | --- |
| `DESCENT_THRESHOLD_HPA` | 0.3 hPa | Detecting descent onset / resumption |
| Stability band | within ±0.05 hPa | Stability judgment from current value vs. moving average |
| `PAUSE_COUNT` | 5 samples | Transition from `DESCENDING` to `PAUSED` |
| `LAND_COUNT` | 60 samples | Transition from `PAUSED` to `LANDED` |
| `MOVING_AVG_SIZE` | 10 samples | Moving average used for judgment |
| Sampling interval | ~1 second | Sensor read, logging, LCD update |

To detect descent even when the pressure change is gradual, the pressure recorded at the moment of transition to `ARMED` is used as a fixed reference value. Stability judgment during descent uses the moving average of the most recent 10 samples.

#### Rationale Behind Each Threshold

For each parameter, an initial estimate was derived from general physics or rules of thumb, then adjusted based on the field test.

- **Descent threshold, 0.3 hPa**: Near ground level, pressure rises by roughly 1.2 hPa for every 10 m of descent. Taking a floor height of about 3 m for residential buildings and about 4 m for commercial buildings, and averaging to 3.5 m, one floor corresponds to roughly 0.4 hPa. Accounting for sensor noise, a slightly lower threshold of 0.3 hPa was chosen.
- **Pause judgment, 5 samples (~5 seconds)**: A typical elevator door open/close cycle takes about 2 seconds, and boarding/alighting takes roughly 10–30 seconds. Around 5 seconds of stable pressure was judged sufficient to indicate at least a temporary stop during transit.
- **Landing judgment, 60 samples (~60 seconds)**: The longest intermediate stop actually observed during field testing was 37 seconds, so 60 seconds was chosen with some margin to avoid false positives.
- **Stability band, ±0.05 hPa**: The BMP280 sensor used has a pressure error of roughly ±0.1 hPa, so a tighter threshold of 0.05 hPa was chosen to distinguish real change from noise.

---

## SD Card Logging

Logs are appended to `data.csv` at the root of the SD card. If the file does not exist, a header is created at startup.

```csv
run_id,seq,timestamp_ms,pressure_hpa,baseline_hpa,state
1,0,33597,997.23,997.23,ARMED
1,1,34660,997.25,997.23,ARMED
```

| Column | Description |
| --- | --- |
| `run_id` | Identifier for the measurement session |
| `seq` | Sequence number within the session |
| `timestamp_ms` | Elapsed time in milliseconds since the Wio Terminal booted |
| `pressure_hpa` | Pressure reading from the BMP280 |
| `baseline_hpa` | Moving average of the most recent 10 samples |
| `state` | State at the time of recording |

At startup and at the start of each measurement, the last line of the CSV is checked, and the next session is assigned `run_id` + 1. Each record is written and the file is closed immediately afterward, so data already written is likely to be preserved even if power is lost unexpectedly.

---

## LCD Display

During measurement, the Wio Terminal's LCD shows:

- Current state
- Current pressure
- `run_id`
- `seq`
- Stability counter

After landing is confirmed, `*** LANDED ***` is displayed, and the device automatically returns to the waiting state after 5 seconds. This allows recording status to be checked on-site without connecting to a PC.

---

## Experiment Results

A field test simulating an elevator descent from an observation deck was conducted, yielding two runs of data.

Experiment data: [`ref/data_phase1.0.csv`](./ref/data_phase1.0.csv)

| Item | run_id 1 | run_id 2 |
| --- | ---: | ---: |
| Record count | 142 | 129 |
| Duration | ~147.6 s | ~134.0 s |
| Minimum pressure | 997.22 hPa | 997.47 hPa |
| Maximum pressure | 1009.24 hPa | 1009.51 hPa |
| Pressure range | 12.02 hPa | 12.04 hPa |
| Final state | `LANDED` | `LANDED` |

In run_id 1, the system transitioned to `PAUSED` once during a brief period of pressure stability, then returned to `DESCENDING` upon re-detecting a pressure change, before finally reaching `LANDED` after the final stop. This confirms, with real data, that the state machine correctly avoids treating an intermediate stop as landing.

```text
run_id 1: ARMED → DESCENDING → PAUSED → DESCENDING → PAUSED → LANDED
run_id 2: ARMED → DESCENDING → PAUSED → LANDED
```

### Visualization

Pressure over time and state transitions are visualized using pandas and matplotlib.

#### run_id 1

![Pressure and state transition for run_id 1](./ref/run_id01.png)

#### run_id 2

![Pressure and state transition for run_id 2](./ref/run_id02.png)

Analysis notebook: [`ref/cansat_data_phase1.ipynb`](./ref/cansat_data_phase1.ipynb)

---

## Design Highlights

### 1. Parameter Tuning Through Staged Validation

Before the field test, preliminary validation was carried out on the stairs of my apartment and of a station building. Roughly 10 round trips on the apartment stairs were used mainly to check the effect of code adjustments, while 2 runs on the station stairs (equivalent to the 3rd floor) were used to confirm behavior under different conditions — a higher starting point and gentler stairs than the apartment.

This testing revealed that with the gradual pressure changes typical of walking, the difference from the moving average sometimes failed to reach the fixed threshold (0.3 hPa), causing descent to go undetected. To address this, the pressure at the moment of transition to `ARMED` is used as a fixed reference for descent detection, so that even gradual changes can be detected. Stability and landing judgments, on the other hand, use the moving average, which reduces the effect of short-term sensor noise.

### 2. Introducing a Pause State Based on Real-World Investigation

The target elevator was initially assumed to run non-stop between floors. However, further investigation during experiment planning revealed that it could stop at intermediate floors. It was also noted that, in a real CanSat, turbulent airflow can cause pressure changes to flatten out even before the ground is reached. Based on this, a `PAUSED` state was introduced so that stable pressure is not immediately interpreted as landing. As a result, the field test showed the same logic correctly handling two different outcomes: an intermediate stop in run_id 1, and a non-stop descent in run_id 2.

### 3. Rethinking run_id Management to Fit Power Usage

The original design incremented `run_id` by one at every Wio Terminal power-on. However, since battery conservation required powering the device off between tests, this design would have kept `run_id` fixed at 1 across all runs — a flaw noticed while adjusting the logging behavior. The design was therefore changed to read the last `run_id` from the SD card log and determine the next value from it.

### 4. Combining Automatic Detection with Manual Abort

Descent onset and landing are judged automatically from sensor values. At the same time, button C allows disarming or aborting a measurement, to handle operator mistakes or unexpected behavior during testing.

### 5. Logging the Basis for Each Judgment

Each log row stores not just the raw sensor reading but also the moving average and state used for the judgment at that moment, making it possible to verify the validity of state transitions after the fact.

---

## File Structure

```text
.
├── platformio.ini                         # PlatformIO configuration, dependencies
├── src/
│   └── main.cpp                           # Sensor acquisition, state machine, SD logging, LCD display
└── ref/
    ├── data_phase1.0.csv                  # Phase 1.0 experiment log
    ├── cansat_data_phase1.ipynb           # Notebook for visualizing experiment data
    ├── run_id01.png                       # Visualization result for run_id 1
    └── run_id02.png                       # Visualization result for run_id 2
```

---

## Build and Upload

### Prerequisites

- PlatformIO Core
- Wio Terminal
- BMP280 pressure sensor
- A microSD card formatted as FAT32 (16 GB or smaller)

### Build

```bash
pio run
```

### Uploading to the Wio Terminal

Put the Wio Terminal into bootloader mode, connect it to a PC, and run:

```bash
pio run --target upload
```

### Debug Output

To enable serial debugging, enable the following definition in `src/main.cpp`:

```cpp
#define DEV_MODE
```

The serial baud rate when enabled is 115200 bps. Debug output is disabled in normal builds.

---

## Operation

1. Connect the BMP280 and microSD card, then power on the Wio Terminal.
2. Confirm that the LCD shows the `IDLE` state after initialization.
3. Before starting a measurement, press button C on the Wio Terminal to transition to `ARMED`.
4. During descent, the device automatically transitions to `DESCENDING` and logs pressure and state.
5. Once pressure remains stable for 5 seconds, the state transitions to `PAUSED`. If descent is detected again afterward, it transitions back to `DESCENDING`.
6. Once pressure in `PAUSED` remains stable for about 60 seconds, the state transitions to `LANDED`.
7. After the experiment, copy `data.csv` from the microSD card to a PC for analysis.

Pressing button C while in `ARMED`, `DESCENDING`, or `PAUSED` aborts the measurement and returns the device to `IDLE`.

---

## Known Limitations

- Currently offline-logging only; Wi-Fi transmission and automatic retransmission after a communication outage are not yet implemented.
- The pressure threshold and sample counts are empirical values based on this elevator experiment. They may need to be re-tuned for other locations or weather conditions.
- `MOVING_AVG_SIZE` (the moving average window) and `PAUSE_COUNT` (the pause-judgment sample count) are nominally independent parameters, but a wider moving-average window increases the delay before stability is detected, so the two likely interact in practice. No adverse effect has been observed in field testing, but this has not been quantitatively verified.
- The stability threshold (0.05 hPa) is currently hard-coded rather than defined as a `#define` constant like the other thresholds.
- Judgment is based on pressure alone; vehicle acceleration, attitude, and vibration are not used.
- `timestamp_ms` is not an absolute timestamp but elapsed time since the Wio Terminal booted.
- There is no handling yet for cases where the LCD, pressure sensor, or SD card fails to initialize.
- The visualization code (`cansat_data_phase1.ipynb`) was originally written in a different directory from the CanSat code, so paths may need to be adjusted when re-run within this repository.

---

## Future Work

Building on Phase 1.0, the following extensions are being considered:

- Adding Wi-Fi communication
- Buffering to the SD card during communication outages
- Automatic retransmission of unsent data after communication is restored
- Data-loss countermeasures via delivery confirmation and retransmission control
- Reinforcing descent/landing judgment with an accelerometer
- Attitude estimation and sensor fusion using a 9-axis IMU
- Extending to an outdoor CanSat using GPS
- Retry logic and error display for log write failures
- Unit tests for the state transition logic
- Re-validating `MOVING_AVG_SIZE` at 5 samples (checking its interaction with `PAUSE_COUNT`)
- Defining the stability threshold as a `#define` constant
- Porting part of the codebase to Rust and evaluating an embedded Rust environment

Future features will be added incrementally, aiming toward an architecture where sensor processing, state management, data integrity, and communication are cleanly separated.
