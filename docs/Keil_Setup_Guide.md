# Keil µVision 5 Setup & Hardware Flashing Guide (Fresh Project Workflow)

This guide provides step-by-step instructions to create a **fresh Keil µVision 5 project** from scratch, add the standalone **`src/main.c`**, compile the project, and flash the **STM32F401 microcontroller**.

---

## 1. Prerequisites

1. **Keil MDK-ARM µVision 5** (v5.30 or newer).
2. **STM32F4 Device Family Pack (DFP):** `Keil.STM32F4xx_DFP` installed in Keil Pack Installer.
3. **ST-LINK v2 Programmer / Debugger** (with ST-LINK USB drivers).

---

## 2. Creating a Fresh Project in Keil µVision 5

1. Open **Keil µVision 5**.
2. Click **Project** $\to$ **New µVision Project...**
3. Choose a folder and name your project (e.g. `Smart_Streetlight`).
4. **Select Device:** In the device selection dialog, search and select:
   ```
   STMicroelectronics -> STM32F4 Series -> STM32F401 -> STM32F401CCUx (or STM32F401CC)
   ```
   Click **OK**.

---

## 3. Configuring Keil RTE (Run-Time Environment)

In the **Manage Run-Time Environment** window that pops up, you only need:

1. **CMSIS:**
   - Check $\checkmark$ **`CORE`**
2. **Device:**
   - Check $\checkmark$ **`Startup`**

*(Note: Because `main.c` is written in 100% pure CMSIS bare-metal register C, you do NOT need to check any HAL or CubeMX libraries!)*

3. Click **OK**.

---

## 4. Adding `main.c` to the Project

1. In the **Project / Target Tree** on the left, right-click **Source Group 1** (or create a new group called `User`).
2. Click **Add Existing Files to Group...**
3. Browse and select:
   ```
   src\main.c
   ```
4. Click **Add**, then click **Close**.

---

## 5. Target Options Configuration (`Alt + F7`)

1. Right-click **Target 1** $\to$ select **Options for Target 'Target 1'...** (or press `Alt + F7`).
2. **Target Tab:**
   - **Xtal (MHz):** `16.0`
   - **Floating Point Hardware:** *Single Precision*
3. **Output Tab:**
   - Check $\checkmark$ **`Create HEX File`** (if flashing with ST-Link Utility / STM32CubeProgrammer)
4. Click **OK**.

---

## 6. Building the Project

1. Click **Project** $\to$ **Rebuild all target files** (or press **F7**).
2. The Build Output console will confirm:
   ```text
   linking...
   Program Size: Code=... RO-data=... RW-data=... ZI-data=...
   FromELF: creating hex file...
   "Objects\Smart_Streetlight.hex" - 0 Error(s), 0 Warning(s).
   ```

---

## 7. Flashing & Hardware Verification

1. **Hardware Connections:**
   - Connect your **ST-LINK v2** programmer to the STM32F401 board (`SWDIO`, `SWCLK`, `3.3V`, `GND`).
   - Wire the **LM35** output to `PA1` and **LDR** voltage divider to `PA2`.
   - Wire the **Red Hazard LED** to `PA3` and **Streetlight LED** to `PA0` (TIM2 PWM).
   - Wire the **HC-05 Bluetooth Module** (`TX` $\to$ `PA10`, `RX` $\to$ `PA9`).
2. **Flash the MCU:** In Keil, click **Flash** $\to$ **Download** (or press `F8`).
3. **Bluetooth Terminal Testing:**
   - Pair your smartphone with the HC-05 module using any Serial Bluetooth Terminal app at **9600 Baud (8-N-1)**.
   - Live telemetry packets will stream every 500 ms:
     ```text
     Temp:28C | Warn LED:OFF | Lights PWM:0
     ```
   - Send wireless commands from your phone:
     - `'U'` or `'+'` $\implies$ Step Streetlight brightness **UP** (+5%).
     - `'D'` or `'-'` $\implies$ Step Streetlight brightness **DOWN** (-5%).
     - `'A'` $\implies$ Enable **Auto Photocell Mode** (LDR sensor control).
     - `'1'` $\implies$ Force **100% Full Illumination**.
     - `'0'` $\implies$ Turn **OFF** luminaire.
