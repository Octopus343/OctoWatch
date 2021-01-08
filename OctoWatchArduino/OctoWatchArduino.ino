int commonPinType = 0;      // Определяет тип сегментов - общий анод (1) или катод (0)

#define BRIGHT_SENSOR A0
#define BUTTON_TIMER A1
#define BUTTON_CLOCK A2
#define BLINK_LED 10
#define BLUE_FLASK A3

/* 
        A
       --- 
    |       |
  F |       | B
    |       |
       -G-
    |       |
  E |       | C
    |       |
       ---
        D
*/

// Подключение индикторов
//                    A, B, C, D,  E,  F,  G
int segmentPins[7] = {2, 4, 7, 8, 11, 12, 13}; // У всех четырех эти контакты соединены в одну шину

// Контакты заземления у каждого свои, номера слева на право в обратном порядке с циферблатом
//                M  M  H  H
int gndPins[4] = {3, 5, 6, 9};

//____________________________________________________________________________________
//____________________________________________________________________________________

// Массив цифр для индикторов (1 - сегмент горит, 0 - не горит)
// 10 цифр, 11й элемент - выключение всего
byte digits[11] = { B0111111,
                    B0000110,
                    B1011011,
                    B1001111,
                    B1100110,
                    B1101101,
                    B1111101,
                    B0000111,
                    B1111111,
                    B1101111, 
                    B0000000 };


// Инкапсулирует считывание и сглаживание значений с датчика
class Sensor
{
    public:
        // Коэффициент сглаживания
        double alpha;

        // Конструктор
        Sensor(double alpha, int pin)
        {
            this->alpha = alpha;
            this->pin = pin;
            prevValue = analogRead(pin);
        }

        // Обрабатывает и возвращает значение с датчика
        double getValue()
        {
            // Получаем значение с АЦП
            int value = analogRead(pin);

            // Экспоненциально сглаживаем
            double smoothed = prevValue + alpha * (value - prevValue);

            // Обновляем переменную предыдущего значения
            prevValue = smoothed;

            // Возвращаем обработанное значение
            return smoothed;
        }
        
    private:
        // Сохраненное предыдущее обработанное значение датчика
        double prevValue;
        
        // Пин датчика
        int pin;
};

// Заводим датчик освещенности 
Sensor brightSensor(0.1, BRIGHT_SENSOR);


int delayForCalibration = 3000; // Время в миллисекундах, необходимое для перехода в режим калибровки

byte maxBrightness = 255;       // Максимальная яркость индикаторов в диапазоне 0..255
byte minBrightness = 20;        // Минимальная яркость индикаторов в диапазоне 0..255
byte brightness = 120;          // Текущая яркость индикаторов в диапазоне 0..255
int updateTime = 2;             // Время обновления цифр в миллисекундах (не рекомендуется больше 5 - моргание становится заметным)
int ledBlinkInterval = 1000;    // Полупериод мигания разделительной точки в миллисекундах
int blinkClockInterval = 250;   // Полупериод мигания цифр при изменении времени

bool forced = false;
unsigned long pressingDuration = 1000;
unsigned long startedPressing = 1; // Сейчас используется как флаг для отключения дребезга кнопки при изменении времени.
                                   // В перспективе - с помощью нее учитывать продолжительность нажатия и 
                                   // автоматически увеличивать время с нарастающей скоростью.
                                   // Также она уже используется для определения продолжительности нажатия
                                   // при переходе в режим калибровки

bool setMinutesMode = false;    // Изменение минут
bool setHoursMode = false;      // Изменение часов
bool sleepMode = false;         // Спящий режим

byte hourBias = 0;      // Смещение количества часов относительно времени с момента включения
byte minuteBias = 0;    // Смещение минут относительно времени с момента включения

unsigned long startTime;    // Сюда сохраняется время millis() при старте таймера
bool isTimerSet = false;    // Идет ли в данный момент таймер
int timerValue = 60 * 5;    // Время отсчета таймера в секундах
int timerTimeout = 3;       // Время в СЕКУНДАХ, в течение которого будут мигать нули после истечения времени

 

// ___________________________________
// Функции


void display(byte);                 // Выводит с помощью пинов индикатора требуемую последовательность бит 
void displayNumber(int, bool=false);// Выводит на индикатор цифру
void setTimer(bool=true);           // Включает (true) или выключает (false) таймер

void Error();                   // Выводит на индикаторы " Err"
bool clockBtnPressed();         // Состояние кнопки часов
bool timerBtnPressed();         // Состояние кнопки таймера
void displayTime();             // Выводит текущее время

