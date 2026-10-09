#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "oled.h"
// #include "bmp.h"
#include "led.h"
#include "delay.h"
#include "uart.h"
#include "semphr.h"
#include "dma.h"
#include "rtc.h" 
#include "menu.h"
#include "key.h"

// LED任务句柄
TaskHandle_t LEDTask1_Handler;
TaskHandle_t LEDTask2_Handler;
TaskHandle_t OLEDTask_Handler;
extern TaskHandle_t xPrintTaskHandle;

void oled_task(void *pvParameters);
void led_task(void *pvParameters);



typedef struct
{
    GPIO_TypeDef* GPIOx;      // GPIO端口
    uint16_t GPIO_Pin;        // GPIO引脚
    uint32_t delay_on;       // 延时时间
    uint32_t delay_off;       // 延时时间
} LED_Config;

LED_Config led1_config = {GPIOC, GPIO_Pin_13, 500, 500};
LED_Config led2_config = {GPIOA, GPIO_Pin_1, 500, 500};

// void config_task(void *pvParameters)
// {


//     vTaskDelete(NULL); // 删除配置任务
// }

int main(void)
{
    SystemInit();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init();          // 初始化延时
    OLED_Init();
    OLED_ColorTurn(0);//0正常显示，1 反色显示
    OLED_DisplayTurn(0);//0正常显示 1 屏幕翻转显示
    LED_GPIO_Config();  //led、蜂鸣器初始化
    KEY_Init();

    // ★ 初始化串口（内部会开中断）
    uart_init(115200);
    // ★ 配置DMA（内部会开中断）
    DMA_Config(DMA1_Channel5,(u32)&USART1->DR,(u32)CercleBuffer, CercleBufferSize);//DMA1通道5,外设为串口1,存储器为SendBuff,长度SEND_BUF_SIZE.
    //使用流缓冲区
    StreamBuffer_Init(); // 初始化 StreamBuffer
    
    RTC_Init();
    // 创建定时器
    timer_init();

    xQueueKey = xQueueCreate(10, sizeof(key_event_t));

    // //创建初始化任务
    // xTaskCreate(config_task, "config_task", 128, NULL, 1, NULL);
    // // 创建OLED显示任务
    xTaskCreate(oledmenu_task, "oledmenu_task", 256, NULL, 3, &OLEDTask_Handler);
    xTaskCreate(key_task, "key_task", 128, NULL, 4, NULL);
    // 创建LED任务
    xTaskCreate((TaskFunction_t )led_task,
                (const char*    )"led_task1",
                (uint16_t       )128,
                (void*          )&led1_config,
                (UBaseType_t    )3,
                (TaskHandle_t*  )&LEDTask1_Handler);
    
    xTaskCreate(wifi_get_task,"wifi_get_task",512,NULL,3,NULL);

    xTaskCreate(data_parse_task,"data_parse",256, NULL, 3, NULL);

    xTaskCreate(wifi_send_task,"wifi_send_task",196,NULL,1,&xWifisendTaskHandle);
    
    xTaskCreate(RTC_task,"RTC_task",256,NULL,5,NULL); 
    
    // 启动调度器
    vTaskStartScheduler();
    
    // 正常情况下不会运行到这里
    while(1);

    
}

void led_task(void *pvParameters)
{
    LED_Config* config = (LED_Config*)pvParameters;
    
    while(1)
    {
        if(config->GPIO_Pin==GPIO_Pin_13)
        {
            GPIO_ResetBits(config->GPIOx, config->GPIO_Pin);  // LED灭
            vTaskDelay(pdMS_TO_TICKS(config->delay_off));     // 延时
            GPIO_SetBits(config->GPIOx, config->GPIO_Pin);    // LED亮
            vTaskDelay(pdMS_TO_TICKS(config->delay_on));      // 延时
        }else if(config->GPIO_Pin==GPIO_Pin_1){
            GPIO_SetBits(config->GPIOx, config->GPIO_Pin);    // LED亮
            vTaskDelay(pdMS_TO_TICKS(config->delay_on));      // 延时
            GPIO_ResetBits(config->GPIOx, config->GPIO_Pin);  // LED灭
            vTaskDelay(pdMS_TO_TICKS(config->delay_off));     // 延时
        }
    }
}



