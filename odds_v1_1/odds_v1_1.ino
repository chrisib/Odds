/* Sebsongs Modular ODDS | Firmware 1.1 
 * Random generative sequencer that quantise notes to a given scale and chord structure.
 */

#include <EEPROM.h>

/*  Array for defining musical scales the number of notes in each scale.
 *  Each row defines one choice of scale:
 *  - The first number is the number of notes in the scale.
 *  - The following 12 numbers define the notes in the scale defined as semitones.
 *  - If you want to change the scales, it is recommended that you copy the whole original array and paste it underneath, and give it another name, i.e. "scalesOriginal[91]".
 */

byte scales[91] = { 7, 0, 2, 4, 5, 7, 9,11, 0, 0, 0, 0, 0, // Major
                    7, 0, 2, 3, 5, 7, 8,10, 0, 0, 0, 0, 0, // Minor Natural
                    7, 0, 2, 3, 5, 7, 8,11, 0, 0, 0, 0, 0, // Minor Harmonic 
                    5, 0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0, 0, // Major Pentatonic
                    5, 0, 3, 5, 7,10, 0, 0, 0, 0, 0, 0, 0, // Minor Pentatonic
                    6, 0, 2, 4, 6, 8,10, 0, 0, 0, 0, 0, 0, // Whole Tone
                   12, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11  // Chromatic
                  };

// Hardware pin definitions
const int PIN_CV = 3;
const int PIN_RESET = 4;
const int PIN_TRIG = 5;
const int PIN_LOOP_SWITCH = 6;
const int PIN_GATE = 8;
const int PIN_SHIFT = 9;
const int PIN_PROB = A3;
const int PIN_SCALE = A2;
const int PIN_CV_IN = A1;
const int PIN_LOOP = A0;
const int PIN_INTERNAL_LED = 13;

// PWM CV output variables.
const byte PWM_BITMASK = B01111111;
const int PWM_FACTOR = 2;
const int PWM_SCALING = 239;
byte note = 0;

// Hysteresis variable for stabilizing analog input data.
int hysteresis = 3;

//Main counter and trig edge detection variables.
int ctr = 0;
bool trig = 0;
bool trigFlag = 0;

// Variables for detecting change of loop switch.
bool loopSwitch = 0;
int lastCase = 1;

// Variables for detecting change of shift switch.
bool rawShiftState = 0;
bool shiftState = 0;
bool previousShiftState = 0;

// Variables for storing loop length and detecting new values.
int loopLength = 0;
int rawLoop = 0;
int prevRawLoop = 0;

// Variables for storing probability and detecting new values.
int probability = 0;
int rawProb = 0;
int prevRawProb = 0;

// Variables for storing octave and detecting new values.
int octave = 0;
int prevOctave = 0;
int octaveProb = 0;

// Variables for storing CV input and detecting new values.
int cvInput = 0;
int rawCV = 0;
int prevRawCV = 0;

// Variables for storing and setting the chord structure.
int chord = 0;
int chordSelect = 0;
int chordStructure = 200;
int chordMultiplier = 7;

// Variables for storing and selecting scales and detecting new values.
int scaleSize = 0;
int rawScale = 0;
int prevRawScale = 0;
int scaleSelect = 0;

// Variables for storing offset values.
int offset = 0;
int scaleOffset = 0;

// Variable for storing finished note value before setting the CV output.
int makeNote = 0;

// Variable for checking for calibration mode (HOLD SHIFT AT START UP).
bool calibrationMode = 0;

// Variable for detecting of CV input has changed, set note to 0 (root note) plus octave if so.
bool cvInputChanged = 0;

// Main array for storing sequence.
byte loopBuffer[64] = {0};

// Look up table for CV input.
int cvInputLookup[61] = {0, 13, 30, 47, 64, 81, 97, 115, 131, 149, 165, 182, 
                         199, 217, 234, 251, 268, 285, 301, 317, 335, 353, 369, 387, 
                         404, 420, 437, 455, 472, 489, 505, 523, 539, 556, 573, 590, 
                         607, 624, 640, 657, 674, 690, 707, 724, 740, 756, 771, 787, 
                         803, 818, 832, 847, 860, 874, 887, 899, 911, 921, 931, 940, 
                         948, 
                        };

