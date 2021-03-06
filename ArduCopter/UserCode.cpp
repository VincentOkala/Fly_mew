#include <AP_Notify/Buzzer.h>
#include <math.h>

#include "Copter.h"
#include "pid.h"

#define FRAME_W 320
#define FRAME_H 240
#define INV_SCALE_X ((float)FRAME_W / 640.0f)
#define INV_SCALE_Y ((float)FRAME_H / 480.0f)
#define FOCAL_PIXEL_X 720.5 * INV_SCALE_X
#define FOCAL_PIXEL_Y 720.5 * INV_SCALE_Y

Buzzer buzzer;
PID pid_roll;
PID pid_pitch;
// int buzzer_ifg = 0;

// recommended PID params Kp:6 Ki:0 Kd:9 for 20hz frame rate, image resolution: 320*240
// PID input: error in pixels, PID output: centidegree, saturate at +- 1500 centidegrees.

// void tiltCompensate(int *X_Err, int *Y_Err, float currentRoll, float currentPitch)
// {
//     *X_Err = *X_Err + tanf(currentRoll) * FOCAL_PIXEL_X;
//     *Y_Err = *Y_Err + tanf(currentPitch) * FOCAL_PIXEL_Y;
// }

#ifdef USERHOOK_INIT
void Copter::userhook_init()
{
    // put your initialisation code here
    // this will be called once at start-up
    c_buff = 0;  //0: temp-buf
    c_state = 0; //0: wait-start-buff, 1: data-buffer, 2: end-buffer-->0
    ips_bytes = 0;
    s16_US_HEIGHT = 156;
    optflow.init();

    pid_roll.pid_set_k_params(g.RYA_PID_P_ROLL, g.RYA_PID_I_ROLL, g.RYA_PID_D_ROLL, 0.01, 1000 - abs(g.RYA_OFFSET_ROLL)); // 0.1, 0, 0.125, 0.01, 1000
    pid_pitch.pid_set_k_params(g.RYA_PID_P_PITCH, g.RYA_PID_I_PITCH, g.RYA_PID_D_PITCH, 0.01, 1000 - abs(g.RYA_OFFSET_PITCH));

    buzzer.init();
}
#endif

int old_X_err_in_pixel = 0;
int old_Y_err_in_pixel = 0;

float old_target_roll = 0;
float old_target_pitch = 0;

