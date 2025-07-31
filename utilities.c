#include "utilities.h"

int16_t AxesValue(int16_t axisValue,int16_t e_value) {

        int srcRangeMax;
        int srcRangeMin;
        int dstRangeMin;
        int dstRangeMax;

        switch(axisValue) {
                case 0:
                        //throttle
                        srcRangeMin = -32786;
                        srcRangeMax = 32786;
                        dstRangeMin = 0;
                        dstRangeMax = 1400;
                        break;
                case 1: //yaw
                case 2: //roll
                case 3: //pitch
                        srcRangeMin = -32768;
                        srcRangeMax = 32767;
                        dstRangeMin = -700;
                        dstRangeMax = 700;
                        break;
                default:
                        //No Scaling
                        srcRangeMin =-32786;
                        srcRangeMax = 32786;
                        dstRangeMin = srcRangeMin;
                        dstRangeMax = srcRangeMax;
                        break;
        }
        return (dstRangeMin + (e_value - srcRangeMin) * (dstRangeMax -dstRangeMin) /(srcRangeMax - srcRangeMin));
}

