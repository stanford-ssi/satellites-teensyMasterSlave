#include <SPI.h>
unsigned int SYNC=3;
unsigned int SMPL_CLK=29;
unsigned int CS0=35;
unsigned int CS1=37;
unsigned int CS2=7;
unsigned int CS3=2;
unsigned int DRL=26; //50
unsigned int derp=0;
// sample frequency in Hz
unsigned int SAMPLE_FREQ = 4000;
// long to count microseconds (as reported by micros) since last sample ready
unsigned long timer_count = 0;
// long to hold 32 bit signed sample value read from ADC
signed long SAMPLE=0;
bool SAMPLE_ON = false;
//LTC2500 control word
// [11:10] 10 required for valid control word
// [9] 0 digital gain comprssion
// [8] 0 digital gain expansion
// [7:4] 1100 4096 averaging
// [3:0] 0001 filter type "Sinc^1"
// 4 extra blank bits to make 16 bit word for 2x SPI transfers
uint16_t CONTROL_WORD = 0b1000110000010000;
void capture();
void stupidLoop();
void stupidSetup() {
  Serial.begin(115200);
  // enables +7V, -7V and +160V on avionics board
  pinMode(54, OUTPUT);
  pinMode(52, OUTPUT);
  pinMode(53, OUTPUT);
  digitalWrite(54, HIGH);
  digitalWrite(52, HIGH);
  digitalWrite(53, HIGH);
  pinMode(SYNC, OUTPUT);
  digitalWrite(SYNC,LOW);
  pinMode(SMPL_CLK, OUTPUT);
  pinMode(CS0, OUTPUT);
  pinMode(CS1, OUTPUT);
  pinMode(CS2, OUTPUT);
  pinMode(CS3, OUTPUT);
  digitalWriteFast(CS0, HIGH);
  digitalWriteFast(CS1, HIGH);
  digitalWriteFast(CS2, HIGH);
  digitalWriteFast(CS3, HIGH);
  pinMode(DRL, INPUT_PULLUP);
  attachInterrupt(DRL, capture, FALLING);
  Serial.println("Wassup bby");
  SPI.begin();
  digitalWrite(CS0, LOW);
  digitalWrite(CS1, LOW);
  digitalWrite(CS2, LOW);
  digitalWrite(CS3, LOW);
  delayMicroseconds(5);
  SPI.transfer16(CONTROL_WORD);
  delayMicroseconds(5);
  digitalWrite(CS0, HIGH);
  digitalWrite(CS1, HIGH);
  digitalWrite(CS2, HIGH);
  digitalWrite(CS3, HIGH);

  // set up PWM to drive sample clock
  // 50% duty cycle and desired frequency in Hz
  // start with NO SAMPLE CLOCK, turn on via serial command
  analogWriteFrequency(SMPL_CLK, SAMPLE_FREQ);
  analogWrite (SMPL_CLK,0);
  while (1) {
      stupidLoop();
  }
}
void capture(void) {
  //Sync filters
//  digitalWrite(SYNC, LOW);
//  delayMicroseconds(1);
//  digitalWrite(SYNC, HIGH);
//  delayMicroseconds(1);
//  digitalWrite(SYNC, LOW);
// how long since last sample?
derp = timer_count;
timer_count = micros();
derp = timer_count - derp;
//Serial.printf("%d microseconds since last DRL\n", derp);
  // read values
  digitalWrite(CS2, LOW);
  derp = SPI.transfer16(0x0000);
  SAMPLE = (derp << 16);
  derp = SPI.transfer16(0x0000);
  SAMPLE = SAMPLE + derp;
  derp = 0;
  Serial.printf("Sample A %d\n", SAMPLE);
  digitalWrite(CS2, HIGH);
  delayMicroseconds(5);
  digitalWrite(CS3, LOW);
  derp = SPI.transfer16(0x0000);
  SAMPLE = (derp << 16);
  derp = SPI.transfer16(0x0000);
  SAMPLE = SAMPLE + derp;
  derp = 0;
  Serial.printf("Sample B %d\n", SAMPLE);
  digitalWrite(CS3, HIGH);
  delayMicroseconds(5);
  digitalWrite(CS0, LOW);
  derp = SPI.transfer16(0x0000);
  SAMPLE = (derp << 16);
  derp = SPI.transfer16(0x0000);
  SAMPLE = SAMPLE + derp;
  derp = 0;
  Serial.printf("Sample C %d\n", SAMPLE);
  digitalWrite(CS0, HIGH);
  delayMicroseconds(5);
  digitalWrite(CS1, LOW);
  derp = SPI.transfer16(0x0000);
  SAMPLE = (derp << 16);
  derp = SPI.transfer16(0x0000);
  SAMPLE = SAMPLE + derp;
  derp = 0;
  Serial.printf("Sample D %d\n", SAMPLE);
  digitalWrite(CS1, HIGH);
}
void stupidLoop() {
  if (Serial.available()) {
    char c = Serial.read();
    // single toggle sample clock
    // rewrite each ADC control word
    // seems to work, programs all 4 ADC's with new settings (in control word register, see LTC2500 datasheet)
    if (c == 'r') {
      noInterrupts()
      if (SAMPLE_ON){
        analogWrite(SMPL_CLK,0);
        SAMPLE_ON = false;
      }
      digitalWrite(SYNC,HIGH);
      delayMicroseconds(10);
      digitalWrite(SYNC,LOW);
      delayMicroseconds(10);
      digitalWrite(CS0, LOW);
      digitalWrite(CS1, LOW);
      digitalWrite(CS2, LOW);
      digitalWrite(CS3, LOW);
      delayMicroseconds(5);
      SPI.transfer16(CONTROL_WORD);
      delayMicroseconds(5);
      digitalWrite(CS0, HIGH);
      digitalWrite(CS1, HIGH);
      digitalWrite(CS2, HIGH);
      digitalWrite(CS3, HIGH);
      interrupts();
    } // end if char = r for rewrite ADC control word

    // start / stop sampling
    if (c == 's'){
      // turn off sampling
      if(SAMPLE_ON){
        analogWrite(SMPL_CLK, 0);
        SAMPLE_ON = false;
      } else { // else turn on sampling
        analogWrite(SMPL_CLK, 1);
        SAMPLE_ON = true;
      } // end else sample on
    } // end if sample on / off toggle
  }

  if (micros() % 10000000 == 0) {
    Serial.println("Still kicking!");
  } // end if 10s heartbeat
}
