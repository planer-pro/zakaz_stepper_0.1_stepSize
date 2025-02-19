# Описание Проекта / Project Description

## На русском
Данный код реализует систему управления шаговым двигателем на базе Arduino. Основные возможности:
- **Регулировка скорости**: Изменение скорости вращения через энкодер с сохранением настроек в EEPROM.
- **Режимы работы**: Поддержка четырёх режимов — настройка, готовность, работа, пауза.
- **Визуализация**: Вывод данных на LCD-дисплей (скорость, время работы, статус).
- **Управление**: Запуск, остановка и пауза через кнопки; удержание мотора в заданном состоянии.
- **Точность**: Использование микрошагов для плавного вращения (1/16 шага).
- **Анимации**: Эффекты отображения текста при старте системы.
- **Отладка**: Логирование данных через последовательный порт.

Проект написан в Platformio на Visual Studio Code и протестирован на Arduino nano ATmega328.
Для того, чтобы его отклыть в Arduino IDE, нужно у файла main.cpp (ноходящего в папке src) поменять расширение на .ino. А уже потом его открывать в Arduino IDE. Также, в папке .pio\libdeps\ лежат все необходимые для кода библиотеки, которые нужно установить простым копированием в папку libraries в Arduino IDE.

## In English
This code controls a stepper motor using an Arduino. Key features:
- **Speed adjustment**: Encoder-based speed control with EEPROM storage.
- **Operational modes**: Four modes — setup, ready, work, pause.
- **Display**: LCD output for speed, elapsed time, and status.
- **Control**: Start, stop, and pause via buttons; motor hold states.
- **Precision**: Microstepping (1/16 step) for smooth rotation.
- **Animations**: Text effects during system startup.
- **Debugging**: Data logging via serial port.

The project is written in PlatformIO on Visual Studio Code and tested on an Arduino Nano ATmega328.
To open it in Arduino IDE, you need to change the extension of the main.cpp file (located in the src folder) to .ino, and then open it in Arduino IDE. Additionally, all required libraries for the code are located in the .pio\libdeps\ folder. These libraries must be installed by simply copying them into the libraries folder in Arduino IDE.
