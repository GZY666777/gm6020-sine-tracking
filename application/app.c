/**
 * @file    app.c
 * @brief   Speed / position sine tracking for one GM6020.
 *
 * Control structure
 * -----------------
 *   Speed mode:
 *       rpm_ref = A_speed * sin(2*pi*f_speed*t)
 *       voltage = PI(rpm_ref - rpm) + Kff * rpm_ref
 *
 *   Position mode (cascade):
 *       deg_ref = zero + A_pos * sin(2*pi*f_pos*t)
 *       rpm_ref = P(deg_ref - deg) + Kff_pos * d(deg_ref)/dt
 *       voltage = PI(rpm_ref - rpm) + Kff * rpm_ref
 *
 *   The position loop re-captures its zero when it is entered, so switching
 *   from the speed demo to the position demo never produces a step.
 *
 * Protection
 * ----------
 *   - 100 ms motor feedback watchdog (latched, output forced to 0)
 *   - 250 ms independent watchdog refreshed by the 1 kHz control step
 *   - CAN bus-off / repeated TX failure / overtemperature / position limit
 */

#include "app.h"

#include <math.h>

#include "app_config.h"
#include "board_io.h"
#include "bsp_can.h"
#include "can.h"
#include "debug_uart.h"
#include "motor_gm6020.h"
#include "pid.h"
#include "safety.h"
#include "sine_profile.h"

#define DEG_PER_RPM_SEC 6.0f

typedef struct
{
    volatile uint32_t now_ms;
    volatile uint8_t state;
    volatile uint8_t mode;
    volatile uint8_t auto_demo;
    volatile float target_rpm;
    volatile float actual_rpm;
    volatile float target_deg;
    volatile float actual_deg;
    volatile float pos_rel_deg;
    volatile int16_t voltage_cmd;
    volatile uint32_t fault_bits;
} app_snapshot_t;

static app_snapshot_t s_snap;

static volatile uint8_t s_state = APP_STATE_WAIT;
static volatile uint8_t s_mode = APP_MODE_SPEED;
static volatile uint8_t s_auto = 1u;
static volatile uint8_t s_initialized = 0u;

static pid_t s_speed_pid;
static pid_t s_pos_pid;

static float s_pos_zero_deg = 0.0f;
static float s_t_s = 0.0f;
static float s_mode_time_s = 0.0f;
static uint32_t s_wait_since_ms = 0u;
static uint32_t s_last_fault_bits = 0u;

/* Commands are executed inside the 1 kHz control step so that all CAN
 * traffic and state changes happen in one context. */
#define APP_CMD_NONE     0u
#define APP_CMD_SPEED    1u
#define APP_CMD_POSITION 2u
#define APP_CMD_AUTO     3u
#define APP_CMD_STOP     4u
#define APP_CMD_CLEAR    5u

static volatile uint8_t s_cmd = APP_CMD_NONE;
static volatile uint8_t s_clear_result = 0u; /* 0 none, 1 ok, 2 rejected */

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static float app_wrap180(float angle_deg)
{
    while (angle_deg > 180.0f)
    {
        angle_deg -= 360.0f;
    }
    while (angle_deg < -180.0f)
    {
        angle_deg += 360.0f;
    }
    return angle_deg;
}

static float app_clamp(float value, float min_value, float max_value)
{
    if (value > max_value)
    {
        return max_value;
    }
    if (value < min_value)
    {
        return min_value;
    }
    return value;
}

static int16_t app_float_to_i16(float value)
{
    if (value >= 0.0f)
    {
        return (int16_t)(value + 0.5f);
    }
    return (int16_t)(value - 0.5f);
}

static void app_update_measurements(void)
{
    float absolute_deg = GM6020_GetAngleDeg();

    s_snap.actual_rpm = GM6020_GetSpeedRpm();
    s_snap.pos_rel_deg = app_wrap180(absolute_deg - s_pos_zero_deg);
    s_snap.actual_deg = s_snap.pos_rel_deg;
    s_snap.fault_bits = Safety_GetFaults();
}