void waitForRelease(bool=true); // Ожидание отпускания всех кнопок (при этом показывается время)
                                // Опционален показ времени во время ожидания
                                    
void enterSleepMode();          // Вхождение в режим сна
void exitSleepMode();           // Пробуждение

void enterCalibration();        // Запуск калибровки

void setup()
{
    // Запускаем последовательный порт (для отладки)
    Serial.begin(9600);

    // Объявляем пины индикаторов как выходные
    for (int i = 0; i < sizeof(segmentPins)/sizeof(segmentPins[0]); i++)
    {
        pinMode(segmentPins[i], OUTPUT);
    }
    for (int i = 0; i < sizeof(gndPins) / sizeof(gndPins[0]); i++)
    {
        pinMode(gndPins[i], OUTPUT);
        
        // Устанавливаем разность потенциалов между катодом и анодом каждого индикатора по умолчанию
        //analogWrite(gndPins[i], 255 - brightness);
    }
    if (commonPinType == 1)
    {
        for (int i = 0; i < sizeof(digits) / sizeof(digits[0]); i++)
        {
            digits[i] = 127 - digits[i];
        }
    }

    // Объявляем остальные пины
    pinMode(BRIGHT_SENSOR, INPUT_PULLUP); // Датчик освещенности
    pinMode(BUTTON_TIMER, INPUT_PULLUP);  // Кнопка таймера
    pinMode(BUTTON_CLOCK, INPUT_PULLUP);  // Кнопка часов
    pinMode(BLINK_LED, OUTPUT);           // Мигающий светодиод
    pinMode(BLUE_FLASK, OUTPUT);          // Колба незаметности (светодиод)
}

void loop() 
{
    // Получаем значение с датчика освещенности и конвертируем в удобные нам единицы
    int rawBrightness = brightSensor.getValue();
    brightness = map(rawBrightness, 0, 1023, maxBrightness, minBrightness);
    if(1 == commonPinType) // Конвертируем значение, если индикаторы с общим анодом
    {
        brightness = 255 - brightness;
    }
    
    // В зависимости от освещенности включаем или выключаем индикатор заметности
    if (rawBrightness < 500 && !sleepMode)
    {
        digitalWrite(BLUE_FLASK, HIGH);
    }
    else
    {
        digitalWrite(BLUE_FLASK, LOW);
    }

    if (sleepMode)  // Спящий режим 
    {
        // Выводим время при нажатии на кнопку
        if (clockBtnPressed())
        {
            // В зависимости от состояния таймера показываем время
            if (isTimerSet)
            {
                displayRemainingTime();
            }
            else
            {
                displayTime();
            }

            // А если нажата и вторая кнопка - выключаем спящий режим
            if (timerBtnPressed())
            {
                exitSleepMode();
            }
        }
        // Штатно запускаем таймер по кнопке
        else if(timerBtnPressed())
        {
            // Ждем отпуска кнопки
            while(timerBtnPressed())
            {
                // Выключение спящего режима
                if (clockBtnPressed())
                {
                    exitSleepMode();
                    return;
                }
                delay(1);
            }
    
            // Запускайте бычка!
            setTimer(!isTimerSet);
        }
    }
    else if (setHoursMode || setMinutesMode)    // Проверяем, включен ли режим установки времени
    {
        // Обработка нажатия кнопки изменения времени
        if (clockBtnPressed())
        {
            // Переключаем изменяемую пару
            setHoursMode = !setHoursMode;
            setMinutesMode = !setMinutesMode;

            // Ждем отпускания кнопок
            while(clockBtnPressed() || timerBtnPressed())
            {
                displayTime();

                // Если обе кнопки нажались - выход из режима
                if (timerBtnPressed())
                {
                    setMinutesMode = false;
                    setHoursMode = false;
                }
            }
        }

        
        // Обработка нажатия кнопки таймера - непосредственно изменение времени
        if(timerBtnPressed())
        {
            if (!forced)
            {
                // Если кнопка уже была нажата, то показываем время и ждем
                if (startedPressing != 0)
                {
                    displayTime();
                    if (millis() - startedPressing > pressingDuration)
                    {
                        forced = true;
                    }
                    return;
                }
            }
            else if (millis() - startedPressing < 70)
            {
                displayTime();
                return;
            }
            
            startedPressing = millis();
            

            //int delta = (unsigned long) millis() - (unsigned long) startedPressing;

            // В зависимости от режима меняем нужные смещения
            if (setHoursMode)
            {
                hourBias = (hourBias + 1) % 24;
            }
            else if (setMinutesMode)
            {
                // Смещение минут нельзя просто так взять по модулю - при переполнении нужно увеличить смещение часов на 1
                minuteBias += 1;
                if (minuteBias >= 60)
                {
                    minuteBias %= 60;
                    hourBias = (hourBias + 1) % 24;
                }
            }
        }
        // Отпустили кнопку
        else
        {
            startedPressing = 0;
            forced = false;
        }

        // Вывод времени
        displayTime();
        return;
    }
    // Включение режима изменения времени
    else if (clockBtnPressed())
    {
        if (timerBtnPressed())
        {
            enterSleepMode();
        }
        else
        {
            while (clockBtnPressed())
            {
                displayTime();
                if(timerBtnPressed())
                {
                    enterSleepMode();
                    return;
                }
            }
            setHoursMode = true;
        }
        
        return;
    }
    else if (timerBtnPressed()) // Включение таймера
    {
        // Ждем отпуска кнопки
        while(timerBtnPressed())
        {
            displayTime();
            if (clockBtnPressed())
            {
                enterSleepMode();
                return;
            }
        }

        // Запускайте бычка!
        setTimer(!isTimerSet);
    }
    else if (isTimerSet)    // Если сейчас идет таймер, то показываем его оставшееся время
    {
        displayRemainingTime();
        return;
    }

    // Выводим время
    if(!sleepMode)
    {
        displayTime();
    }
}

