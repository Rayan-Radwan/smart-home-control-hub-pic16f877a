//  Smart Home Hub - PIC16F877A  FULL SYSTEM
//  OLED: Brightness % | Smoke status | Flame status | Fan status
//  Bluetooth: FAN ON / FAN OFF
//  Emergency: button ? buzzer + red LED + screen + lock
//  Recovery: button ? stop buzzer + green LED + resume
//  PWM LEDs: brightness inverse of LDR
//  Flame sensor: flinch LED on RD6

#include <xc.h>
#include <stdint.h>
#include <string.h>

#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP  = OFF
#pragma config CPD  = OFF
#pragma config WRT  = OFF
#pragma config CP   = OFF

#define _XTAL_FREQ 20000000

// ---- Pin Definitions ----
#define PIR_PIN         PORTBbits.RB4   // pin 37
#define SMOKE_DO        PORTBbits.RB3   // pin 36
#define FLAME_PIN       PORTBbits.RB2   // pin 35 (active-low)
#define EMERG_BTN       PORTBbits.RB0   // pin 33 (emergency button)
#define RECOV_BTN       PORTBbits.RB1   // pin 34 (recovery button)

#define SCL             RC3
#define SDA             RC4 
#define SCL_TRIS        TRISC3
#define SDA_TRIS        TRISC4

#define STATUS_LED      RD0             // pin 19
#define FLAME_LED       RD6             // pin 29
#define GREEN_LED       RD1             // pin 22
#define RED_LED         RD2             // pin 28
#define BUZZER          RD3             // pin 27
#define FAN_RELAY       RD7             // pin 30

// ---- Constants ----
#define OLED_ADDR       0x78

// ---- Global Flags ----
volatile uint8_t emergency = 0; //volatile (the variable can change inside an interrupt)

//  I2C SOFTWARE
void I2C_delay(void) { __delay_us(5); }

void I2C_start(void) { //Makes sure both wires are ready (SDA and SCL)
    SDA_TRIS = 0; SCL_TRIS = 0;
    SDA = 1; SCL = 1; I2C_delay(); //listen, a message is starting
    SDA = 0; I2C_delay();
    SCL = 0;
}

void I2C_stop(void) { //done talking, communication finished
    SDA = 0; SCL = 1; I2C_delay();
    SDA = 1; I2C_delay();
}

void I2C_write_byte(uint8_t data) { //It sends a number (8 bits = 1 byte) bit by bit
    for (uint8_t i = 0; i < 8; i++) {
        SDA = (data & 0x80) ? 1 : 0; //Look at the leftmost bit If it?s 1 ( send HIGH) , If it?s 0 ( send LOW)
        SCL = 1; I2C_delay();
        SCL = 0; I2C_delay();
        data <<= 1;
    } //ACK check:
    SDA_TRIS = 1;
    SCL = 1; I2C_delay();
    SCL = 0; I2C_delay();
    SDA_TRIS = 0;
}

//  OLED SH1106
void OLED_cmd(uint8_t cmd) {
    I2C_start();
    I2C_write_byte(OLED_ADDR);
    I2C_write_byte(0x00);
    I2C_write_byte(cmd);
    I2C_stop();
}

void OLED_data(uint8_t dat) {
    I2C_start();
    I2C_write_byte(OLED_ADDR);
    I2C_write_byte(0x40);
    I2C_write_byte(dat);
    I2C_stop();
}

void OLED_init(void) {
    __delay_ms(100);
    OLED_cmd(0xAE);
    OLED_cmd(0xA8); OLED_cmd(0x3F);
    OLED_cmd(0xD3); OLED_cmd(0x00);
    OLED_cmd(0x40);
    OLED_cmd(0xA1);
    OLED_cmd(0xC8);
    OLED_cmd(0xDA); OLED_cmd(0x12);
    OLED_cmd(0x81); OLED_cmd(0xFF);
    OLED_cmd(0xA4);
    OLED_cmd(0xA6);
    OLED_cmd(0xD5); OLED_cmd(0x80);
    OLED_cmd(0x8D); OLED_cmd(0x14);
    OLED_cmd(0xAF);
}

