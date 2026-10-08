#include "menu.h"
#include "FreeRTOS.h"
#include "rtc.h"
#include "oled.h"
#include "oledfont.h"  
#include "bmp.h"
#include "queue.h"
#include "task.h"
#include "key.h"
#include "led.h"
#include "uart.h"

// xQueueHandle menu_queue;
xQueueHandle xQueueKey;

menu_page_t menu_stack[MENU_STACK_MAX];//菜单栈
int menu_top = -1;   // 栈顶索引
send_data_t send_data = {0, 0}; // 新建发送数据结构体并初始化
area_weather_t area_weather_data = {0, 0, 0, 0, 0};// 新建地区天气数据结构体并初始化
TaskHandle_t xWifisendTaskHandle;   // wifi_send_task 的任务句柄

const unsigned char *texts[11][4] = {
    {Hzk4[0] , Hzk4[1] , Hzk4[2] , Hzk4[3] },//设置时间
    {Hzk4[0] , Hzk4[1] , Hzk4[4] , Hzk4[5] },//设置日期
    {Hzk4[6] , Hzk4[7] , Hzk4[4] , Hzk4[5] },//更新时间
    {Hzk4[10], Hzk4[11], Hzk4[8] , Hzk4[9] },//添加闹钟
    {Hzk4[12], Hzk4[13], Hzk4[8] , Hzk4[9] },//删除闹钟
    {Hzk4[14], Hzk4[15], Hzk4[8] , Hzk4[9] },//打开闹钟
    {Hzk4[16], Hzk4[17], Hzk4[8] , Hzk4[9] },//关闭闹钟
    {Hzk4[18], Hzk4[19], Hzk4[20], Hzk4[21]},//选择地区
    {Hzk4[22], Hzk4[23]                    },//杭州
    {Hzk4[24], Hzk4[25]                    },//上海
    {Hzk4[26], Hzk4[27],                   }//深圳
};

const menu_item_t alarm_menu[] = {
    {"Alarm Add", alarm_add_page, NULL, 0, texts[3]},
    {"Alarm Delete", alarm_delete_page, NULL, 0, texts[4]},
    {"Alarm Enable", alarm_enable_page, NULL, 0, texts[5]},
    {"Alarm Disable", alarm_disable_page, NULL, 0, texts[6]},
};

const menu_item_t area_select[] = {
    {"hangzhou", send1, NULL, 0, texts[8]},
    {"shanghai", send2, NULL, 0, texts[9]},
    {"shenzhen", send3, NULL, 0, texts[10]},
};

const menu_item_t time_menu[] = {
    {"Set Time", time_set_page, NULL, 0, texts[0]},
    {"Set Date", date_set_page, NULL, 0, texts[1]},
    {"Update Time", date_update_page, NULL, 0, texts[2]},
};

const menu_item_t area_menu[] = {
    {"Select Area", NULL, area_select, 3, texts[7]},
};

const menu_item_t main_menu[] = {
    {"Time", NULL, time_menu, 3},
    {"Alarm", NULL, alarm_menu, 4},
    {"Area", NULL, area_menu, 1},
};

const unsigned char *week_name[] = {
    Hzk3[1], Hzk3[2], Hzk3[3], Hzk3[4], Hzk3[5], Hzk3[6], Hzk3[7]
};

const unsigned char *weather_name[] = {
    BMP4, BMP4, BMP5, BMP6, BMP7, BMP8
};

void menu_push(const menu_item_t *items, uint8_t count)
{
    if (menu_top < MENU_STACK_MAX - 1) {
        menu_top++;
        menu_stack[menu_top].menu_items = items;
        menu_stack[menu_top].menu_index = 0;
        menu_stack[menu_top].menu_item_count = count;
    }
}

void menu_pop(void)
{
    if (menu_top >= 0) {
        menu_top--;
    }

}

menu_page_t *menu_current(void)//当前菜单
{
    if (menu_top >= 0) {
        return &menu_stack[menu_top];
    }
    return NULL;
}

void menu_render(void)//渲染选择菜单
{
    menu_page_t *p = menu_current();
    if (p == NULL) return; // 栈为空，直接返回
    OLED_Clear();
    if (p->menu_items[p->menu_index].next_items == time_menu) {
        // OLED_ShowString(98, 0, (u8*)"Set Time", 16);
        OLED_ShowPicture(32, 0, 96, 8, BMP2);
    }
    else if (p->menu_items[p->menu_index].next_items == alarm_menu) {
        // OLED_ShowString(98, 0, (u8*)"Set Alarm", 16);
        OLED_ShowPicture(32, 0, 96, 8, BMP3);
    }
    else if (p->menu_items[p->menu_index].next_items == area_menu) {
        // OLED_ShowString(98, 0, (u8*)"Select Area", 16);
        OLED_ShowPicture(32, 0, 96, 7, BMP1);
    }
    else {
        int total = p->menu_item_count;
        int idx   = p->menu_index;
        int WIN   = 3;

        int start = idx;
        if (start + WIN > total) start = total - WIN;
        if (start < 0) start = 0;

        for (int row = 0; row < WIN; row++) {
            int item = start + row;
            if (item < total) {
                char line[32];
                int i=0;
                while(i<4 && p->menu_items[item].text[i] != NULL) {
                    sprintf(line, "%c ",
                        (item == idx) ? '>' : ' ');
                    OLED_ShowString(0, row * 18, (u8*)line, 12);
                    OLED_ShowChinese(24+i * 15, row * 18, p->menu_items[item].text[i]);
                    i++;
                }
            }
        }
    }
    OLED_Refresh();
}

