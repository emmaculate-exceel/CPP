# Vehicle Diagnostic Tool

A command-line OBD-II vehicle diagnostic tool written in C++ 17.  
Currently runs in **simulation mode** — no hardware required.  
Real ELM327/USB adapter support can be added as a future extension.

---

## Features

| # | Feature | Description |
|---|---------|-------------|
| 1 | Live Sensor Data | Reads RPM, speed, temperature, throttle, battery, and more |
| 2 | Read Fault Codes (DTCs) | Displays active diagnostic trouble codes with descriptions |
| 3 | Clear Fault Codes | Clears all active DTCs from the ECU |
| 4 | Vehicle Info (VIN) | Decodes and displays the Vehicle Identification Number |
| 5 | Freeze Frame Data | Shows sensor snapshot recorded when a fault was triggered |
| 6 | Inject Fault Code | Adds a test DTC into the simulator (simulation only) |

---

## Project Structure

```
OBD/
├── include/
│   ├── obd2.h              # Data types: SensorData, DTC, FreezeFrame, PIDs
│   ├── simulator.h         # VehicleSimulator class declaration
│   ├── decoder.h           # OBDDecoder class declaration
│   └── diagnostic_tool.h   # DiagnosticTool class declaration
├── src/
│   ├── main.cpp            # Entry point
│   ├── simulator.cpp       # Fake ECU — simulates OBD-II responses
│   ├── decoder.cpp         # Decodes raw hex responses into readable values
│   └── diagnostic_tool.cpp # CLI menu and feature logic
├── Makefile
└── README.md
```

---

## Build & Run

**Requirements:** g++ with C++ 17 support

```bash
# Build
make

# Run
./vehicle_diagnostic

# Build and run in one step
make run

# Clean build artifacts
make clean
```

---

## How It Works

The tool follows the OBD-II standard (SAE J1979). Commands are sent as hex strings and responses are decoded using standard formulas.

```
Simulation mode:   [Tool] <---> [simulator.cpp (fake ECU)]
Real hardware:     [Tool] <---> [ELM327 adapter] <---> [Car ECU]
```

### Example OBD-II Commands

| Command | PID  | Description         |
|---------|------|---------------------|
| `01 0C` | 0x0C | Engine RPM          |
| `01 0D` | 0x0D | Vehicle Speed       |
| `01 05` | 0x05 | Coolant Temperature |
| `01 11` | 0x11 | Throttle Position   |
| `01 42` | 0x42 | Battery Voltage     |
| `03 00` | —--  | Read DTCs           |
| `04 00` | —--  | Clear DTCs          |
| `09 02` | 0x02 | Vehicle VIN         |

---

## Supported Sensors (Live Data)

| Sensor | Unit | Formula |
|--------|------|---------|
| Engine RPM | RPM | (A×256 + B) / 4 |
| Vehicle Speed | km/h | A |
| Coolant Temperature | °C | A − 40 |
| Throttle Position | % | A × 100 / 255 |
| Engine Load | % | A × 100 / 255 |
| Fuel Pressure | kPa | A × 3 |
| Intake Air Temperature | °C | A − 40 |
| MAF Air Flow Rate | g/s | (A×256 + B) / 100 |
| Battery Voltage | V | (A×256 + B) / 1000 |

---

## Known DTC Codes

The tool includes descriptions for 20 standard OBD-II fault codes including:

- `P0171` — System Too Lean (Bank 1)
- `P0300` — Random/Multiple Cylinder Misfire Detected
- `P0420` — Catalyst System Efficiency Below Threshold
- `P0440` — Evaporative Emission Control System Malfunction
- and many more...

---

## Protocol

- **Standard:** OBD-II / SAE J1979
- **Services used:** 01 (Live Data), 02 (Freeze Frame), 03 (Read DTCs), 04 (Clear DTCs), 09 (Vehicle Info)

---

## Roadmap

- [ ] Real serial port communication (ELM327 via `/dev/ttyUSB0`)
- [ ] Continuous live data refresh mode
- [ ] Export diagnostic report to file
- [ ] More DTC code descriptions
- [ ] Windows COM port support