void OLED_set_cursor(uint8_t page, uint8_t col) {
    col += 2;
    OLED_cmd(0xB0 + page);
    OLED_cmd(0x00 + (col & 0x0F));
    OLED_cmd(0x10 + (col >> 4));
}

void OLED_clear(void) {
    for (uint8_t page = 0; page < 8; page++) {
        OLED_set_cursor(page, 0);
        for (uint8_t col = 0; col < 128; col++)
            OLED_data(0x00);
    }
}

const uint8_t font5x7[][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
    {0x00,0x00,0x00,0x00,0x00}, // space (26)
    {0x00,0x00,0x5F,0x00,0x00}, // ! (27)
    {0x3E,0x51,0x49,0x45,0x3E}, // 0 (28)
    {0x00,0x42,0x7F,0x40,0x00}, // 1 (29)
    {0x42,0x61,0x51,0x49,0x46}, // 2 (30)
    {0x21,0x41,0x45,0x4B,0x31}, // 3 (31)
    {0x18,0x14,0x12,0x7F,0x10}, // 4 (32)
    {0x27,0x45,0x45,0x45,0x39}, // 5 (33)
    {0x3C,0x4A,0x49,0x49,0x30}, // 6 (34)
    {0x01,0x71,0x09,0x05,0x03}, // 7 (35)
    {0x36,0x49,0x49,0x49,0x36}, // 8 (36)
    {0x06,0x49,0x49,0x29,0x1E}, // 9 (37)
    {0x14,0x14,0x14,0x14,0x14}, // % (38)
};

uint8_t char_index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A'; // A is 0 , B is 1 
    if (c >= '0' && c <= '9') return 28 + (c - '0'); // 0 is 28 , 1 is 29 etc
    if (c == '%') return 38;
    if (c == '!') return 27;
    return 26; //blank
}

void OLED_print(uint8_t page, uint8_t col, const char* str) {
    while (*str) {
        OLED_set_cursor(page, col);
        uint8_t idx = char_index(*str);
        for (uint8_t i = 0; i < 5; i++)
            OLED_data(font5x7[idx][i]);
        OLED_data(0x00);
        col += 6;
        str++;
    }
}

//  ADC 
void ADC_Init(void) {
    ADCON1 = 0b10000000; //result should be right-justified 
    ADCON0 = 0b01000001; //turns the ADC on and sets the speed it works at
}

uint16_t ADC_Read(uint8_t ch) {
    ADCON0 &= 0b11000111;
    ADCON0 |= (ch << 3); //selecting which pin to read
    __delay_us(30);
    ADCON0bits.GO = 1; //measure now
    while (ADCON0bits.GO);
    return (uint16_t)((ADRESH << 8) | ADRESL); // result comes in two 8-bit registers, We stitch them together into one number
}

uint8_t ADC_to_percent(uint16_t adc) { //Converting the raw number to something human readable.
    return (uint8_t)((adc * 100UL) / 1023); //10-bit number (0-1023).
}

//  PWM
void PWM_Init(void) {
    TRISC2  = 0;
    PR2     = 249; //setting how fast the on/off switching happens,4000 switches per second (4kHz)
    T2CON   = 0b00000101; // timer2
    CCP1CON = 0b00001100; //PIC work in PWM mode
    CCPR1L  = 0; //starting at 0% duty cycle. LEDs start off.
}

void PWM_SetDuty(uint16_t duty) {
    if (duty > 1023) duty = 1023;
    CCPR1L = (uint8_t)(duty >> 2); //duty cycle is a 10-bit number , shift right by 2 to fit the top 8 bits in there.
    CCP1CONbits.CCP1X = (duty >> 1) & 1;
    CCP1CONbits.CCP1Y = duty & 1; // the extra 2 bits store them separately in two special bits inside CCP1CON
}