static void app_enter_mode(uint8_t mode)
{
    s_mode = mode;
    s_t_s = 0.0f;
    s_mode_time_s = 0.0f;

    PID_Reset(&s_speed_pid);
    PID_Reset(&s_pos_pid);

    if (mode == APP_MODE_POSITION)
    {
        /* Start the position sine where the rotor currently is. */
        s_pos_zero_deg = GM6020_GetAngleDeg();
    }
}

static void app_enter_wait(void)
{
    s_state = APP_STATE_WAIT;
    s_wait_since_ms = 0u;
    s_snap.state = s_state;
}

static void app_execute_cmd(uint8_t cmd)
{
    if (cmd == APP_CMD_NONE)
    {
        return;
    }

    if (cmd == APP_CMD_STOP)
    {
        s_auto = 0u;
        s_state = APP_STATE_STOP;
        s_snap.state = s_state;
        s_snap.target_rpm = 0.0f;
        s_snap.target_deg = 0.0f;
        s_snap.voltage_cmd = 0;
        return;
    }

    if (cmd == APP_CMD_CLEAR)
    {
        float pos_rel_deg;

        if (Safety_IsFaulted() == 0u)
        {
            s_clear_result = 1u;
            return;
        }

        pos_rel_deg = app_wrap180(GM6020_GetAngleDeg() - s_pos_zero_deg);

        if (Safety_TryClear(HAL_GetTick(), GM6020_GetFeedback(),
                            pos_rel_deg) != 0u)
        {
            PID_Reset(&s_speed_pid);
            PID_Reset(&s_pos_pid);
            app_enter_wait();
            s_clear_result = 1u;
        }
        else
        {
            s_clear_result = 2u;
        }
        return;
    }

    /* Mode changes are refused while a fault is latched. */
    if (Safety_IsFaulted() != 0u)
    {
        return;
    }

    switch (cmd)
    {
        case APP_CMD_SPEED:
            s_auto = 0u;
            app_enter_mode(APP_MODE_SPEED);
            break;

        case APP_CMD_POSITION:
            s_auto = 0u;
            app_enter_mode(APP_MODE_POSITION);
            break;

        case APP_CMD_AUTO:
            s_auto = 1u;
            app_enter_mode(APP_MODE_SPEED);
            break;

        default:
            return;
    }

    app_enter_wait();
    s_snap.auto_demo = s_auto;
}

static void app_handle_char(char ch)
{
    switch (ch)
    {
        case 'a':
        case 'A':
            App_RequestMode(APP_MODE_AUTO);
            break;

        case 's':
        case 'S':
            App_RequestMode(APP_MODE_SPEED);
            break;

        case 'p':
        case 'P':
            App_RequestMode(APP_MODE_POSITION);
            break;

        case 'x':
        case 'X':
            App_Stop();
            break;

        case 'r':
        case 'R':
            App_ClearFault();
            break;

        case '?':
            DebugUART_Write("\r\ncommands: a=auto s=speed p=position "
                            "x=stop r=clear-fault\r\n");
            break;

        default:
            break;
    }
}

static void app_print_help(void)
{
    DebugUART_Write("\r\n"
                    "GM6020 sine tracking\r\n"
                    "  s : speed sine\r\n"
                    "  p : position sine\r\n"
                    "  a : auto demo (12 s speed, 12 s position)\r\n"
                    "  x : stop (zero voltage)\r\n"
                    "  r : clear latched fault\r\n"
                    "  ? : help\r\n");
    DebugUART_Write("CSV: ms,state,mode,auto,rpm_ref_x10,rpm_x10,"
                    "deg_ref_rel_x10,deg_rel_x10,voltage,temp,fault_hex\r\n");
}

