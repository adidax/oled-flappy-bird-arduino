# Flappy Bird Clone for Arduino with OLED display
Play Flappy Bird on the Arduino with OLED Display 128x64  
[Makerblog.at](https://www.makerblog.at)

## Hardware Requirements
- Arduino UNO R3 (or compatible)
- OLED I2C Display 128x64 with SSD1306
- 1 x Pushbutton

## Wiring

### OLED -> Arduino UNO R3
- SDA -> A4  
- SCL -> A5  
- GND -> GND  
- VIN -> 5V  

### Push Button
- GND -> Push Button -> D2  

### Push Button RIGHT  
- GND -> Push Button -> D3  

## Required Libraries
Ensure that the following libraries are installed via the Library Manager:
- Adafruit SSD1306 including Adafruit GFX

## Optimization
To avoid floating point numbers (`float`), all relevant coordinate positions are **scaled by a factor of 10** and stored as integers. This improves **performance** by avoiding data type **float** and prevents rounding errors.

### Scaled Values:
- Look out for the SCALE_FACTOR const, used with most coordinates
- **gravity** and **velocity** is also used **multiplied by 10** in calculations.
- Before **displaying on the screen**, these values are **divided by 10**.

### Example:
- Players vertical position `y = 450` is actually at **45.0 pixels** on the display.
- A gravity of `GRAVITY = 5` means a **change of 0.5 px per frame**.

This scaling is applied in **most calculations** (except the horizontal position of the play which is always at x=15), but before rendering (`drawBitmap`, `fillRect`, etc.), the values are converted back to **real pixel values** using `x / SCALE_FACTOR`.
