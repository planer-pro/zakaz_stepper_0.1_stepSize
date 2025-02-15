#include <Arduino.h>
#include <EncButton.h>
#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>
#include <EEPROM.h>

enum
{
    ccw,
    cw
};

enum
{
    set_mode,
    loaded_mode,
    work_mode,
    pause_mode
};

#define START_BTN_PIN 8 // Пин для кнопки START
#define MENU_ENC_PIN 6  // Пин для кнопки ENC
#define ENCA_PIN 4      // Пин ENCA
#define ENCB_PIN 5      // Пин ENCB

#define EN_STEPPER_PIN 7           // Пин для включения шагового двигателя
#define STEP_PIN 2                 // Пин шага
#define DIR_PIN 3                  // Пин направления
#define STEPS_PER_REV 200 * 16     // 3200 шагов/оборот (для микрошага 1/16)
#define MIN_SPEED 0.1              // Минимальная скорость в об/мин
#define MAX_SPEED 6.0              // Максимальная скорость в об/мин
#define MAX_TIME_H 9999            // Максимальное время в часах
#define MOTOR_FREEZE_ON_STOP false // Удерживание мотора при STOP
#define MOTOR_FREEZE_ON_PAUSE true // Удерживание мотора при PAUSE
#define MOTOR_DIRECTION cw         // Направление вращения по часовой стрелке

uint8_t displayMode = loaded_mode; // Начальный режим отображения
uint8_t tmS = 0, tmH = 0, tmM = 0; // Таймерные переменные пройденного времени
uint16_t tmD = 0;                  // Таймерная переменная дней
float speedRpm = MIN_SPEED;        // Текущая/начальная скорость (об/мин)
uint32_t stepDelayUs = 0;          // Задержка между шагами в микросекундах
volatile uint32_t accum_1S = 0;    // Используется для накопления прерываний
bool updDisplayFlag = false;       // Флаг обновления дисплея

EncButton eb(ENCA_PIN, ENCB_PIN, MENU_ENC_PIN);
Button sb(START_BTN_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4);

void Handle_Eb();
void Handle_Sb();
void updateDisplayHandle();
void UpdateLcdDisplay();
void CheckInputLimits(float inpSpeed);
void CalculateIsrDelay();
void initISR();
void MakeStep();
void MakeISR();
void RiseTimer_1s();
void SetMotorFreeze(bool freeze);
void DisplaySerialDebugData();
void FraseInMiddlePos(uint8_t posY, String frase);
void SaveEEPROM();
void LoadEEPROM();
void DisplayIntro();
void RandomBuildString(String st, uint8_t posX, uint8_t posY, uint16_t delayTime);

void setup()
{
    Serial.begin(115200);
    Serial.println();

    EEPROM.begin();
    delay(5);

    LoadEEPROM();

    while (isnan(speedRpm)) // проверяем наличие данных в EEPROM (первый запуск)
    {
        speedRpm = MIN_SPEED;

        SaveEEPROM();
    }

    lcd.init();
    lcd.backlight();

    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_STEPPER_PIN, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    randomSeed(analogRead(A0));

    SetMotorFreeze(MOTOR_FREEZE_ON_STOP); // переводим мотор в выбраный режим во режим остановки

    digitalWrite(DIR_PIN, MOTOR_DIRECTION); // Направление вращения мотора

    DisplayIntro();

    CalculateIsrDelay(); // Калькуляция задержки шага и установка прерывания
    initISR();           // Инициализация прерывания

    Serial.println();
    Serial.println("Welcome");

    DisplaySerialDebugData();

    updDisplayFlag = true;
}

void loop()
{
    updateDisplayHandle();

    eb.tick();
    sb.tick();

    Handle_Eb();
    Handle_Sb();
}

