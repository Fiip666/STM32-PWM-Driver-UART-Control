/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------- Константы и параметры ------------------------------ */
#define UART_BUF_SIZE     64      // размер буфера для приёма UART
#define PWM_DEFAULT_FREQ  20000   // частота ШИМ по умолчанию 20 кГц
#define MAX_DUTY          100     // максимальная скважность в %
#define DUTY_STEP_DELAY   20      // задержка в мс между шагами плавного изменения

/* ------------------- Глобальные переменные ------------------------------ */
TIM_HandleTypeDef htim1;          // дескриптор таймера TIM1
UART_HandleTypeDef huart5;        // дескриптор UART5

volatile uint8_t duty_now = 50;          // текущая скважность
volatile uint8_t duty_target = 50;       // целевая скважность
volatile uint8_t pwm_enabled = 0;        // флаг состояния PWM (включён/выключен)
volatile uint32_t pwm_freq = PWM_DEFAULT_FREQ; // текущая частота ШИМ

char uart_buf[UART_BUF_SIZE];    // буфер для приёма команды по UART
volatile uint8_t uart_idx = 0;   // индекс в буфере
volatile uint8_t uart_ready = 0; // флаг, что строка полностью получена

/* ------------------- Прототипы функций ------------------------------ */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_UART5_Init(void);

void PWM_Start(void);
void PWM_Stop(void);
void PWM_SetDuty(uint8_t duty);
void PWM_SetFreq(uint32_t freq);
void UART_Process(char *cmd);

/* ------------------- Переопределение printf для UART ------------------ */
int __io_putchar(int ch)
{
    // перенаправляем printf на UART5
    HAL_UART_Transmit(&huart5, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/* ------------------- Обработчик прерывания UART ----------------------- */
void UART5_IRQHandler(void)
{
    // Передаём обработку HAL‑библиотеке
    HAL_UART_IRQHandler(&huart5);
}

/* ------------------- Callback приёма одного байта UART ----------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == UART5)
    {
        char c = uart_buf[uart_idx]; // текущий принятый символ

        if(c == '\r' || c == '\n')
        {
            // если получили Enter/новую строку — завершаем строку
            uart_buf[uart_idx] = 0; // добавляем терминатор строки
            uart_ready = 1;         // помечаем, что команда готова
            uart_idx = 0;           // сбрасываем индекс для следующей команды
        }
        else
        {
            // иначе добавляем символ в буфер
            uart_idx++;
            if(uart_idx >= UART_BUF_SIZE) uart_idx = 0; // защита от переполнения
        }

        // снова запускаем прерывание на приём следующего байта
        HAL_UART_Receive_IT(&huart5, (uint8_t*)&uart_buf[uart_idx], 1);
    }
}

/* =================== Главная функция =================== */
int main(void)
{
    HAL_Init();               // инициализация HAL
    SystemClock_Config();     // конфигурация тактирования

    MX_GPIO_Init();           // инициализация GPIO
    MX_TIM1_Init();           // инициализация TIM1 для ШИМ
    MX_UART5_Init();          // инициализация UART5

    /* Запуск прерывания на приём UART */
    HAL_UART_Receive_IT(&huart5, (uint8_t*)&uart_buf[uart_idx], 1);

    // задаём начальную скважность и запускаем PWM
    PWM_SetDuty(duty_now);
    PWM_Start();

    printf("\r\n=== PWM CONTROLLER READY ===\r\n");

    uint32_t last = 0;

    while (1)
    {
        // когда строка получена — обрабатываем команду
        if (uart_ready)
        {
            uart_ready = 0;
            UART_Process(uart_buf);
        }

        // плавное изменение скважности к целевой
        if (HAL_GetTick() - last > DUTY_STEP_DELAY)
        {
            last = HAL_GetTick();
            if(duty_now < duty_target) PWM_SetDuty(duty_now + 1);
            else if(duty_now > duty_target) PWM_SetDuty(duty_now - 1);
        }
    }
}

/* =================== PWM функции =================== */

// включение PWM (TIM1 + комплиментарные выходы)
void PWM_Start(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

    __HAL_TIM_MOE_ENABLE(&htim1); // включаем главный выход (MOE)

    pwm_enabled = 1;
    printf("PWM START\r\n");
}

// отключение PWM
void PWM_Stop(void)
{
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_3);

    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);

    pwm_enabled = 0;
    printf("PWM STOP\r\n");
}

