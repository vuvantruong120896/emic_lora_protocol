# Flash firmware from VS Code (Renesas Flash Programmer + E2 Lite)

This repo can build directly in VS Code. Flashing is also possible by running Renesas Flash Programmer (RFP) in batch mode via a VS Code task.

## 1) Prerequisites

- Hardware: E2 Lite
- Software: Renesas Flash Programmer installed
- A working RFP project configured for:
  - Target MCU: RL78 (R7F100GGG)
  - Tool: E2 Lite
  - Connection settings for your board
  - Image file: `HardwareDebug/EMIC_LORA_PROTOCOL.mot`

> RFP CLI switches differ across versions. The recommended workflow is to create the project once in the GUI, confirm it programs successfully, then run that same project in batch mode.

## 2) Configure environment variables

Set these in your user environment (Windows):

- `RFP_EXE`: Full path to your RFP executable (e.g. `...\RFPV3.exe`)
- `RFP_PROJECT_FILE`: Full path to your saved RFP project file (the one you tested in the GUI)

Optional:

- `RFP_EXTRA_ARGS`: Extra CLI arguments for your RFP version.
  - If auto-detection fails, set this to the full argument string you want to run.
  - Example shape (flags vary by version): `-p "C:\path\project.rpj" -auto -quit`

## 3) Run flash from VS Code

- Run Task: **Flash (RFP / E2 Lite)**

Default image path passed to the script:

- `HardwareDebug/EMIC_LORA_PROTOCOL.mot`

## 4) Troubleshooting

- If the task says it cannot auto-detect switches:
  - Run `RFPV3.exe /?` (or `rfp.exe /?`) in a terminal.
  - Find the flags that run a project in batch mode.
  - Put them into `RFP_EXTRA_ARGS`.

- If RFP cannot connect:
  - Ensure no other tool is using the E2 Lite (e2studio debug session, another RFP instance).
  - Check target power and programming wiring.

- If RFP reports "excessive data outside device memory area" (e.g. address `0x000FC228`):
  - The generated `.mot` can include RAM initialization records at high addresses (0x00FBxxxx+).
  - The VS Code script filters the `.mot` to the flash address range (derived from the sibling `.map`) before launching RFP.
  - If you still see this error, confirm your RFP project target MCU matches the build (RL78 / R7F100GGG), and that the `.map` file exists next to the `.mot`.

