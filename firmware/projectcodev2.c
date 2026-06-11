// =============================================================================
//  Smart Home System - PIC16F877A
//  Module: Temperature Monitor + Smart Lighting + Fan
// =============================================================================

#include <xc.h>
#include <stdint.h>

// Configuration Bits 
#pragma config FOSC  = XT       // 4MHz crystal
#pragma config WDTE  = OFF      // Watchdog timer off
#pragma config PWRTE = ON       // Power-up timer on
#pragma config BOREN = ON       // Brown-out reset on
#pragma config LVP   = OFF      // Low voltage programming off
#pragma config CPD   = OFF      // Data EEPROM protection off
#pragma config WRT   = OFF      // Flash write protection off
#pragma config CP    = OFF      // Code protection off

#define _XTAL_FREQ 4000000      // 4 MHz (Most common)

// Pin Definitions 
#define PIR_PIN     RB4         // PIR motion sensor (digital input)
#define FAN_PIN     RD0        // Climate control LED output

// ADC Channel Numbers 
#define LM35_CH     0      // AN0 - LM35 temperature sensor
#define LDR_CH      1     // AN1 - LDR light sensor (mimicing the dark/bright)

// Timing Constant 
#define NO_MOTION_LIMIT  300

// Global Variables 
volatile uint16_t no_motion_timer = 0;  
volatile uint8_t  room_occupied   = 0;  

float    temperature = 0.0;     
uint16_t ldr_val     = 0;       
uint16_t pwm_duty    = 0;       

//------------------------------------------------------------------------------
// ADC Initialization (Binary Formatting)
//------------------------------------------------------------------------------
void ADC_Init(void) {
    // ADCON1: Right-justified result | RA0, RA1 analog inputs
    ADCON1 = 0b10000000;  
    
    // ADCON0: Fosc/8 clock | Select Channel 0 (AN0) | Turn ADC ON
    ADCON0 = 0b01000001;  
}

uint16_t ADC_Read(uint8_t ch) {
    ADCON0 &= 0b11000111;       // Clear channel select bits (bits 5:3)
    ADCON0 |= (ch << 3);        // Set desired channel
    __delay_us(30);             // Acquisition time
    ADCON0bits.GO = 1;          // Start conversion (A-to-D))
    while (ADCON0bits.GO);      
    return (uint16_t)((ADRESH << 8) | ADRESL); // Get the bits (10-bits)
}

//------------------------------------------------------------------------------
// PWM Initialization & Control
//------------------------------------------------------------------------------
void PWM_Init(void) {
    PR2     = 249;              // PWM period register --> 4kHz @ 4MHz Fosc
    T2CON   = 0b00000100;       // Timer2 ON | pre-scaler 1:1
    CCP1CON = 0b00001100;       // CCP1 in PWM mode
    TRISC2  = 0;                // Explicitly RC2 is an output
    CCPR1L  = 0;                // Starting at 0% duty cycle
}

// duty range: 0 (0%) to 1000 (100% brightness)
void PWM_SetDuty(uint16_t duty) {
    if (duty > 1000) duty = 1000;
    
    // Sending the highest 8 bits to CCPR1L
    CCPR1L = (uint8_t)(duty >> 2);
    
    // Placing the lowest 2 bits into CCP1CON bits 5 and 4
    CCP1CONbits.CCP1X = (duty >> 1) & 1;
    CCP1CONbits.CCP1Y = duty & 1;
}

//------------------------------------------------------------------------------
// Timer1 Initialization
//------------------------------------------------------------------------------
void Timer1_Init(void) {
    T1CON  = 0b00110001;        // ON | 1:8 pre-scaler | internal clock
    TMR1H  = 0xCF;              
    TMR1L  = 0x2C;              
    PIR1bits.TMR1IF = 0;        
    PIE1bits.TMR1IE = 1;        
    INTCONbits.PEIE = 1;        
}

//------------------------------------------------------------------------------
// Helper Calculations
//------------------------------------------------------------------------------
float Read_Temperature(void) {
    uint16_t raw = ADC_Read(LM35_CH);
    return (float)raw * 0.4883f;
}

uint16_t LDR_to_Duty(uint16_t ldr) {
    uint16_t inverted = 1023 - ldr;     
    return (uint16_t)((inverted * 1000UL) / 1023); 
}

//------------------------------------------------------------------------------
// Interrupt Service Routine
//------------------------------------------------------------------------------
void interrupt ISR(void) {
    if (PIE1bits.TMR1IE && PIR1bits.TMR1IF) {
        TMR1H = 0xCF;
        TMR1L = 0x2C;
        PIR1bits.TMR1IF = 0;    

        if (PIR_PIN) {
            no_motion_timer = 0;
            room_occupied   = 1;
        } else {
            if (no_motion_timer < NO_MOTION_LIMIT)
                no_motion_timer++;
        }
    }
}

//------------------------------------------------------------------------------
// Main Program Execution
//------------------------------------------------------------------------------
void main(void) {
    // SETING ALL PORT DIRECTIONS 
    TRISA = 0xFF;        // PORTA all input (ADC on RA0, RA1)
    TRISB = 0xFF;        // PORTB all input (PIR on RB1)
    TRISC = 0xFF;        // PORTC inputs default
    TRISD = 0x00;        // PORTD outputs for fan/appliances
    PORTD = 0x00;        // Start clear

    // RUNNING INITIALIZATIONS (PWM_Init can now safely override TRISC2)
    ADC_Init();
    PWM_Init();
    Timer1_Init();

    INTCONbits.GIE = 1;  // Enable global interrupts

    while (1) {
        // Reading sensors 
        temperature = Read_Temperature();
        ldr_val = ADC_Read(LDR_CH);

        // Smart Climate Control (LED TOGGLES)
        if (temperature >= 30.0) {
            FAN_PIN = 1; // Turn DC Motor fan ON
        } else {
            FAN_PIN = 0; // Turn DC Motor fan OFF 
        }

        // Smart Lighting Logic
        if (no_motion_timer >= NO_MOTION_LIMIT) {
            PWM_SetDuty(0);
            room_occupied = 0;
        } else if (room_occupied || PIR_PIN) {
            pwm_duty = LDR_to_Duty(ldr_val);
            PWM_SetDuty(pwm_duty);
        }
        __delay_ms(200);    
    }
}