void Handle_Eb()
{
    if (eb.turn())
    {
        if (displayMode != set_mode) // игнорируем если не в режиме настройки
            return;

        if (eb.dir() > 0)
            speedRpm += 0.1;
        else if (eb.dir() < 0)
            speedRpm -= 0.1;

        CheckInputLimits(speedRpm);
    }

    if (eb.click())
    {
        if (displayMode == work_mode || displayMode == pause_mode) // игнорируем в режиме работы и паузы
            return;

        if (displayMode == set_mode)
        {
            displayMode = loaded_mode;

            Serial.println("Enter loaded mode");
        }
        else if (displayMode == loaded_mode)
        {
            displayMode = set_mode;

            Serial.println("Enter setup mode");
        }

        lcd.clear();

        updDisplayFlag = true;
    }
}

void Handle_Sb()
{
    if (displayMode == set_mode) // игнорируем в режиме настройки
        return;

    if (sb.click())
    {
        if (displayMode == pause_mode) // игнорируем в режиме паузы
            return;

        if (displayMode == loaded_mode)
        {
            SaveEEPROM();

            SetMotorFreeze(true); // включаем мотор для работы

            Timer1.restart();

            displayMode = work_mode;

            Serial.println("Enter work mode");
        }
        else if (displayMode == work_mode)
        {
            Timer1.stop();

            SetMotorFreeze(MOTOR_FREEZE_ON_STOP); // переводим мотор в выбраный режим во режим остановки

            displayMode = loaded_mode;

            Serial.println("Enter loaded mode");
        }

        tmS = 0;
        tmH = 0;
        tmM = 0;
        tmD = 0;

        lcd.clear();

        updDisplayFlag = true;
    }
    else if (sb.hold())
    {
        if (displayMode == set_mode || displayMode == loaded_mode) // игнорируем в режиме настройки и загрузки
            return;

        if (displayMode == work_mode)
        {
            Timer1.stop();

            SetMotorFreeze(MOTOR_FREEZE_ON_PAUSE); // переводим мотор в выбраный режим во время паузы

            displayMode = pause_mode;

            Serial.println("Enter pause mode");
        }
        else if (displayMode == pause_mode)
        {
            SetMotorFreeze(true); // включаем мотор для продолжения работы

            Timer1.resume();

            displayMode = work_mode;

            Serial.println("Enter work mode");
        }

        lcd.clear();

        updDisplayFlag = true;
    }
}

void CheckInputLimits(float inpSpeed)
{

    if (inpSpeed < MIN_SPEED)
    {
        Serial.println("Corrected to min: out of range from 0.1 to 6.0 rpm");

        inpSpeed = MIN_SPEED;
    }
    else if (inpSpeed > MAX_SPEED)
    {
        Serial.println("Corrected to max: out of range from 0.1 to 6.0 rpm");

        inpSpeed = MAX_SPEED;
    }

    speedRpm = inpSpeed;

    CalculateIsrDelay();
    DisplaySerialDebugData();

    updDisplayFlag = true;
}

void CalculateIsrDelay()
{
    stepDelayUs = (uint32_t)(60000000.0 / (speedRpm * STEPS_PER_REV));

    Timer1.setPeriod(stepDelayUs); // Инициализация таймера в микросекундах
}

void initISR()
{
    Timer1.initialize(stepDelayUs);   // Инициализация таймера в микросекундах
    Timer1.attachInterrupt(MakeStep); // Привязываем функцию к таймеру
    Timer1.stop();
}

void MakeStep()
{
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(1);
    digitalWrite(STEP_PIN, LOW);

    MakeISR();
}

void MakeISR()
{
    accum_1S += stepDelayUs;

    if (accum_1S >= 1000000)
    {
        RiseTimer_1s();

        accum_1S -= 1000000;
    }
}

void RiseTimer_1s()
{
    tmS++;
    updDisplayFlag = true;
}

void SetMotorFreeze(bool freeze)
{
    if (freeze)
        digitalWrite(EN_STEPPER_PIN, LOW);
    else
        digitalWrite(EN_STEPPER_PIN, HIGH);
}

