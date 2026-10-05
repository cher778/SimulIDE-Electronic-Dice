// 4位共阴数码管倒计时 74HC595驱动 + LED报警 + 有源蜂鸣器
// 外部无上拉电阻版本：按键直接接 Arduino 引脚到 GND
// 代码中使用 INPUT_PULLUP 让 Arduino 内部上拉

const int DS = 2;
const int SHCP = 3;
const int STCP = 4;
const int bitPins[] = {5, 6, 7, 8};  // n1,n2,n3,n4
const int LED = 10;
const int BUZZER = 9;  // 有源蜂鸣器
const int KEY_SHIFT = 11;
const int KEY_ADD = 12;
const int KEY_SUB = 13;
const int KEY_START_STOP = A0;  // 启动/停止按钮（内部上拉）

// 共阴数码管段码 (gfedcba)
byte segCode[] = {
  0x3F,  // 0: 0011 1111
  0x06,  // 1: 0000 0110
  0x5B,  // 2: 0101 1011
  0x4F,  // 3: 0100 1111
  0x66,  // 4: 0110 0110
  0x6D,  // 5: 0110 1101
  0x7D,  // 6: 0111 1101
  0x07,  // 7: 0000 0111
  0x7F,  // 8: 0111 1111
  0x6F   // 9: 0110 1111
};

enum STATE { SET, RUN, ALARM };
STATE sysState = SET;

int digit[4] = {0, 0, 0, 0};
int selBit = 0;
unsigned int totalSec;

unsigned long lastSecTime = 0;
unsigned long blinkTimer = 0;
bool blinkFlag = true;
unsigned long keyTimer = 0;
const unsigned long keyDebounce = 50;

unsigned long shiftPressStart = 0;
bool shiftKeyHold = false;
bool alreadyStarted = false;
const unsigned long holdTime = 1000;

static bool lastAddKey = false;
static bool lastSubKey = false;
static bool lastStartStopKey = false;

unsigned long alarmEndTime = 0;
const unsigned long ALARM_DURATION = 10000;

void setup() {
  pinMode(DS, OUTPUT);
  pinMode(SHCP, OUTPUT);
  pinMode(STCP, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(bitPins[i], OUTPUT);
    digitalWrite(bitPins[i], HIGH);
  }

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // 内部上拉：按键按下时为 LOW，未按下时为 HIGH
  pinMode(KEY_SHIFT, INPUT_PULLUP);
  pinMode(KEY_ADD, INPUT_PULLUP);
  pinMode(KEY_SUB, INPUT_PULLUP);
  pinMode(KEY_START_STOP, INPUT_PULLUP);

  sendSeg(0x00);
  updateTotalSec();
}

void updateTotalSec(){
  totalSec = digit[0] * 1000 + digit[1] * 100 + digit[2] * 10 + digit[3];
}

void splitSecToDigit(unsigned int sec){
  digit[0] = sec / 1000;
  digit[1] = (sec / 100) % 10;
  digit[2] = (sec / 10) % 10;
  digit[3] = sec % 10;
}

void sendSeg(byte dat){
  digitalWrite(STCP, LOW);
  for (int i = 7; i >= 0; i--) {
    digitalWrite(SHCP, LOW);
    digitalWrite(DS, (dat >> i) & 1);
    digitalWrite(SHCP, HIGH);
  }
  digitalWrite(STCP, HIGH);
}

void display(){
  for (int i = 0; i < 4; i++) {
    if (sysState == SET && i == selBit && blinkFlag == false) {
      sendSeg(0x00);
    } else {
      sendSeg(segCode[digit[i]]);
    }

    digitalWrite(bitPins[i], LOW);
    delayMicroseconds(5000);
    digitalWrite(bitPins[i], HIGH);
    delayMicroseconds(200);
  }
}

void keyScan(){
  if (millis() - keyTimer < keyDebounce) return;
  keyTimer = millis();

  // INPUT_PULLUP 模式：按下时 LOW，未按下时 HIGH
  bool shiftKey = (digitalRead(KEY_SHIFT) == LOW);
  bool addKey = (digitalRead(KEY_ADD) == LOW);
  bool subKey = (digitalRead(KEY_SUB) == LOW);
  bool startStopKey = (digitalRead(KEY_START_STOP) == LOW);

  // shift键：短按切位，长按1秒启动
  if (shiftKey) {
    if (!shiftKeyHold) {
      shiftKeyHold = true;
      shiftPressStart = millis();
      alreadyStarted = false;
    }

    unsigned long pressDur = millis() - shiftPressStart;
    if (pressDur >= holdTime && sysState == SET && !alreadyStarted) {
      sysState = RUN;
      lastSecTime = millis();
      alreadyStarted = true;
    }
  } else {
    if (shiftKeyHold) {
      shiftKeyHold = false;
      unsigned long pressDur = millis() - shiftPressStart;
      if (pressDur < holdTime) {
        if (sysState == SET) {
          selBit++;
          if (selBit >= 4) selBit = 0;
        }
      }
    }
  }

  if (addKey && !lastAddKey) {
    if (sysState == SET) {
      digit[selBit]++;
      if (digit[selBit] > 9) digit[selBit] = 0;
      updateTotalSec();
    }
  }
  lastAddKey = addKey;

  if (subKey && !lastSubKey) {
    if (sysState == SET) {
      if (digit[selBit] > 0) {
        digit[selBit]--;
      } else {
        digit[selBit] = 9;
      }
      updateTotalSec();
    }
  }
  lastSubKey = subKey;

  // START/STOP键：只在 ALARM 状态下有效
  if (startStopKey && !lastStartStopKey) {
    if (sysState == ALARM) {
      sysState = SET;
      digitalWrite(LED, LOW);
      digitalWrite(BUZZER, LOW);
      splitSecToDigit(0);
    }
  }
  lastStartStopKey = startStopKey;
}

void loop() {
  keyScan();

  if (millis() - blinkTimer > 300) {
    blinkTimer = millis();
    blinkFlag = !blinkFlag;
  }

  if (sysState == RUN) {
    if (millis() - lastSecTime >= 1000) {
      lastSecTime = millis();
      if (totalSec > 0) {
        totalSec--;
        splitSecToDigit(totalSec);
      }
      if (totalSec == 0) {
        sysState = ALARM;
        alarmEndTime = millis();
      }
    }
    digitalWrite(LED, LOW);
    digitalWrite(BUZZER, LOW);
  }
  else if (sysState == ALARM) {
    if (millis() - alarmEndTime > ALARM_DURATION) {
      sysState = SET;
      digitalWrite(LED, LOW);
      digitalWrite(BUZZER, LOW);
      return;
    }

    if (blinkFlag) {
      digitalWrite(LED, HIGH);
      digitalWrite(BUZZER, HIGH);
    } else {
      digitalWrite(LED, LOW);
      digitalWrite(BUZZER, LOW);
    }
  }
  else if (sysState == SET) {
    digitalWrite(LED, LOW);
    digitalWrite(BUZZER, LOW);
  }

  display();
}