void display(byte digit)
{
    // Число, с помощью которого будем доставать значение нужного бита
    byte pointer = 1;
    
    for (int i = 0; i < sizeof(segmentPins) / sizeof(segmentPins[0]); i++)
    {
        // Получаем бит для каждого индикатора и инвертируем его, если индикаторы с общим анодом
        byte value = digit & pointer;
        
        // Подаем на каждую ногу соответствующее значение
        digitalWrite(segmentPins[i], value);
        
        // Смещаем наш операнд
        pointer *= 2;
    }
}

void displayNumber(int number, bool showLeadZeros=false)
{
    // Проверка корректности числа
    if (number < 0 || number > 9999)
    {
        Error();
        return;
    }

    // Конвертируем яркость в значение ШИМ
    byte outValue = 255 - constrain(brightness, 0, 255);

    // Выключаем все индикаторы
    for (int i = 0; i < sizeof(gndPins) / sizeof(gndPins[0]); i++)
    {
        analogWrite(gndPins[i], 255 * (1 - commonPinType));
    }

    // Здесь будем хранить информацию о том, является ли нуль лидирущим
    bool lead = !showLeadZeros;


    for (int i = sizeof(gndPins) / sizeof(gndPins[0]) - 1; i >= 0 ; i--)
    {
        // Вытаскиваем цифру из числа
        byte digit = (int)(number / pow(10, i)) % 10;

        // Отдельно проверяем нули
        if (digit == 0 && lead)
        {
            // Нуль лидирующий, не показываем его
            delay(updateTime);
            continue;
        }
        else
        {
            // В этом случае уже есть старшая цифра, отличная от нуля - все остальные нули нужно показать
            lead = false;
        }

        // Если включен режим изменения времени, моргаем индикаторами
        if ( (setHoursMode && (i >= 2)) ||      // i=2, i=3 - индикаторы часов
             (setMinutesMode && (i <= 1)) )     // i=0. i=1 - индикаторы минут
        {
            if (millis() % (blinkClockInterval * 2) > blinkClockInterval)
            {
                delay(updateTime);
                continue;
            }
        }

        analogWrite(gndPins[i], outValue);// . Создаем необходимую разность потенциалов на одном из индикаторов.
        display(digits[digit]); // ........... Выводим цифру на индикаторе.
        delay(updateTime); // ................ Ждем зажигания.
        analogWrite(gndPins[i], 255 * (1 - commonPinType)); // ..... Выключаем индикатор, цепляя контакт заземления на +.
        
        // Выводим пустоту, чтобы убрать шум от этой цифры на других индикаторах
        display(digits[10]);
    }
    // Выжидаем еще один интервал
    delay(updateTime);
}

void setTimer(bool on = true)
{
    // Выключаем моргающий светодиод
    digitalWrite(BLINK_LED, LOW);
    
    // Ставим флаг в зависимости от параметра включения
    isTimerSet = on;
    if(on)
    {
        // Устанавливаем стартовое время, если мы решили включить таймер
        startTime = millis();
    }
}

