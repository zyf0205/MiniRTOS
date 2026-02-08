;===========================================================
; 文件说明：FreeRTOS 在 ARM Cortex-M 上的任务调度汇编实现
; 汇编器：ARM armasm（Keil MDK 风格）
; 架构：  ARM Cortex-M3/M4/M7（Thumb-2 指令集）
; 作用：  实现任务的首次启动和上下文切换
;===========================================================

    PRESERVE8                       ; 保证栈 8 字节对齐（ARM ABI 规范要求）
    THUMB                           ; 使用 Thumb / Thumb-2 指令集

    AREA |.text|, CODE, READONLY    ; 定义代码段，只读

    ; 导出符号（供 C 代码调用）
    EXPORT vPortStartFirstTask      ; 启动第一个任务
    EXPORT SVC_Handler              ; SVC 中断服务函数（系统调用）
    EXPORT PendSV_Handler           ; PendSV 中断服务函数（任务切换）

    ; 导入外部符号（C 代码中定义）
    IMPORT pxCurrentTCB             ; 指向当前任务控制块（TCB）的指针
    IMPORT vTaskSwitchContext       ; FreeRTOS 的任务切换函数（选择下一个任务）


;===========================================================
; vPortStartFirstTask - 启动第一个任务
; 作用：初始化 MSP，然后通过 SVC 触发第一次任务调度
;===========================================================
vPortStartFirstTask PROC

    ; --- 第一步：重置主栈指针 MSP ---
    ; 0xE000ED08 是 VTOR 寄存器地址（向量表偏移寄存器）
    ; VTOR -> 向量表基地址 -> 向量表第一个元素（即初始 MSP 值）
    LDR     r0, =0xE000ED08         ; r0 = VTOR 寄存器的地址
    LDR     r0, [r0]                ; r0 = 向量表的基地址（读取 VTOR 的内容）
    LDR     r0, [r0]                ; r0 = 向量表[0]，即初始 MSP 栈顶值
    MSR     msp, r0                 ; 将 MSP 恢复为初始值（丢弃启动阶段的栈数据）

    ; --- 第二步：使能中断 ---
    CPSIE   i                       ; 使能全局中断（清除 PRIMASK）
    CPSIE   f                       ; 使能全局异常/故障中断（清除 FAULTMASK）
    DSB                             ; 数据同步屏障，确保前面的写操作完成
    ISB                             ; 指令同步屏障，清空流水线

    ; --- 第三步：触发 SVC 系统调用 ---
    SVC     0                       ; 触发 SVC 异常，编号 0
                                    ; CPU 会跳转到 SVC_Handler
                                    ; 在 SVC_Handler 中恢复第一个任务的上下文
    NOP                             ; 空操作（防止流水线问题）
    ENDP                            ; 函数结束


;===========================================================
; SVC_Handler - SVC 中断处理函数
; 作用：恢复第一个任务的上下文，让第一个任务开始运行
; 触发时机：vPortStartFirstTask 中的 SVC 0 指令
;===========================================================
SVC_Handler PROC

    ; --- 第一步：获取第一个任务的栈指针 ---
    ; pxCurrentTCB 是一个全局指针，指向当前任务的 TCB
    ; TCB 的第一个成员就是该任务的栈顶指针 pxTopOfStack
    LDR     r3, =pxCurrentTCB       ; r3 = &pxCurrentTCB（指针的地址）
    LDR     r1, [r3]                ; r1 = pxCurrentTCB（当前 TCB 的地址）
    LDR     r0, [r1]                ; r0 = pxCurrentTCB->pxTopOfStack（任务栈顶）

    ; --- 第二步：从任务栈中恢复 r4-r11（手动保存的寄存器）---
    ; 创建任务时，FreeRTOS 会在栈中预先压入初始寄存器值
    ; 栈中数据布局（从低地址到高地址）：
    ;   [r4, r5, r6, r7, r8, r9, r10, r11]  ← 手动保存的
    ;   [r0, r1, r2, r3, r12, LR, PC, xPSR] ← 硬件自动恢复的
    LDMIA   r0!, {r4-r11}           ; 从栈中弹出 r4-r11，从 r0 指向的地址连续读取 8 个字（32 字节）
    MSR     psp, r0                 ; 将更新后的 r0 设为 PSP（进程栈指针）
                                    ; 此时 PSP 指向硬件自动恢复的部分，指向 [r0, r1, r2, r3, r12, LR, PC, xPSR] 的起始位置
    ISB                             ; 指令同步屏障

    ; --- 第三步：清除中断屏蔽，使能所有中断 ---
    MOV     r0, #0
    MSR     basepri, r0             ; BASEPRI = 0，不屏蔽任何中断

    ; --- 第四步：返回任务（切换到线程模式 + 使用 PSP）---
    ; LR（r14）的特殊值 EXC_RETURN：
    ;   0xFFFFFFFD = 返回线程模式，使用 PSP
    ; ORR 0x0D 确保：
    ;   bit[0] = 1 → Thumb 模式
    ;   bit[2] = 1 → 返回时使用 PSP（而非 MSP）
    ;   bit[3] = 1 → 返回线程模式
    ORR     r14, r14, #0x0D         ; 设置 EXC_RETURN = 0xFFFFFFFD
    BX      r14                     ; 异常返回：
                                    ;   硬件自动从 PSP 恢复 r0-r3,r12,LR,PC,xPSR
                                    ;   CPU 跳转到任务函数开始执行
    ENDP


