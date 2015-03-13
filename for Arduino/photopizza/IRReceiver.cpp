/**
 * File: IRReceiver.cpp
 * Created on: 26 feb 2015 г.
 * Description:
 * PhotoPizza DIY is an open source project of 360° product photography turntable.
 *
 * Author: Roman Savrulin <romeo.deepmind@gmail.com>
 * Project Author:
 * Project Site: PhotoPizza.org
 *
 * Copyright: 2015 MakerDrive
 * Copying permission statement:
 *  This file is part of PhotoPizza DIY.
 *
 *  PhotoPizza DIY is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "IRReceiver.h"
#include <Arduino.h>
#include "defines.h"

//TODO: convert to class

///////////  IR
#define IR_BIT_LENGTH 32    // number of bits sent by IR remote
#define FirstLastBit 15     // divide 32 bits into two 15 bit chunks for integer variables. Ignore center two bits. they are all the same.
#define BIT_1 1500          // Binary 1 threshold (Microseconds)
#define BIT_0 450           // Binary 0 threshold (Microseconds)
#define BIT_START 4000      // Start bit threshold (Microseconds)

#define IR_PIN 2            // IR Sensor pin

#define IR_TIMEO 100000L    //

/*
 IR pulses encode binary "0" as a short pulse, and binary "1"
 as a long pulse.  Given an array containing pulse lengths,
 convert this to an array containing binary values
 */

static bool prvPulseToKey(volatile int pulse[], int *bits);

static volatile int pulses[IR_BIT_LENGTH];
static volatile char arrPos = 0;
static volatile unsigned long lastPulseTime = 0;
static volatile bool ready = false;
static volatile bool reading = false;

static void prvIRQ();

///////////  IR RECEIVER
static void prvDump() {
  for (char i = 0; i < IR_BIT_LENGTH; i++) {
    Serial.println((String) (int) i + F(": ") + pulses[i]);
  }
}

int IrGetKey() {
  int key = 0;
  if (ready) {
    //Serial.println("RDY");
    if (prvPulseToKey(pulses, &key)) {
      Serial.println((String) F("IR ReadCode: ") + key);
    } else
      Serial.println(F("IR ReadCode: Parsing error"));
    arrPos = 0;
    ready = false;
  }

  return key;
}

void IrInit() {
  pinMode(IR_PIN, INPUT_PULLUP);
  attachInterrupt(0, prvIRQ, CHANGE);
  interrupts();
}

static void prvIRQ() {
  if (ready) { //skip all frames if current key is not read yet
    //TODO: inc stat. frame overrun
    //Serial.println("BSY (IRQ)");
    return;
  }
  unsigned long pulseTime = micros();
  int state = digitalRead(IR_PIN);

  if (!reading) { //receiving new packet
    if (arrPos == 0 && state) {
      //Serial.println("FE");
      //TODO: inc Sat frame start error;
      lastPulseTime = pulseTime;
      return;
    }
    reading = true;
    lastPulseTime = pulseTime;
    return;
  }

  if (lastPulseTime > pulseTime) {
    //Serial.println("OVR (IRQ)");
    //TODO: inc stat. TimeOverflow, reset frame or calc correct value
    arrPos = 0;
    lastPulseTime = pulseTime;
    return;
  }

  unsigned long timeo = pulseTime - lastPulseTime;
  if (reading && (timeo > IR_TIMEO)) {
    //Serial.println((String) "Timeo: " + timeo);
    //TODO: stat timeo increase
    reading = false;
    arrPos = 0;
    lastPulseTime = pulseTime;
    return;
  }

  if (arrPos == 0 && state) { //skip start bit
    lastPulseTime = pulseTime;
    return;
  }

  if (reading && state == 1) { //do not count time of low pulses
    lastPulseTime = pulseTime;
    return;
  }

  pulses[arrPos] = pulseTime - lastPulseTime;

  if (pulses[arrPos] < BIT_START) //skip long impulses
    ++arrPos;

  if (arrPos >= IR_BIT_LENGTH) {
    //Serial.println("LEN");
    ready = true;
    reading = false;
    //TODO: inc stat frames recieved, process data
    arrPos = 0;
  }
  lastPulseTime = pulseTime;
}

static bool prvPulseToKey(volatile int pulse[], int *bits) {
  int result = 0;
  int seed = 1;
  bool bit = false;
  for (int i = 0; i < IR_BIT_LENGTH; i++) {

    if (pulse[i] > BIT_1) //is it a 1?
    {
      bit = true;
    } else if (pulse[i] > BIT_0) //is it a 0?
    {
      bit = false;
    } else //data is invalid...
    {
      return false;
    }

    if( i >= (IR_BIT_LENGTH - FirstLastBit)) {
      if (bit) {
        result += seed;
      }
      seed *= 2;
    }

  }
  *bits = result;
  return true;
}