//  USART
void USART_Init(void) {
    TRISC6 = 1;
    TRISC7 = 1;
    SPBRG  = 32; //setting the speed of communication to 9600 baud
    TXSTA  = 0b00100000; //TXEN bit = 1 , enable sending
    RCSTA  = 0b10010000; //SPEN = 1  "turn on the serial port" CREN = 1 means "enable receiving."
}

void USART_write(char data) {
    while (!TXSTAbits.TRMT);
    TXREG = data;
}

void USART_print(const char* str) {
    while (*str) USART_write(*str++);
}

uint8_t USART_available(void) {
    return PIR1bits.RCIF; // RCIF is a flag that goes HIGH when a new character has arrived in the receive buffer, Don't read when nothing arrives
}

char USART_read(void) {
    if (RCSTAbits.OERR) { //checking for overrun error (if characters keep arriving faster than we read them)
        RCSTAbits.CREN = 0;
        RCSTAbits.CREN = 1;
    }
    return RCREG; // reading the character from the receive register
}

//  INTERRUPT ISR  Emergency button on RB0
void __interrupt() ISR(void) {
    if (INTCONbits.INTF) {
        emergency = 1;
        INTCONbits.INTF = 0;
    }
}

//  HELPERS
void uint8_to_str(uint8_t val, char* buf) { // display numbers as txt 
    if (val >= 100) { // 3 digits
        buf[0] = '0' + (val / 100);
        buf[1] = '0' + ((val / 10) % 10);
        buf[2] = '0' + (val % 10);
        buf[3] = '\0';
    } else if (val >= 10) { // 2 digits 
        buf[0] = '0' + (val / 10);
        buf[1] = '0' + (val % 10);
        buf[2] = '\0';
    } else { // 1 digit 
        buf[0] = '0' + val;
        buf[1] = '\0';
    }
}

void draw_labels(void) {
    OLED_clear();
    OLED_print(0, 4, "BRIGHTNESS");
    OLED_print(2, 4, "SMOKE");
    OLED_print(4, 4, "FLAME");
    OLED_print(6, 4, "FAN");
}

void update_dashboard(uint8_t light_pct, uint8_t smoke, uint8_t flame, uint8_t fan) {
    char num[5];

    // Brightness
    OLED_print(0, 76, "    ");
    uint8_to_str(light_pct, num);
    OLED_print(0, 76, num);
    OLED_print(0, 76 + (light_pct >= 100 ? 18 : light_pct >= 10 ? 12 : 6), "%");

    // Smoke
    OLED_print(2, 76, "    ");
    OLED_print(2, 76, smoke ? "YES!" : "NO  ");

    // Flame
    OLED_print(4, 76, "    ");
    OLED_print(4, 76, flame ? "YES!" : "NO  ");

    // Fan
    OLED_print(6, 76, "    ");
    OLED_print(6, 76, fan ? "ON  " : "OFF ");
}

void show_emergency_screen(void) {
    OLED_clear();
    OLED_print(1, 4, "!!! EMERGENCY !!!");
    OLED_print(4, 4, "PRESS RECOVERY");
    OLED_print(6, 4, "TO RESUME");
}

void show_recovery_screen(void) {
    OLED_clear();
    OLED_print(3, 10, "SYSTEM");
    OLED_print(5, 10, "RECOVERED");
}

