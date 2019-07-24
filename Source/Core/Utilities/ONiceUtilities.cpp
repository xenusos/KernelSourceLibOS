/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "ONiceUtilities.hpp"
#include "NiceVals.h"

int8_t  Utilities::Nice::NiceToKPRIO(uint8_t windows)
{
    return helper_nice_to_win[windows + 20];
}

uint8_t Utilities::Nice::KPTRIOToNice(int8_t nice)
{
    return helper_win_to_nice[nice];
}