void setup() {
  //Blink the internal LED four times to confirm that firmware upload went well.
  pinMode(PIN_INTERNAL_LED, OUTPUT);
  delay(500);
  digitalWrite(PIN_INTERNAL_LED, HIGH);
  delay(100);
  digitalWrite(PIN_INTERNAL_LED, LOW);
  delay(200);
  digitalWrite(PIN_INTERNAL_LED, HIGH);
  delay(100);
  digitalWrite(PIN_INTERNAL_LED, LOW);
  delay(200);
  digitalWrite(PIN_INTERNAL_LED, HIGH);
  delay(100);
  digitalWrite(PIN_INTERNAL_LED, LOW);
  delay(200);
  digitalWrite(PIN_INTERNAL_LED, HIGH);
  delay(100);
  digitalWrite(PIN_INTERNAL_LED, LOW);

  // Set up pins at startup.
  pinMode(PIN_CV, OUTPUT);
  pinMode(PIN_GATE, OUTPUT);
  pinMode(PIN_TRIG, INPUT);
  pinMode(PIN_PROB, INPUT);
  pinMode(PIN_SCALE, INPUT);
  pinMode(PIN_LOOP, INPUT);
  pinMode(PIN_CV_IN, INPUT);
  pinMode(PIN_LOOP_SWITCH, INPUT);
  pinMode(PIN_SHIFT, INPUT_PULLUP);

  // Setup PWM output to high frequency and correct scaling.
  TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(WGM22) | _BV(CS20);
  OCR2A = PWM_SCALING;
  OCR2B = 0;

  // Seed the random function with analog input for better random generation.
  randomSeed(analogRead(A5));

  // Read the sequence saved in EEPROM into the loopBuffer for instant playback of last saved sequence.
  for (int i = 0; i < 64; i++) {
    loopBuffer[i] = EEPROM.read(i);
  }

  // Update all analog inputs.
  rawProb = analogRead(PIN_PROB);
  rawScale = analogRead(PIN_SCALE);
  rawLoop = analogRead(PIN_LOOP);
  rawCV = analogRead(PIN_CV_IN);

  // Set the musical scale to what ever the pot is set to.
  scaleSelect = potScaling(rawScale) * 13;
  scaleSize = scales[scaleSelect];

  // Set the loop length to what ever the pot is set to.
  loopLength = potScaling13(rawLoop);

  // Set the probability to what ever the pot is set to.
  probability = map(rawProb, 0, 1023, 0, 100);

  // Check if SHIFT switch is pressed at startup, used for calibration mode.
  rawShiftState = digitalRead(PIN_SHIFT);

  if(rawShiftState != previousShiftState){
    shiftState = rawShiftState;
    previousShiftState = rawShiftState;
  }

  // Wait a little for shift state to settle.
  delay(10);

  // Check if SHIFT switch is pressed at startup, if so, enter calibration mode. If not, enter normal mode.
  if(shiftState){
    calibrationMode = 1;

    // Blink the gate LED twice to indicate that calibration mode is active.
    digitalWrite(PIN_GATE, HIGH);
    delay(50);
    digitalWrite(PIN_GATE, LOW);
    delay(250);
    digitalWrite(PIN_GATE, HIGH);
    delay(50);
    digitalWrite(PIN_GATE, LOW);
  }

}

void read_inputs() {
  // Update all digital inputs.
  trig = digitalRead(PIN_TRIG);
  loopSwitch = digitalRead(PIN_LOOP_SWITCH);
  rawShiftState = digitalRead(PIN_SHIFT);

  // Check if SHIFT switch is pressed.
  if(rawShiftState != previousShiftState){
    shiftState = rawShiftState;
    Serial.print("Shift state: ");
    Serial.println(shiftState);
    previousShiftState = rawShiftState;
  }

  // Update all analog inputs.
  rawProb = analogRead(PIN_PROB);
  rawScale = analogRead(PIN_SCALE);
  rawLoop = analogRead(PIN_LOOP);
  rawCV = analogRead(PIN_CV_IN);
}

void loop_calibration() {
  // Check if probability potentiometer is being changed.
  if(rawProb < prevRawProb - hysteresis || rawProb >= prevRawProb + hysteresis){

    // Set the octave parameter according to the potentiometer.
    octave = potScaling(rawProb);

    // Check if there's a change in value of the potentiometer.
    if(octave != prevOctave){
      
      // Set the CV output to octaves (1V/Oct) according to the potentiometer, for easy calibration of the OFFSET and GAIN trimmers on PCB.
      // Also trigger the gate out every time a new octave is detected from the potentiometer.
      digitalWrite(PIN_GATE, HIGH);
      
      pinMode(PIN_CV, OUTPUT);
      
      TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
      TCCR2B = _BV(WGM22) | _BV(CS20);
      OCR2A = PWM_SCALING;
      OCR2B = 0;
      OCR2B = (octave*12 & PWM_BITMASK) * PWM_FACTOR;
      prevOctave = octave;
    }
    prevRawProb = rawProb;
  }
  else{
    
    //In the case of no new octave values, turn off the gate output.
    digitalWrite(PIN_GATE, LOW);
  }
}

