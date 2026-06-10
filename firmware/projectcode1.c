#define _XTAL_FREQ 8000000
#include <xc.h>

// CONFIG
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

// LCD pins
#define RS RB0
#define EN RB1

// LED
#define LED RC0

// Motion sensor
#define MOTION PORTBbits.RB2

// I2C DS1307
#define SDA RC4
#define SCL RC3

// ---------------- LCD ----------------
void lcd_pulse() {
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_ms(2);
}

void lcd_cmd(unsigned char cmd) {
    RS = 0;
    PORTD = cmd;
    lcd_pulse();
}

void lcd_data(unsigned char data) {
    RS = 1;
    PORTD = data;
    lcd_pulse();
}

void lcd_init() {
    __delay_ms(20);
    lcd_cmd(0x38);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);
}

void lcd_string(const char *str) {
    while(*str) lcd_data(*str++);
}

void lcd_clear() {
    lcd_cmd(0x01);
}

// ---------------- I2C ----------------
void I2C_delay(){ __delay_us(5); }

void I2C_init(){
    TRISC3 = 0;
    TRISC4 = 0;
    SCL = 1;
    SDA = 1;
}

void I2C_start(){
    SDA = 1; SCL = 1; I2C_delay();
    SDA = 0; I2C_delay();
    SCL = 0;
}

void I2C_stop(){
    SDA = 0; SCL = 1; I2C_delay();
    SDA = 1; I2C_delay();
}

void I2C_write(unsigned char data){
    for(int i=0;i<8;i++){
        SDA = (data & 0x80) ? 1 : 0;
        SCL = 1; I2C_delay();
        SCL = 0;
        data <<= 1;
    }
    SCL = 1; I2C_delay();
    SCL = 0;
}

unsigned char I2C_read(){
    unsigned char data=0;
    SDA = 1;
    for(int i=0;i<8;i++){
        data <<= 1;
        SCL = 1; I2C_delay();
        if(SDA) data |= 1;
        SCL = 0;
    }
    return data;
}

// ---------------- TIME ----------------
unsigned char bcd_to_dec(unsigned char val){
    return (val/16)*10 + (val%16);
}

void DS1307_read(unsigned char *h, unsigned char *m){
    I2C_start();
    I2C_write(0xD0);
    I2C_write(0x00);
    I2C_start();
    I2C_write(0xD1);

    unsigned char sec = I2C_read();
    *m = I2C_read();
    *h = I2C_read();

    I2C_stop();

    *h = bcd_to_dec(*h);
    *m = bcd_to_dec(*m);
}

// ---------------- ADC ----------------
void ADC_Init() {
    ADCON0 = 0x41;
    ADCON1 = 0x82;
}

int ADC_Read(int channel) {
    ADCON0 &= 0xC5;
    ADCON0 |= (channel << 3);
    __delay_ms(2);
    GO_nDONE = 1;
    while(GO_nDONE);
    return ((ADRESH << 8) + ADRESL);
}

// ---------------- MAIN ----------------
void main() {

    TRISB = 0b00000100; // RB2 input
    TRISC = 0x00;
    TRISD = 0x00;
    TRISA = 0xFF;

    PORTC = 0;
    PORTD = 0;
    LED = 0;

    OPTION_REGbits.nRBPU = 0;

    lcd_init();
    ADC_Init();
    I2C_init();

    lcd_clear();
    lcd_string("System Starting");
    __delay_ms(2000);
    lcd_clear();
    lcd_string("Ready");
    __delay_ms(1500);
    lcd_clear();

    int timer = 0;
    int schedule = 0;
    int last_mm = -1;

    while(1) {

        // ---------------- TIME ----------------
        unsigned char hh, mm;
        DS1307_read(&hh, &mm);

        // ---------------- SCHEDULE ----------------
        if(hh == 16 && mm == 51) {
            schedule = 1;
        }

        if(hh == 16 && mm == 40) {
            schedule = 0;
        }

        // ---------------- SCHEDULE MODE ----------------
        if(schedule == 1) {

            LED = 1;
            lcd_clear();
            lcd_string("AUTO ON 16:39");

        }

        // ---------------- NORMAL MODE ----------------
        else {

            if(PORTBbits.RB2 == 0) {   // motion detected
                timer = 0;
                LED = 1;

                lcd_clear();
                lcd_string("Motion Detected");
            }
            else {

                if(timer < 30) {
                    timer++;
                }
                else {
                    LED = 0;   // ALWAYS FORCE OFF
                    lcd_clear();
                    lcd_string("Lights OFF");
                }
            }
        }
    }
}