# smart-home-control-hub-pic16f877a
Smart Home Control Hub using PIC16F877A with Bluetooth control, 
OLED dashboard, environmental sensors, relay fan automation, 
and hardware interrupt emergency handling.

## Features
- Bluetooth fan control via HC-05
- OLED live sensor dashboard
- Ambient light sensing with PWM LED response
- Smoke and flame detection
- Relay-controlled fan automation
- Hardware interrupt emergency system with recovery

## Hardware
- PIC16F877A
- HC-05 Bluetooth module
- OLED SH1106 128x64
- MQ-2 smoke/gas sensor
- Flame sensor
- LDR light sensor
- 2-way relay module (12V fan)
- Active buzzer

## Communication Protocols
- USART (Bluetooth serial)
- I2C (OLED display)
- ADC (LDR, MQ-2 analog)
- PWM (LED brightness)
- GPIO Interrupts (emergency button)