;===========================================================
; PendSV_Handler - PendSV 中断处理函数
; 作用：执行任务上下文切换（保存当前任务 + 恢复下一个任务）
; 触发时机：FreeRTOS 在 SysTick 或 yield 时挂起 PendSV
; 注意：PendSV 优先级设为最低，确保不会打断其他中断
;===========================================================
PendSV_Handler PROC

    ;=======================================================
    ;  第一部分：保存当前任务的上下文
    ;=======================================================

    ; --- 获取当前任务的 PSP ---
    ; 进入 PendSV 时，硬件已自动将 r0-r3,r12,LR,PC,xPSR 压入 PSP 栈
    MRS     r0, psp                 ; r0 = 当前任务的 PSP（硬件压栈后的位置）
    ISB                             ; 指令同步屏障

    ; --- 获取当前 TCB ---
    LDR     r3, =pxCurrentTCB       ; r3 = &pxCurrentTCB
    LDR     r2, [r3]                ; r2 = pxCurrentTCB（当前任务 TCB 地址）

    ; --- 手动保存 r4-r11 到当前任务栈 ---
    ; STMDB: Store Multiple, Decrement Before（先减后存，满递减栈）
    ; 硬件只自动保存 r0-r3,r12,LR,PC,xPSR
    ; r4-r11 需要我们手动保存
    STMDB   r0!, {r4-r11}           ; 将 r4-r11 压入任务栈，r0 自动递减

    ; --- 更新 TCB 中的栈顶指针 ---
    STR     r0, [r2]                ; pxCurrentTCB->pxTopOfStack = r0
                                    ; 保存更新后的栈顶位置

    ;=======================================================
    ;  第二部分：调用 FreeRTOS 选择下一个任务
    ;=======================================================

    ; 保存 r3（&pxCurrentTCB）和 r14（EXC_RETURN），因为调用 C 函数会破坏它们
    PUSH    {r3, r14}               ; 保护关键寄存器
    BL      vTaskSwitchContext      ; 调用 C 函数：
                                    ;   - 检查就绪任务列表
                                    ;   - 选择最高优先级任务
                                    ;   - 更新 pxCurrentTCB 指向新任务
    POP     {r3, r14}               ; 恢复 r3 和 r14

    ;=======================================================
    ;  第三部分：恢复新任务的上下文
    ;=======================================================

    ; --- 获取新任务的栈顶指针 ---
    LDR     r1, [r3]                ; r1 = pxCurrentTCB（已被 vTaskSwitchContext 更新为新任务）
    LDR     r0, [r1]                ; r0 = 新任务的 pxTopOfStack

    ; --- 从新任务栈中恢复 r4-r11 ---
    LDMIA   r0!, {r4-r11}           ; 弹出 r4-r11，r0 自动递增

    ; --- 设置 PSP 为新任务的栈指针 ---
    MSR     psp, r0                 ; PSP 指向新任务栈中硬件自动恢复的部分
    ISB                             ; 指令同步屏障

    ; --- 异常返回，切换到新任务 ---
    BX      r14                     ; EXC_RETURN 触发异常返回：
                                    ;   硬件自动从 PSP 恢复 r0-r3,r12,LR,PC,xPSR
                                    ;   CPU 开始执行新任务的代码
    ENDP


    ALIGN                           ; 4 字节对齐
    END                             ; 汇编文件结束