void Error()
{
    // Получаем значение для ШИМ
    byte outValue = 255 - constrain(brightness, 0, 255);

    // Строим " Err" для индикаторов
    byte Err[4] = { B1010000,
                    B1010000,
                    B1111001,
                    B0000000 };

    // Выводим Err по той же технологии, что и вывод обычного числа [см. displayNumber()]
    for (int i = sizeof(gndPins) / sizeof(gndPins[0]) - 1; i >= 0 ; i--)
    {
        analogWrite(gndPins[i], outValue);
        display(Err[i]);
        delay(updateTime);
        analogWrite(gndPins[i], 255 * (1 - commonPinType));
        display(digits[10]);
    }
    delay(updateTime);
}

bool clockBtnPressed()
{
    return digitalRead(BUTTON_CLOCK) == 0;
}

bool timerBtnPressed()
{    
    return digitalRead(BUTTON_TIMER) == 0;
}

void displayTime()
{
    // Вычисляем настоящее время на основе прошедшего времени с начала работы и всех смещений
    unsigned long secs = millis() / 1000 + (long)hourBias * 3600 + (long)minuteBias * 60;
    
    // Вытаскиваем минуты и часы
    int mins = (secs / 60) % 60;
    int hours = (secs / 3600) % 24;
    //Serial.println(String(hours)+':'+String(mins));
    // Смотрим, пришло ли время моргать точкой
    bool value = millis() % (ledBlinkInterval * 2) > ledBlinkInterval;
    if(value || sleepMode) // Если включен спящий режим, точкой не мигаем
    {
        // Моргаем
        analogWrite(BLINK_LED, 0);
    }
    else
    {
        // Не моргаем
        analogWrite(BLINK_LED, 1);
    }

    // Выводим на индикаторы число - часы и минуты
    displayNumber(hours * 100 + mins, true);
    //   H.H.0.0
    // +     M.M
    // = H.H.M.M
}

void waitForRelease(bool showTime = true)
{
    // Запоминаем время
    startedPressing = millis();
    
    // Пока хотя бы одна кнопка нажата - показываем время и не рыпаемся
    while(clockBtnPressed() || timerBtnPressed())
    {
        unsigned long delta = millis() - startedPressing;
        if (delta > delayForCalibration)
        {
            enterCalibration();
            return;
        }
        else if (showTime)
        {
            displayTime();
        }
        else
        {
            delay(5);
        }
    }
}

void enterSleepMode()
{
    // Ждем отпускания кнопок
    waitForRelease(false);
    
    // Выключаем точку
    analogWrite(BLINK_LED, 0);

    // Засыпаем
    sleepMode = true;
}

void exitSleepMode()
{
    // Ждем отпускания кнопок
    waitForRelease();
    
    // Подъем!
    sleepMode = false;
}

void displayRemainingTime()
{
    // Считаем, сколько прошло миллисекунд с момента запуска
    unsigned long elapsedTime = millis() - startTime;
    
    // Вычисляем оставшееся время в секундах
    int timeLeft = timerValue - (elapsedTime / 1000) % 10000;

    // Время закончилось
    if (timeLeft < -timerTimeout)
    {
        isTimerSet = false;
        return;
    }

    // Форматируем время для вывода на индикаторы
    int formatedTime = timeLeft / 60 * 100 + timeLeft % 60;

    // Когда время отрицательное, таймер закончился. Значит, нужно вывести нули и помигать ими
    if (formatedTime < 0)
    {
        formatedTime = 0;
    }

    // Мигание цифр, когда осталось меньше минуты
    if (timeLeft > 60 || (elapsedTime + 750) % 1000 > 500)
    {
        displayNumber(formatedTime, true);    
    }
    else
    {
        displayNumber(0);
    }
}

void enterCalibration()
{
    int pwmValue;
    int value;
    waitForRelease();
    
    while(true)
    {
        
        if ((millis() / 10) % 10 == 0)
        {
            // Обновляем значение с датчика
            value = brightSensor.getValue();
    
            // Переводим его в значение для ШИМ
            pwmValue = map(value, 0, 1023, 0, 255);
        }
        else
        {
            // Вызываем функцию для активной работы экспоненциального сглаживания
            brightSensor.getValue();
        }
        // Выводим значение на индикаторы
        displayNumber(value);
        
        if (clockBtnPressed())
        {
            while(clockBtnPressed())
            {
                if (timerBtnPressed())
                {
                    return;
                }
                displayNumber(value);
            }
            minBrightness = pwmValue;
        }
        else if (timerBtnPressed())
        {
            while(timerBtnPressed())
            {
                if (timerBtnPressed())
                {
                    return;
                }
                displayNumber(value);
            }
            maxBrightness = pwmValue;
        }
    }
}