void DisplaySerialDebugData()
{
    Serial.println("Debug data:");
    Serial.print("Step delay: ");
    Serial.print(stepDelayUs);
    Serial.println(" us");
    Serial.print("Speed set to: ");
    Serial.print(speedRpm);
    Serial.println(" rpm");
}

void updateDisplayHandle()
{
    if (updDisplayFlag)
    {
        updDisplayFlag = false;

        UpdateLcdDisplay();
    }
}

void UpdateLcdDisplay()
{
    switch (displayMode)
    {
    case set_mode:
        FraseInMiddlePos(0, "SETUP RPM:");
        FraseInMiddlePos(1, "New RPM:>" + (String)speedRpm + "<");
        FraseInMiddlePos(2, "Rotate ENC to ADJUST");
        FraseInMiddlePos(3, "Press ENC to LOAD");

        break;
    case loaded_mode:
        FraseInMiddlePos(0, "LOADED RPM: " + (String)speedRpm);
        FraseInMiddlePos(1, "Ready to START");
        FraseInMiddlePos(2, "Press ENC to SETUP");
        FraseInMiddlePos(3, "Press BTN to START");

        break;
    case work_mode:
        if (tmS >= 60) // переводим во время
        {
            tmS = 0;
            tmM++;

            if (tmM >= 60)
            {
                tmM = 0;
                tmH++;

                if (tmH >= 24)
                {
                    tmH = 0;
                    tmD++;

                    if (tmD > MAX_TIME_H)
                        tmD = 0;
                }
            }

            lcd.clear();
        }

        FraseInMiddlePos(0, "WORK: " + (String)tmD + "d " + (String)tmH + ":" + (String)tmM + ":" + (String)tmS);
        FraseInMiddlePos(1, "RPM = " + (String)speedRpm);
        FraseInMiddlePos(2, "Hold BTN to PAUSE");
        FraseInMiddlePos(3, "Press BTN to STOP");

        break;
    case pause_mode:
        FraseInMiddlePos(0, "PAUSE");
        FraseInMiddlePos(1, "RPM = " + (String)speedRpm);

        FraseInMiddlePos(2, "Time: " + (String)tmD + "d " + (String)tmH + ":" + (String)tmM + ":" + (String)tmS);
        FraseInMiddlePos(3, "Hold BTN to RESUME");

        break;
    }
}

void FraseInMiddlePos(uint8_t posY, String frase)
{
    uint8_t middlePosMess = ((19 - frase.length()) / 2) + 1;

    lcd.setCursor(middlePosMess, posY);
    lcd.print(frase);
}

void SaveEEPROM()
{
    EEPROM.put(0, speedRpm);

    delay(10);

    Serial.println("Save EEPROM data: " + (String)speedRpm);
}

void LoadEEPROM()
{
    EEPROM.get(0, speedRpm);

    delay(10);

    Serial.println("Load EEPROM data: " + (String)speedRpm);
}

void DisplayIntro()
{
    lcd.clear();
    FraseInMiddlePos(0, "WELCOME");
    delay(1000);
    RandomBuildString("STEPPER CONTROLER", 1, 2, 150);
    RandomBuildString("v1.0", 8, 3, 150);
    delay(1000);

    lcd.clear();

    FraseInMiddlePos(1, "mail:");
    FraseInMiddlePos(2, "planer.reg@gmail.com");

    delay(2000);

    lcd.clear();
}

void RandomBuildString(String st, uint8_t posX, uint8_t posY, uint16_t delayTime)
{
    int len = st.length();
    int indices[len]; // Массив хранения индексов

    for (int i = 0; i < len; i++)
    {
        indices[i] = i;
    }

    for (int i = len - 1; i > 0; i--) // Перемешиваем массив индексов
    {
        int j = random(i + 1); // Случайный индекс от 0 до i

        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }

    for (int i = 0; i < len; i++) // Выводим символы в случайном порядке
    {
        int pos = indices[i];
        lcd.setCursor(pos + posX, posY);
        lcd.print(st[pos]);

        delay(delayTime);
    }
}