// установка скважности (0–100%)
void PWM_SetDuty(uint8_t duty)
{
    if (duty > MAX_DUTY) duty = MAX_DUTY; // ограничение

    uint32_t pulse = (htim1.Init.Period * duty) / 100; // считаем значение CCR

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, pulse);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, pulse);

    duty_now = duty; // обновляем текущую скважность
}

// установка частоты PWM
void PWM_SetFreq(uint32_t freq)
{
    // проверка диапазона частоты
    if (freq < 1000 || freq > 50000)
    {
        printf("FREQ ERR\r\n");
        return;
    }

    uint32_t clk = HAL_RCC_GetPCLK2Freq(); // частота шины
    uint32_t arr = (clk / freq) - 1;        // новый период

    PWM_Stop();            // отключаем PWM перед изменением
    __HAL_TIM_SET_AUTORELOAD(&htim1, arr);  // меняем регистр ARR
    PWM_SetDuty(duty_now); // обновляем скважность под новый ARR
    PWM_Start();           // снова запускаем PWM

    pwm_freq = freq;
    printf("FREQ %lu Hz\r\n", freq);
}

/* =================== Обработка UART команд =================== */
void UART_Process(char *cmd)
{
    // команда "set X" — установить целевую скважность
    if (!strncmp(cmd, "set ", 4))
    {
        duty_target = atoi(cmd + 4);  // получаем число после "set "
        if(duty_target > 100) duty_target = 100; // ограничение
        printf("TARGET %d%%\r\n", duty_target);
    }
    // команда "freq X" — установить частоту
    else if (!strncmp(cmd, "freq ", 5))
    {
        PWM_SetFreq(atoi(cmd + 5));
    }
    // команда "start" — включить PWM
    else if (!strcmp(cmd, "start"))
    {
        PWM_Start();
    }
    // команда "stop" — выключить PWM
    else if (!strcmp(cmd, "stop"))
    {
        PWM_Stop();
    }
    // команда "status" — вывести текущий статус
    else if (!strcmp(cmd, "status"))
    {
        printf("Duty: %d%% -> %d%%\r\n", duty_now, duty_target);
        printf("Freq: %lu Hz\r\n", pwm_freq);
        printf("State: %s\r\n", pwm_enabled ? "ON" : "OFF");
    }
    // если команда неизвестна
    else
    {
        printf("CMD ERR\r\n");
    }
}

/* =================== TIM1 инициализация (PWM) =================== */
static void MX_TIM1_Init(void)
{
    uint32_t clk = HAL_RCC_GetPCLK2Freq();       // частота шины APB2
    uint32_t arr = (clk / PWM_DEFAULT_FREQ) - 1; // период для заданной частоты

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = arr;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim1);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1;    // режим PWM1
    oc.Pulse = arr / 2;             // стартовое значение (50%)
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;

    // настраиваем 3 канала
    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_3);

    // dead‑time и выходы комплиментарные
    TIM_BreakDeadTimeConfigTypeDef bd = {0};
    bd.DeadTime = 40;                 // dead‑time ≈ 500 нс
    bd.BreakState = TIM_BREAK_DISABLE;
    bd.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE;
    HAL_TIMEx_ConfigBreakDeadTime(&htim1, &bd);

    HAL_TIM_MspPostInit(&htim1);      // вызывается после базовой инициализации
}

/* =================== GPIO инициализация =================== */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();     // включаем тактирование портов
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_AF_PP;         // альтернативная функция, push‑pull
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    // выводы TIM1 CH1–CH3 (PWM основные)
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOA, &g);

    // выводы TIM1 CH1N–CH3N (комплиментарные)
    g.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &g);
}

/* =================== UART5 инициализация =================== */
static void MX_UART5_Init(void)
{
    huart5.Instance = UART5;
    huart5.Init.BaudRate = 115200;                    // скорость UART
    huart5.Init.WordLength = UART_WORDLENGTH_8B;
    huart5.Init.StopBits = UART_STOPBITS_1;
    huart5.Init.Parity = UART_PARITY_NONE;
    huart5.Init.Mode = UART_MODE_TX_RX;               // приём и передача
    huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart5.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart5);

    /* Настраиваем NVIC для прерываний UART5 */
    HAL_NVIC_SetPriority(UART5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(UART5_IRQn);
}

/* =================== Конфигурация тактирования =================== */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}