void menu_key_handler(key_event_t key)
{
    menu_page_t *p = menu_current();
    if (p == NULL) return; 
    switch (key) {
        case KEY_UP:   if (p->menu_index > 0) {p->menu_index--;}else {p->menu_index = p->menu_item_count - 1;} break;
        case KEY_DOWN: if (p->menu_index < p->menu_item_count - 1) {p->menu_index++;} else {p->menu_index = 0;} break;
        case KEY_OK:
            if (p->menu_items[p->menu_index].action)
                p->menu_items[p->menu_index].action();
            if (p->menu_items[p->menu_index].next_items)
                menu_push(p->menu_items[p->menu_index].next_items,
                          p->menu_items[p->menu_index].next_count);
            break;
        case KEY_BACK: menu_pop(); if (menu_top == -1) main_window_render(); break;
        case NO_PRESS: break;
    }
}

void key_task(void *pvParameters)
{
    key_event_t evt;

    while (1) {
        evt = KEY_Scan();
        if (evt != NO_PRESS) {
            xQueueSend(xQueueKey, &evt, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // 去抖
    }
}

void oledmenu_task(void *pvParameters)
{
    key_event_t evt;

    main_window_render();   // 开机先显示一次

    while (1) {
        if (xQueueReceive(xQueueKey, &evt, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if(menu_top == -1) {
                // 如果栈为空，按下任意键进入主菜单
                menu_push(main_menu, sizeof(main_menu)/sizeof(main_menu[0]));
                menu_render(); 
                continue;
            }
            menu_key_handler(evt);
            menu_render();
        } else{
            // 没有按键事件,栈为空，刷新主界面时间显示
            if (menu_top == -1) {
                main_window_render();
            }
        }

    }
}

void main_window_render(void)
{
    
    menu_top = -1;

    OLED_Clear();
    char line1[32], line2[32],line3[10], line4[16];
    sprintf(line1, "%02d-%02d-%02d", calendar.w_year % 100, calendar.w_month, calendar.w_date);
    sprintf(line2, "%02d:%02d:%02d", calendar.hour, calendar.min, calendar.sec);
    sprintf(line4, "%d,%d-%d", area_weather_data.current_temp,area_weather_data.area_temperature_high, area_weather_data.area_temperature_low);

    OLED_ShowString(0, 0 , (u8*)line1, 12);//日期
    OLED_ShowString(0, 17, (u8*)line2, 12);//时间

    OLED_ShowChinese(0, 33, Hzk3[0]);//星期
    OLED_ShowChinese(15, 33, week_name[calendar.week]);

    OLED_ShowChinese(0 ,48, Hzk1[0]);//当
    OLED_ShowChinese(15,48, Hzk1[1]);//前
    OLED_ShowChinese(30,48, Hzk1[2]);//气
    OLED_ShowChinese(45,48, Hzk1[3]);//温
    OLED_ShowString(60, 48, (u8*)line4, 12);

    OLED_ShowPicture(80, 0, 120, 5, weather_name[area_weather_data.area_weather]);


    OLED_Refresh();

}

void alarm_add_page(void)
{
    // Implementation for alarm add page
    main_window_render();  // For now, just render the main window
}

void alarm_delete_page(void)
{
    // Implementation for alarm delete page
    main_window_render();  // For now, just render the main window
}

void alarm_enable_page(void)
{
    // Implementation for alarm enable page
    main_window_render();  // For now, just render the main window
}

void alarm_disable_page(void)
{
    // Implementation for alarm disable page
    main_window_render();  // For now, just render the main window
}

void time_set_page(void)
{
    // Implementation for time set page
    main_window_render();  // For now, just render the main window
}

void date_set_page(void)
{
    // Implementation for date set page
    main_window_render();  // For now, just render the main window
}

void date_update_page(void)
{
    // Implementation for date update page
    send_data.get_time = 0x01; // Set the flag to get time
    xTaskNotify(xWifisendTaskHandle, 0x01, eSetValueWithOverwrite);
    main_window_render();  // For now, just render the main window
}

void send1(void)
{
    // Implementation for sending area code
    send_data.area_index = 0x01;
    xTaskNotify(xWifisendTaskHandle, 0x01, eSetValueWithOverwrite);
    main_window_render();
}

void send2(void)
{
    // Implementation for sending area code
    send_data.area_index = 0x02;
    xTaskNotify(xWifisendTaskHandle, 0x02, eSetValueWithOverwrite);
    main_window_render();
}

void send3(void)
{
    // Implementation for sending area code
    send_data.area_index = 0x03;
    xTaskNotify(xWifisendTaskHandle, 0x03, eSetValueWithOverwrite);
    main_window_render();

}