#ifdef USERHOOK_FASTLOOP
void Copter::userhook_FastLoop()
{
    // put your 100Hz code here

    pid_roll.pid_set_k_params(g.RYA_PID_P_ROLL, g.RYA_PID_I_ROLL, g.RYA_PID_D_ROLL, 0.01, 1000 - abs(g.RYA_OFFSET_ROLL)); // 0.1, 0, 0.125, 0.01, 1000
    pid_pitch.pid_set_k_params(g.RYA_PID_P_PITCH, g.RYA_PID_I_PITCH, g.RYA_PID_D_PITCH, 0.01, 1000 - abs(g.RYA_OFFSET_PITCH));

    // pid_roll.pid_set_k_params(1,0,1, 0.01, 1000); // 0.1, 0, 0.125, 0.01, 1000
    // pid_pitch.pid_set_k_params(1,0,1, 0.01, 1000);

    // uartF: serial5, baud 115200
    //================================IPS_POSITION====================================//
    // Get available bytes
    ips_bytes = hal.uartF->available();
    while (ips_bytes-- > 0)
    {
        // Get data string here
        ips_char[0] = hal.uartF->read();
        if (ips_char[0] == 's')
        {
            c_buff = 1;
            c_state = 1;
        }
        else if (ips_char[0] == 'e')
        {
            // end-of-frame: get ips_pos & time_stamp
            if (ips_char[5] == ',')
            {
                // valid frame
                ips_data[0] = ips_char[1] - 0x30;
                ips_data[1] = (ips_char[2] - 0x30) * 100 + (ips_char[3] - 0x30) * 10 + (ips_char[4] - 0x30); //pos_x
                ips_data[2] = (ips_char[6] - 0x30) * 100 + (ips_char[7] - 0x30) * 10 + (ips_char[8] - 0x30); //pos_y
            }
            c_buff = 0;
            c_state = 0;
        }
        else
        {
            if (c_state == 1)
            {
                ips_char[c_buff] = ips_char[0];
                c_buff++;
            }
        }
    }
    // Get current information
    int height = rangefinder.distance_cm_orient(ROTATION_PITCH_270);
    float curr_height = (float)height; // cm
    // float curr_roll = ahrs.roll;       // rad
    // float curr_pitch = ahrs.pitch;     // rad

    int isThereaAnyObject = ips_data[0];
    //cliSerial->printf("isThereaAnyObject %d \n", isThereaAnyObject);

    int X_err_in_pixel = ips_data[1] - 320;
    int Y_err_in_pixel = ips_data[2] - 240;

    // cliSerial->printf("px: %d %d \n", X_err_in_pixel, Y_err_in_pixel);
    // Process information
    if (isThereaAnyObject){
        buzzer.on(true);
        if (old_X_err_in_pixel != X_err_in_pixel && old_Y_err_in_pixel != Y_err_in_pixel) //&& curr_roll < MAX_ANGEL && curr_roll >- MAX_ANGEL && curr_pitch < MAX_ANGEL && curr_pitch > -MAX_ANGEL )
        {
            old_X_err_in_pixel = X_err_in_pixel;
            old_Y_err_in_pixel = Y_err_in_pixel;

            // buzzer_ifg = 1;
            // buzzer.on(true);
            //buzzer.play_pattern(Buzzer::BuzzerPattern::ARMING_BUZZ);
            // float pixel_per_cm = curr_height * 0.8871428438 * 2 / 800;
            float pixel_per_cm = curr_height * 0.5543090515 * 2 / 800;
            // cliSerial->printf("height: %f \n", curr_height);

            // tiltCompensate(&X_err_in_pixel, &Y_err_in_pixel, curr_roll, curr_pitch);

            int X_err_in_cm = X_err_in_pixel * pixel_per_cm;
            int Y_err_in_cm = Y_err_in_pixel * pixel_per_cm;

            // cliSerial->printf("X_err Y_err %d %d: \n", X_err_in_cm, Y_err_in_cm);

            // cliSerial->printf("sttt: %d %d \n", X_err_in_cm, Y_err_in_cm);

            target_roll_user = pid_roll.pid_process(X_err_in_cm, millis());
            old_target_roll = target_roll_user;

            target_pitch_user = pid_pitch.pid_process(Y_err_in_cm, millis());
            old_target_pitch = target_pitch_user;
        }
        else
        {
            // buzzer_ifg = 0;
            target_roll_user = old_target_roll;
            target_pitch_user = old_target_pitch;
        }
    }
    else{
        buzzer.on(false);
        target_roll_user = 0;
        target_pitch_user = 0;
    }
    // cliSerial->printf("tg: %f %f \n", target_roll_user, target_pitch_user);
}
#endif

#ifdef USERHOOK_50HZLOOP
void Copter::userhook_50Hz()
{
    // put your 50Hz code here
}
#endif

#ifdef USERHOOK_MEDIUMLOOP
void Copter::userhook_MediumLoop()
{
    // put your 20Hz code here
    //==============================TEMPERATURE======================================//
    air_temperature = barometer.get_temperature();
    // hal.uartF->printf("temp:%f",air_temperature);

    //==============================IPS_TRANSMIT======================================//
    hal.uartE->printf("{PARAM,TRIGGER_US}\n");
    ips_delay_ms = AP_HAL::millis(); // trigger IPS_transmission on Tiva C
}
#endif

#ifdef USERHOOK_SLOWLOOP
// int buzzer_ifg2 = 0;
void Copter::userhook_SlowLoop()
{
    // // put your 3.3Hz code here
    // if (buzzer_ifg == 1)
    //     {
    //         buzzer_ifg2++;
    //         if(buzzer_ifg2%2)
    //             buzzer.on(true);
    //         else
    //             buzzer.on(false);
    //     }
    // else
    //     buzzer.on(false);
}
#endif

#ifdef USERHOOK_SUPERSLOWLOOP
void Copter::userhook_SuperSlowLoop()
{
    // put your 1Hz code here
}
#endif