//  MAIN
void main(void) {
    // Port directions
    TRISA = 0xFF;               // all input (ADC)
    TRISB = 0xFF;               // all input (sensors + buttons)
    TRISC = 0xFF;               // default input (PWM + USART init overrides)
    TRISD = 0x00;               // all output (LEDs, buzzer, relay)
    PORTD = 0x00;
    
    FAN_RELAY = 1;      // Fan OFF at startup (for active-LOW relay)

    // External interrupt RB0  emergency button
    TRISB0 = 1;
    INTCONbits.INTE  = 1;
    INTCONbits.INTF  = 0;
    OPTION_REGbits.INTEDG = 1;  // rising edge
    INTCONbits.GIE   = 1;

    ADC_Init();
    PWM_Init();
    USART_Init();
    OLED_init();

    draw_labels();
    USART_print("READY\r\n");

    // State variables
    uint8_t  fan_on          = 0;
    uint8_t  emergency_shown = 0;
    uint8_t  prev_light      = 255;
    uint8_t  prev_smoke      = 255;
    uint8_t  prev_flame      = 255;
    uint8_t  prev_fan        = 255;

    // Bluetooth command buffer
    char     bt_buf[20];
    uint8_t  bt_idx = 0;

    while (1) {

        //  EMERGENCY STATE
        if (emergency) {
            // Turn everything off except buzzer and red LED
            FAN_RELAY  = 1;
            STATUS_LED = 0;
            FLAME_LED  = 0;
            GREEN_LED  = 0;
            PWM_SetDuty(0);     // PWM LEDs off

            BUZZER  = 1;        // buzzer on
            RED_LED = 1;        // red LED on

            if (!emergency_shown) {
                show_emergency_screen();
                emergency_shown = 1;
                USART_print("EMERGENCY!\r\n");
            }

            // Check recovery button (RB1)
            if (RECOV_BTN) {
                __delay_ms(50);         // debounce
                if (RECOV_BTN) {
                    // Reset emergency
                    emergency       = 0;
                    emergency_shown = 0;
                    BUZZER          = 0;
                    RED_LED         = 0;
                    GREEN_LED       = 1;    // green LED on = recovered

                    show_recovery_screen();
                    __delay_ms(2000);       // show recovery message 2 sec
                    GREEN_LED = 0;

                    // Redraw dashboard
                    draw_labels();
                    prev_light = 255;       // force redraw
                    prev_smoke = 255;
                    prev_flame = 255;
                    prev_fan   = 255;

                    USART_print("RECOVERED\r\n");
                }
            }
            continue;   // skip normal loop while in emergency
        }

        //  BLUETOOTH COMMAND PARSING (non-blocking)
        if (USART_available()) {
            char c = USART_read();
            if (c == '\n' || c == '\r') {
                bt_buf[bt_idx] = '\0';
                if (strcmp(bt_buf, "FAN ON") == 0) {
                    fan_on    = 1; 
                    FAN_RELAY = 0;      // LOW = relay ON
                    USART_print("FAN ON\r\n"); 
                }
                else if (strcmp(bt_buf, "FAN OFF") == 0) {
                    fan_on    = 0;
                    FAN_RELAY = 1;      // HIGH = relay OFF
                    USART_print("FAN OFF\r\n");
                } 
                else if (bt_idx > 0) {
                    USART_print("UNKNOWN CMD\r\n");
                }
                bt_idx = 0;
            } else if (bt_idx < sizeof(bt_buf) - 1) {
                bt_buf[bt_idx++] = c;
            }
        }

        //  READ SENSORS
        uint16_t ldr_raw   = ADC_Read(1);          // RA1 pin 3
        uint8_t  light_pct = ADC_to_percent(ldr_raw);
        uint8_t  smoke_det = !SMOKE_DO;              // RB3 
        uint8_t  flame_det = (FLAME_PIN == 0) ? 1 : 0; // RB2 active-low

        //  PWM LEDs  inverse of brightness
        uint16_t duty = 1023 - ldr_raw;
        PWM_SetDuty(duty);

    
        //  FLAME LED flinch
        FLAME_LED = flame_det;

        //  OLED UPDATE  only when values change
        if (light_pct != prev_light || smoke_det != prev_smoke ||
            flame_det != prev_flame || fan_on != prev_fan) {
            update_dashboard(light_pct, smoke_det, flame_det, fan_on);
            prev_light = light_pct;
            prev_smoke = smoke_det;
            prev_flame = flame_det;
            prev_fan   = fan_on;
        }

        __delay_ms(200);
    }
}