#include "stm32f4xx.h"
#include <stdio.h>
#include "led.h"
#include "usart.h"
#include "task.h"

/*===========================================================
 *  测试 1：同优先级时间片轮转
 *
 *  Task1 和 Task2 优先级相同
 *  不调用 taskYIELD()，靠 SysTick 自动切换
 *===========================================================*/

/*===========================================================
 *  Task1：红灯
 *===========================================================*/
void Task1_Red(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        LED_SetColor(LED_COLOR_RED);
        printf("Task1 RED  count=%d  tick=%d\r\n",
               (int)count++, (int)xTaskGetTickCount());

        for (volatile uint32_t i = 0; i < 300000; i++);

        LED_Off();

        for (volatile uint32_t i = 0; i < 300000; i++);

        /* 注意：没有调用 taskYIELD()！靠 SysTick 自动切换 */
    }
}

/*===========================================================
 *  Task2：蓝灯
 *===========================================================*/
void Task2_Blue(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        LED_SetColor(LED_COLOR_BLUE);
        printf("Task2 BLUE count=%d  tick=%d\r\n",
               (int)count++, (int)xTaskGetTickCount());

        for (volatile uint32_t i = 0; i < 300000; i++);

        LED_Off();

        for (volatile uint32_t i = 0; i < 300000; i++);
    }
}

/*===========================================================
 *  Task3：绿灯（高优先级）
 *
 *  优先级比 Task1 Task2 高
 *  如果抢占生效，Task3 会一直运行
 *  所以 Task3 里要主动让出（模拟干完活就让）
 *===========================================================*/
void Task3_Green(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        LED_SetColor(LED_COLOR_GREEN);
        printf("Task3 GREEN [HIGH] count=%d  tick=%d\r\n",
               (int)count++, (int)xTaskGetTickCount());

        for (volatile uint32_t i = 0; i < 200000; i++);

        LED_Off();

        for (volatile uint32_t i = 0; i < 200000; i++);
    }
}

/*===========================================================
 *  主函数
 *===========================================================*/
int main(void)
{
    UART_Init(115200);
    LED_Init();

    printf("\r\n\r\n");
    printf("===========================\r\n");
    printf("  MiniRTOS Phase 2 Test\r\n");
    printf("===========================\r\n\r\n");

    /*
     * 先测试同优先级轮转：
     *   注释掉 Task3，只创建 Task1 和 Task2
     *   看它们是否自动交替运行（不靠 taskYIELD）
     *
     * 再测试抢占：
     *   取消 Task3 的注释
     *   Task3 优先级更高，应该一直运行
     *   Task1 和 Task2 应该完全得不到 CPU
     */

    /* 同优先级 */
    xTaskCreate(Task1_Red, "Task1", 128, NULL, 1, NULL);
    xTaskCreate(Task2_Blue, "Task2", 128, NULL, 1, NULL);

    /* 高优先级（先注释掉，测完轮转再打开） */
    // xTaskCreate(Task3_Green, "Task3", 128, NULL, 3, NULL);

    printf("Starting scheduler...\r\n\r\n");
    vTaskStartScheduler();

    while (1);
}