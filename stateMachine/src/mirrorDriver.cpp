/*
 * Teensy 3.2 SPI test to control Mirrorcle PicoAmp MEMS mirror driver board.
 * PicoAmp board is based on AD5664R quad 16 bit DAC with SPI input.
 * Michael Taylor
 * Aug 8th(ish) 2017
 *
 * This program generates a varying sinusoid in X and Y, initializes the SPI, initializes the DAC, sets a period
 * then plays back the sinusoid over the MEMS driver.
 */
#include <main.h>
#define BUFFER_LEN 1024
static float sine_wave[BUFFER_LEN]; // buffer to hold decimal value sine wave (scaled to uint16_t before sent to DAC)
static uint8_t DAC_write_word[3]; // DAC input register is 24 bits, SPI writes 8 bits at a time. Need to queue up 3 bytes (24 bits) to send every time you write to it
// Teensy 3.2 pinouts
const int slaveSelectPin = 43;
const int DRIVER_HV_EN_pin = 54;
const int LASER_EN_PIN = 22; // toggles laser on / off for modulation (PWM pin)
bool laser_state = LOW;
int count = 0;
int freq_x = 1;
int freq_y = 1;
float ampl_x = 1.0;
float ampl_y = 1.0;
int signal_x = 0;
int signal_y = 0;
uint16_t DAC_ch_A = 32768;
uint16_t DAC_ch_B = 32768;
uint16_t DAC_ch_C = 32768;
uint16_t DAC_ch_D = 32768;
float twopi = 2*3.14159265359; // good old pi
float phase = twopi/BUFFER_LEN; // phase increment for sinusoid
void mirrorDriverSetup() {
    // Setup Pins:
    pinMode (slaveSelectPin, OUTPUT); // SPI SS pin 10
    pinMode (DRIVER_HV_EN_pin, OUTPUT); // driver board high voltage output enable pin 8
    pinMode (LASER_EN_PIN, OUTPUT); // Laser diode switching pin 7, active low.
    // write pins low
    digitalWrite(slaveSelectPin,LOW);
    digitalWrite(DRIVER_HV_EN_pin,LOW);
    analogWrite(LASER_EN_PIN,0);
    // setup SPI
    debugPrintln("Setting up SPI");
    // initialize SPI:
    SPI2.begin();
    // setup DAC
    Serial.println("Setting up DAC");
    //  send the DAC write word one byte at a time:
    // 0x280001 FULL_RESET
    digitalWrite(slaveSelectPin,LOW);
    SPI2.transfer(0x28);
    SPI2.transfer(0x00);
    SPI2.transfer(0x01);
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(1);
    // 0x380001 INT_REF_EN
    digitalWrite(slaveSelectPin,LOW);
    SPI2.transfer(0x38);
    SPI2.transfer(0x00);
    SPI2.transfer(0x01);
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(1);
    // 0x20000F DAC_EN_ALL
    digitalWrite(slaveSelectPin,LOW);
    SPI2.transfer(0x20);
    SPI2.transfer(0x00);
    SPI2.transfer(0x0F);
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(1);
    // 0x300000 LDAC_EN
    digitalWrite(slaveSelectPin,LOW);
    SPI2.transfer(0x30);
    SPI2.transfer(0x00);
    SPI2.transfer(0x00);
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(1);

    // Fill the sine wave buffer with 1024 points
    // The 0.9 here prevents the driver from ever going over 150V! (It can do ~160 which can damage mirror)
    debugPrintln("Filling in sine wave");
    for (int i = 0; i < BUFFER_LEN; i++){
        sine_wave[i] = sin(i*phase); // fill 0 to twopi phase, output range -0.9 to 0.9 to limit HV output
    }
    // Start the LASER PWM timer
    // 50% duty cycle at desired frequency
    analogWriteFrequency(LASER_EN_PIN, 1000);
    analogWrite (LASER_EN_PIN,0);
}

void sendMirrorOutput(const mirrorOutput& out) {
    int16_t x = out.x * 0.9; // Important: never max out the mirror
    int16_t y = out.y * 0.9;
    // figure out the sine wave output for each channel, from -1,1 to uint16_t centered at 32768
    // X = A - B
    DAC_ch_A = (uint16_t) (32768*(1 + (ampl_x * x / 2)));
    DAC_ch_B = (uint16_t) (32768*(1 - (ampl_x * x / 2)));
    // Y = C - D
    // 90 degree phase shift on Y for a circle>?
    DAC_ch_C = (uint16_t) (32768*(1 + (ampl_y * y / 2)));
    DAC_ch_D = (uint16_t) (32768*(1 - (ampl_y * y / 2)));

  /* HERE BE STUPID */
    // Write all 4 channels 1 at a time
    DAC_write_word[0] = 0b00011000; // write to and update (C = 011) channel A DAC (A = 000), first 2 bits dont care.
    DAC_write_word[1] = DAC_ch_A >> 8;     // MSB
    DAC_write_word[2] = DAC_ch_A & 0x00FF; // LSB

    // manually toggle SS pin (blergh)
    digitalWrite(slaveSelectPin,LOW);
    //  send the DAC write word one byte at a time:
    SPI2.transfer(DAC_write_word[0]);
    SPI2.transfer(DAC_write_word[1]);
    SPI2.transfer(DAC_write_word[2]);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(10);
    DAC_write_word[0] = 0b00011001; // write to and update (C = 011) channel B DAC (B = 001), first 2 bits dont care.
    DAC_write_word[1] = DAC_ch_B >> 8;     // MSB
    DAC_write_word[2] = DAC_ch_B & 0x00FF; // LSB

    // manually toggle SS pin (blergh)
    digitalWrite(slaveSelectPin,LOW);
    //  send the DAC write word one byte at a time:
    SPI2.transfer(DAC_write_word[0]);
    SPI2.transfer(DAC_write_word[1]);
    SPI2.transfer(DAC_write_word[2]);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(10);
    DAC_write_word[0] = 0b00011010; // write to and update (C = 011) channel C DAC (C = 010), first 2 bits dont care.
    DAC_write_word[1] = DAC_ch_C >> 8;     // MSB
    DAC_write_word[2] = DAC_ch_C & 0x00FF; // LSB

    // manually toggle SS pin (blergh)
    digitalWrite(slaveSelectPin,LOW);
    //  send the DAC write word one byte at a time:
    SPI2.transfer(DAC_write_word[0]);
    SPI2.transfer(DAC_write_word[1]);
    SPI2.transfer(DAC_write_word[2]);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(10);
    DAC_write_word[0] = 0b00011011; // write to and update (C = 011) channel D DAC (D = 011), first 2 bits dont care.
    DAC_write_word[1] = DAC_ch_D >> 8;     // MSB
    DAC_write_word[2] = DAC_ch_D & 0x00FF; // LSB

    // manually toggle SS pin (blergh)
    digitalWrite(slaveSelectPin,LOW);
    //  send the DAC write word one byte at a time:
    SPI2.transfer(DAC_write_word[0]);
    SPI2.transfer(DAC_write_word[1]);
    SPI2.transfer(DAC_write_word[2]);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(10);
}

void highVoltageEnable(bool enable) {
    if (enable) {
        digitalWrite(DRIVER_HV_EN_pin,HIGH);
    } else {
        digitalWrite(DRIVER_HV_EN_pin,LOW);
    }
}

void laserEnable(bool enable) {
    if (enable) {
        laser_state = HIGH;
        analogWrite (LASER_EN_PIN, 128); // 50% duty cycle 1khz
    } else {
        laser_state = LOW;
        analogWrite (LASER_EN_PIN, 0);
    }
}