void loop_normal() {
  // Check if there's a new trig on the trig input.
  if(trig && !trigFlag){
    trigFlag = 1;

    // Check if the main counter is higher than the current loop length, if so reset counter to zero.
    if(ctr > loopLength){
      ctr = 0;
    }

    // Main state machine, checks if LOOP switch is on or off.
    if (loopSwitch) {
      play_old_notes();
    }
    else {
      generate_new_notes();
    }
    
    // Increment the main counter.
    ctr++;
  }
  
  // If there's not a trig on the trig input, update all potentiometers and set the appropriate values.
  // This is done here to prioritize timing and snappiness of the sequencer trigging.
  else if(!trig && trigFlag){
    trigFlag = 0;
    // If there's not trig on the trig input, set the gate output to low.
    digitalWrite(PIN_GATE, LOW);

    // Check if there are movement on the probability potentiometer. Set Probability if there is, OR set Octave if SHIFT is held.
    if(rawProb < prevRawProb - hysteresis || rawProb >= prevRawProb + hysteresis){
      if(shiftState){
        octave = potScaling(rawProb);
      }
      else{
        probability = map(rawProb, 0, 1023, 0, 100);
      }
      prevRawProb = rawProb;
    }

    // Check if there are movement on the scale potentiometer. Set Scale if there is, OR set Chord structure if SHIFT is held.
    if(rawScale < prevRawScale - hysteresis || rawScale >= prevRawScale + hysteresis){
      if(shiftState){
        potScalingChord(rawScale);
      }
      else{
        scaleSelect = potScaling(rawScale) * 13;
        scaleSize = scales[scaleSelect];
      }
      prevRawScale = rawScale;
    }

    // Check if there are movement on the loop length potentiometer. Set Loop Length if there is, OR set Loop Offset if SHIFT is held.
    if(rawLoop < prevRawLoop - hysteresis || rawLoop >= prevRawLoop + hysteresis){
      if(shiftState){
        offset = map(rawLoop, 0, 1023, 0, 63);
      }
      else{
        loopLength = potScaling13(rawLoop);
      }
      prevRawLoop = rawLoop;
    }

    // Check if there is change on the CV input. Set the CV input variable accordingly if so.
    if(rawCV < prevRawCV - hysteresis || rawCV >= prevRawCV + hysteresis){ 

      // Check the lookup table for the CV input values. This is not linear due to circuitry on the PCB, therefore the need of look up table.
      for (int i = 0; i <= 60; i++) {
        // Check for values within a range of +/- 3.
        if(rawCV > cvInputLookup[i] - 6 && rawCV < cvInputLookup[i] + 6){
          // If the scale size is 7, move the F key up one key to compensate for the uneven math. This makes it possible to use only white keys when transposing.
          if(scaleSize == 7 && (i == 5 || i == 17 || i == 29 || i == 41 || i == 53)){
            i++; 
          }
          // Scale the CV input value to the size of the scale. This makes sure that octaves are always at whole Volts.
          cvInput = i * scaleSize / 12;

          // Flag that CV input has been changed, next note should be root note so it is easier to hear which chord is being played.
          cvInputChanged = 1;
          
          break;
        }
      }
      prevRawCV = rawCV;
    }
  }
}

void loop() {
  read_inputs();

  // Check if calibration mode is active.
  // Run the appropriate worker function
  if(calibrationMode){
    loop_calibration();
  }
  else{
    loop_normal(); 
  }
  
  delay(1);
}