static void app_print_faults(uint32_t faults)
{
    uint32_t bit;
    uint8_t first = 1u;

    DebugUART_Printf("# FAULT 0x%08lX:", (unsigned long)faults);

    for (bit = SAFETY_FAULT_FEEDBACK_TIMEOUT;
         bit <= SAFETY_FAULT_CAN_TX;
         bit = (bit << 1))
    {
        if ((faults & bit) != 0u)
        {
            DebugUART_Printf("%s%s", (first != 0u) ? " " : ",",
                             Safety_FaultName(bit));
            first = 0u;
        }
    }

    DebugUART_Write("\r\n");
}

static void app_update_led(uint32_t now_ms)
{
    uint8_t red = 0u;
    uint8_t green = 0u;
    uint8_t blue = 0u;

    switch (s_state)
    {
        case APP_STATE_WAIT:
            blue = (uint8_t)((now_ms / 200u) & 1u);
            break;

        case APP_STATE_RUN:
            if (s_mode == APP_MODE_SPEED)
            {
                green = 1u;
            }
            else
            {
                blue = 1u;
            }
            break;

        case APP_STATE_FAULT:
            red = (uint8_t)((now_ms / 100u) & 1u);
            break;

        case APP_STATE_STOP:
        default:
            break;
    }

    BoardIO_SetLED(red, green, blue);
}