void generate_new_notes() {
  // Check if a random number between 0-100 is smaller than value of probability potentiometer.
  if(random(101) < probability){

    // If CV input has not changed, generate a random note within the chosen chord structure.
    if(cvInputChanged == 0){
      chord = ((random(chordStructure)/100)*chordMultiplier) + cvInput; //Chord structure
    }
    // If CV input has just changed, set the root note of the chord in this instance.
    else{
      chord = 0 + cvInput; //Chord structure
      cvInputChanged = 0;
    }
    
    // If CV input is used, this calculation makes sure that the scale is always held within the first octave.
    scaleOffset = chord - ((chord / scaleSize)*scaleSize);

    // Create note within chosen scale and offset it with chosen octave range (which is also randomly generated within a range.)
    makeNote = scales[scaleOffset+scaleSelect+1] + 12*(map(random(7),0,6,constrain(octave-2, 0, 4),octave));

    // Make the final note, and constrain it to maximum CV range (0-7 Volts or 0-84 notes)
    note = constrain(makeNote + ((chord / scaleSize)*12), 0 , 84);
    
    // Save the note in loopBuffer and mask off the gate value in the LSB.
    loopBuffer[(ctr + offset) % 64] = note & PWM_BITMASK;

    // Set the LSB to 1 to store a high gate value.
    bitWrite(loopBuffer[(ctr + offset) % 64], 7, 1);

    // Set the gate output to HIGH to activate the gate output.
    digitalWrite(PIN_GATE, HIGH);

    // Configure the CV output.
    pinMode(PIN_CV, OUTPUT);
    TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(WGM22) | _BV(CS20);
    OCR2A = PWM_SCALING;
    OCR2B = 0;
    // Set the CV output to last generated note.
    OCR2B = (note & PWM_BITMASK) * PWM_FACTOR;

  }
  else{
    // If no note was generated, save the last generated note value to loopBuffer and set the gate output to low.
    loopBuffer[ctr + offset] = note & PWM_BITMASK;
    bitWrite(loopBuffer[(ctr + offset) % 64], 7, 0);
  }

  // Case detection.
  lastCase = 0;
}

void play_old_notes() {
  // Check if LOOP switch just was just pressed.
  if(lastCase == 0){

    // Turn off the gate output to avoid wrong notes being played back.
    digitalWrite(PIN_GATE, bitRead(0, 7));

    // Save the loop buffer to EEPROM in order to retrieve it at next power on. This only saves the length of loop length value.
    for (int i = offset; i <= loopLength + offset; i++) {
      
      EEPROM.write(i % 64, loopBuffer[i % 64]);
    }

    // Case detection.
    lastCase = 1;
  }
  else{

    // Read the stored seqeunce off the EEPROM and set GATE and CV outputs accordingly.
    digitalWrite(PIN_GATE, bitRead(EEPROM.read((ctr + offset) % 64), 7));
    pinMode(PIN_CV, OUTPUT);
      
    TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(WGM22) | _BV(CS20);
    OCR2A = PWM_SCALING;
    OCR2B = 0;
    OCR2B = (EEPROM.read(((ctr + offset) % 64) & PWM_BITMASK) * PWM_FACTOR);
  }
}

// Function for scaling potentiometers to panel graphics.
int potScaling(int input){
  int result;

  if(input < 10){
    result = 0;
  }
  else if(input >= 10 && input < 175){
    result = 1;  
  }
  else if(input >= 175 && input < 375){
    result = 2;  
  }
  else if(input >= 375 && input < 600){
    result = 3;  
  }
  else if(input >= 600 && input < 800){
    result = 4;  
  }
  else if(input >= 800 && input < 1000){
    result = 5;  
  }
  else if(input >= 1000){
    result = 6;  
  }

  return result;
};

// Function for scaling potentiometers to panel graphics.
void potScalingChord(int input){
  int result;

  if(input < 10){
    chordStructure = 200;
    chordMultiplier = 7;
  }
  else if(input >= 10 && input < 175){
    chordStructure = 200;
    chordMultiplier = 2;
  }
  else if(input >= 175 && input < 375){
    chordStructure = 200;
    chordMultiplier = 4;
  }
  else if(input >= 375 && input < 600){
    chordStructure = 300;
    chordMultiplier = 2;
  }
  else if(input >= 600 && input < 800){
    chordStructure = 400;
    chordMultiplier = 2;
  }
  else if(input >= 800 && input < 1000){
    chordStructure = 300;
    chordMultiplier = 4;
  }
  else if(input >= 1000){
    chordStructure = 1200;
    chordMultiplier = 1;
  }

};

// Function for scaling potentiometers to panel graphics.
int potScaling13(int input){
  int result;

  if(input < 5){
    result = 0;
  }
  else if(input >= 5 && input < 30){
    result = 1;  
  }
  else if(input >= 30 && input < 130){
    result = 2;  
  }
  else if(input >= 130 && input < 230){
    result = 3;  
  }
  else if(input >= 230 && input < 350){
    result = 4;  
  }
  else if(input >= 350 && input < 430){
    result = 5;  
  }
  else if(input >= 430 && input < 570){
    result = 6;  
  }
  else if(input >= 570 && input < 650){
    result = 7;  
  }
  else if(input >= 650 && input < 750){
    result = 11;  
  }
  else if(input >= 750 && input < 850){
    result = 15;  
  }
  else if(input >= 850 && input < 950){
    result = 23;  
  }
  else if(input >= 950 && input < 1015){
    result = 31;  
  }
  else if(input >= 1015){
    result = 63;
  }

  return result;
};