static void app_log(void)
{
    const gm6020_feedback_t *fb = GM6020_GetFeedback();

    DebugUART_Printf("%lu,%u,%u,%u,%d,%d,%d,%d,%d,%u,0x%08lX\r\n",
                     (unsigned long)s_snap.now_ms,
                     (unsigned)s_snap.state,
                     (unsigned)s_snap.mode,
                     (unsigned)s_snap.auto_demo,
                     (int)(s_snap.target_rpm * 10.0f),
                     (int)(s_snap.actual_rpm * 10.0f),
                     (int)(s_snap.target_deg * 10.0f),
                     (int)(s_snap.actual_deg * 10.0f),
                     (int)s_snap.voltage_cmd,
                     (unsigned)fb->temperature,
                     (unsigned long)s_snap.fault_bits);
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void App_Init(void)
{
    DebugUART_Init();
    BoardIO_Init();
    GM6020_Init(&hcan1, GM6020_ID);
    Safety_Init();

    PID_Init(&s_speed_pid, SPEED_KP, SPEED_KI, SPEED_KD,
             -(float)GM6020_VOLTAGE_MAX, (float)GM6020_VOLTAGE_MAX);
    PID_Init(&s_pos_pid, POS_KP, POS_KI, POS_KD,
             -POS_MAX_RPM, POS_MAX_RPM);

    s_state = APP_STATE_WAIT;
    s_mode = APP_MODE_SPEED;
    s_auto = 1u;
    s_pos_zero_deg = 0.0f;
    s_t_s = 0.0f;
    s_mode_time_s = 0.0f;
    s_wait_since_ms = 0u;

    s_snap.state = s_state;
    s_snap.mode = s_mode;
    s_snap.auto_demo = s_auto;

    BoardIO_SetLED(0u, 0u, 0u);

    DebugUART_Write("\r\n== GM6020 sine tracking ==\r\n");
    DebugUART_Printf("motor_id=%u can=1Mbps control=1kHz\r\n",
                     (unsigned)GM6020_GetMotorId());
    app_print_help();

    s_initialized = 1u;
}

void App_ControlStep(void)
{
    uint32_t now_ms = HAL_GetTick();
    const gm6020_feedback_t *fb;
    float actual_deg;
    float pos_rel_deg;
    float target_rpm;
    float target_deg;
    float voltage;
    int16_t cmd;
    uint8_t tx_ok;

    s_snap.now_ms = now_ms;

    /* Refresh the hardware watchdog from the real-time task.  If this
     * function stops running, the MCU resets after 250 ms. */
    Safety_Feed();

    if (s_initialized == 0u)
    {
        return;
    }

    if (s_cmd != APP_CMD_NONE)
    {
        uint8_t pending_cmd = s_cmd;
        s_cmd = APP_CMD_NONE;
        app_execute_cmd(pending_cmd);
    }

    if (s_state == APP_STATE_FAULT)
    {
        (void)GM6020_SendVoltageCmd(0);
        s_snap.voltage_cmd = 0;
        s_snap.target_rpm = 0.0f;
        s_snap.target_deg = 0.0f;
        app_update_measurements();
        return;
    }

    if (s_state == APP_STATE_STOP)
    {
        (void)GM6020_SendVoltageCmd(0);
        s_snap.voltage_cmd = 0;
        s_snap.target_rpm = 0.0f;
        s_snap.target_deg = 0.0f;
        app_update_measurements();
        return;
    }

    if (s_state == APP_STATE_WAIT)
    {
        (void)GM6020_SendVoltageCmd(0);
        s_snap.voltage_cmd = 0;
        s_snap.target_rpm = 0.0f;
        s_snap.target_deg = 0.0f;
        app_update_measurements();

        if (GM6020_IsOnline(now_ms, SAFETY_FEEDBACK_TIMEOUT_MS) != 0u)
        {
            if (s_wait_since_ms == 0u)
            {
                s_wait_since_ms = now_ms;
            }

            if ((now_ms - s_wait_since_ms) >= WAIT_STABLE_MS)
            {
                s_pos_zero_deg = GM6020_GetAngleDeg();
                s_t_s = 0.0f;
                s_mode_time_s = 0.0f;
                PID_Reset(&s_speed_pid);
                PID_Reset(&s_pos_pid);
                s_state = APP_STATE_RUN;
                s_snap.state = s_state;

                DebugUART_Printf("# RUN mode=%u zero_deg_x10=%d\r\n",
                                 (unsigned)s_mode,
                                 (int)(s_pos_zero_deg * 10.0f));
            }
        }
        else
        {
            s_wait_since_ms = 0u;
        }

        return;
    }

    /* ---------------------------- RUN ---------------------------- */
    fb = GM6020_GetFeedback();
    actual_deg = GM6020_GetAngleDeg();
    pos_rel_deg = app_wrap180(actual_deg - s_pos_zero_deg);

    s_t_s += CONTROL_DT_S;
    s_mode_time_s += CONTROL_DT_S;

    if (s_auto != 0u && s_mode_time_s >= AUTO_PHASE_S)
    {
        uint8_t next_mode = (s_mode == APP_MODE_SPEED) ? APP_MODE_POSITION
                                                       : APP_MODE_SPEED;
        app_enter_mode(next_mode);
        actual_deg = GM6020_GetAngleDeg();
        pos_rel_deg = app_wrap180(actual_deg - s_pos_zero_deg);
    }

    target_rpm = 0.0f;
    target_deg = 0.0f;

    if (s_mode == APP_MODE_SPEED)
    {
        target_rpm = Sine_Value(SINE_SPEED_AMP_RPM,
                                SINE_SPEED_FREQ_HZ, s_t_s);
    }
    else
    {
        float velocity_deg_s;

        target_deg = s_pos_zero_deg +
                     Sine_Value(SINE_POS_AMP_DEG, SINE_POS_FREQ_HZ, s_t_s);
        velocity_deg_s = Sine_Slope(SINE_POS_AMP_DEG, SINE_POS_FREQ_HZ, s_t_s);

        /* Wrap the position error so the shortest path is always used.
         * The PID sees an adjusted setpoint whose error is the wrapped one. */
        target_rpm = PID_Update(&s_pos_pid,
                                actual_deg + app_wrap180(target_deg - actual_deg),
                                actual_deg,
                                CONTROL_DT_S,
                                velocity_deg_s / DEG_PER_RPM_SEC);
    }

    voltage = PID_Update(&s_speed_pid, target_rpm,
                         GM6020_GetSpeedRpm(),
                         CONTROL_DT_S,
                         target_rpm * SPEED_FF_COUNTS_PER_RPM);

    voltage = app_clamp(voltage,
                        -(float)GM6020_VOLTAGE_MAX,
                        (float)GM6020_VOLTAGE_MAX);

    cmd = app_float_to_i16(voltage);
    tx_ok = GM6020_SendVoltageCmd(cmd);

    Safety_Update(now_ms, fb, pos_rel_deg,
                  (s_mode == APP_MODE_POSITION) ? 1u : 0u,
                  tx_ok);

    if (Safety_IsFaulted() != 0u)
    {
        (void)GM6020_SendVoltageCmd(0);
        s_state = APP_STATE_FAULT;
        s_snap.state = s_state;
        cmd = 0;
    }

    s_snap.mode = s_mode;
    s_snap.auto_demo = s_auto;
    s_snap.target_rpm = target_rpm;
    s_snap.actual_rpm = GM6020_GetSpeedRpm();
    s_snap.target_deg = app_wrap180(target_deg - s_pos_zero_deg);
    s_snap.actual_deg = pos_rel_deg;
    s_snap.pos_rel_deg = pos_rel_deg;
    s_snap.voltage_cmd = cmd;
    s_snap.fault_bits = Safety_GetFaults();
}

void App_MainLoop(void)
{
    static uint32_t last_log_ms = 0u;
    static uint32_t last_led_ms = 0u;
    static uint8_t key_step = 0u;
    uint32_t now_ms = HAL_GetTick();
    uint32_t faults;

    int ch;
    while ((ch = DebugUART_ReadChar()) >= 0)
    {
        app_handle_char((char)ch);
    }

    if (BoardIO_KeyTakePress() != 0u)
    {
        if (Safety_IsFaulted() != 0u)
        {
            App_ClearFault();
        }
        else
        {
            switch (key_step)
            {
                case 0:
                    App_RequestMode(APP_MODE_SPEED);
                    break;
                case 1:
                    App_RequestMode(APP_MODE_POSITION);
                    break;
                case 2:
                    App_Stop();
                    break;
                default:
                    App_RequestMode(APP_MODE_AUTO);
                    break;
            }
            key_step = (uint8_t)((key_step + 1u) & 0x03u);
        }
    }

    faults = Safety_GetFaults();
    if (faults != s_last_fault_bits)
    {
        if (faults != 0u)
        {
            app_print_faults(faults);
        }
        else
        {
            DebugUART_Write("# faults cleared\r\n");
        }
        s_last_fault_bits = faults;
    }

    if (s_clear_result != 0u)
    {
        if (s_clear_result == 1u)
        {
            DebugUART_Write("# fault cleared, waiting for feedback\r\n");
        }
        else
        {
            DebugUART_Write("# clear rejected: feedback/temperature/"
                            "position\r\n");
        }
        s_clear_result = 0u;
    }

    if ((now_ms - last_log_ms) >= LOG_PERIOD_MS)
    {
        last_log_ms = now_ms;
        app_log();
    }

    if ((now_ms - last_led_ms) >= 50u)
    {
        last_led_ms = now_ms;
        app_update_led(now_ms);
    }
}

void App_RequestMode(uint8_t mode)
{
    if (Safety_IsFaulted() != 0u)
    {
        DebugUART_Write("# fault active - press r or key to clear\r\n");
        return;
    }

    if (mode == APP_MODE_AUTO)
    {
        s_cmd = APP_CMD_AUTO;
    }
    else if (mode == APP_MODE_SPEED)
    {
        s_cmd = APP_CMD_SPEED;
    }
    else if (mode == APP_MODE_POSITION)
    {
        s_cmd = APP_CMD_POSITION;
    }
    else
    {
        return;
    }
}

void App_Stop(void)
{
    s_cmd = APP_CMD_STOP;
    DebugUART_Write("# STOP requested\r\n");
}

void App_ClearFault(void)
{
    if (Safety_IsFaulted() == 0u)
    {
        return;
    }

    s_cmd = APP_CMD_CLEAR;
}

uint8_t App_GetState(void)
{
    return s_state;
}

uint8_t App_GetMode(void)
{
    return s_mode;
}

uint8_t App_IsAuto(void)
{
    return s_auto;
}
