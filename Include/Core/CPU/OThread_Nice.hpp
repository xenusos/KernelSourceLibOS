#pragma once

#define CONVERT_NICE_TO_KPRIO(n)  helper_nice_to_win_ish(n)
#define CONVERT_KPRIO_TO_NICE(n)  helper_win_to_nice[n]


static int8_t helper_win_to_nice[31 + 1] = {
	0, 19, 15, 10, 5, 3, 0, -2, -5, -7, -9, -12, -13, -14, -16, -17, -15, -17, -17, -17, -17, -17, -17, -17, -17, -18, -19, -19, -19, -20, -20, -20
};

static inline int8_t helper_nice_to_win(int8_t nice)
{
	for (int8_t i = 31 - 1; i != 0; i--)
		if (helper_win_to_nice[i] == nice)
			return i;
	return -1;
}


/*
LINUX: 
	go fuck yourself. i dont want to think about these odd values

XNU:
	idfk. it supports posix nice values tho

POSIX:
				 [most cpu time]  | [least cpu time]
	NICE range:  -20              - 20

WINDOWS:
	AHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH

	IDLE_PRIORITY_CLASS
		THREAD_PRIORITY_IDLE 1
		THREAD_PRIORITY_LOWEST 2
		THREAD_PRIORITY_BELOW_NORMAL 3
		THREAD_PRIORITY_NORMAL 4
		THREAD_PRIORITY_ABOVE_NORMAL 5
		THREAD_PRIORITY_HIGHEST 6
		THREAD_PRIORITY_TIME_CRITICAL 15
	
	BELOW_NORMAL_PRIORITY_CLASS
		THREAD_PRIORITY_IDLE 1
		THREAD_PRIORITY_LOWEST 4
		THREAD_PRIORITY_BELOW_NORMAL 5
		THREAD_PRIORITY_NORMAL 6
		THREAD_PRIORITY_ABOVE_NORMAL 7
		THREAD_PRIORITY_HIGHEST 8
		THREAD_PRIORITY_TIME_CRITICAL 15
	
	NORMAL_PRIORITY_CLASS
		THREAD_PRIORITY_IDLE 1
		THREAD_PRIORITY_LOWEST 6
		THREAD_PRIORITY_BELOW_NORMAL 7
		THREAD_PRIORITY_NORMAL 8
		THREAD_PRIORITY_ABOVE_NORMAL  9
		THREAD_PRIORITY_HIGHEST 10
		THREAD_PRIORITY_TIME_CRITICAL 15
	
	ABOVE_NORMAL_PRIORITY_CLASS
		THREAD_PRIORITY_IDLE 1
		THREAD_PRIORITY_LOWEST 8
		THREAD_PRIORITY_BELOW_NORMAL 9
		THREAD_PRIORITY_NORMAL 10
		THREAD_PRIORITY_ABOVE_NORMAL 11
		THREAD_PRIORITY_HIGHEST 12
		THREAD_PRIORITY_TIME_CRITICAL 15
	
	HIGH_PRIORITY_CLASS
		THREAD_PRIORITY_IDLE 1
		THREAD_PRIORITY_LOWEST 11
		THREAD_PRIORITY_BELOW_NORMAL 12
		THREAD_PRIORITY_NORMAL 13
		THREAD_PRIORITY_ABOVE_NORMAL 14
		THREAD_PRIORITY_HIGHEST 15
		THREAD_PRIORITY_TIME_CRITICAL 15
	
	REALTIME_PRIORITY_CLASS
		THREAD_PRIORITY_IDLE 16
		THREAD_PRIORITY_LOWEST 22
		THREAD_PRIORITY_BELOW_NORMAL 23
		THREAD_PRIORITY_NORMAL 24
		THREAD_PRIORITY_ABOVE_NORMAL 25
		THREAD_PRIORITY_HIGHEST 26
		THREAD_PRIORITY_TIME_CRITICAL 31
	
Nice to Windows conversion:


	Don't worry about how i scaled these values, just know that they do make some sense:
	yes, there is also some bias for windows threads with linux threads as they have to deal with more abstraction.

	Win - Nice
	1   - 19   // Almost always preempt and fuck off. Thread is idle-ish
	2   - 15   // IDLE_PRIORITY_CLASS/THREAD_PRIORITY_LOWEST
	3   - 10   // IDLE_PRIORITY_CLASS/THREAD_PRIORITY_BELOW_NORMAL
	4   - 5    // IDLE_PRIORITY_CLASS/THREAD_PRIORITY_NORMAL			BELOW_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_LOWEST
	5   - 3	   // IDLE_PRIORITY_CLASS/THREAD_PRIORITY_ABOVE_NORMAL		BELOW_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_BELOW_NORMAL
	6   - 0    // IDLE_PRIORITY_CLASS/THREAD_PRIORITY_HIGHEST			BELOW_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_NORMAL				NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_LOWEST
	7   - -2   //														BELOW_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_ABOVE_NORMAL		NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_BELOW_NORMAL
	8   - -5   															BELOW_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_HIGHEST				NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_NORMAL
	9   - -7   																															NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_ABOVE_NORMAL
	10  - -9   																															NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_HIGHEST
	11  - -12  																																												 ABOVE_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_LOWEST
	12  - -13  																																												 ABOVE_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_BELOW_NORMAL
	13  - -14  																																												 ABOVE_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_NORMAL
	14  - -16   																																											 ABOVE_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_ABOVE_NORMAL
	15  - -17  // this is the heightest val for a normal windows thread	(officially "dynamic priorities")																					 ABOVE_NORMAL_PRIORITY_CLASS/THREAD_PRIORITY_HIGHEST
	16  - -15  ///////////// "real time" stuff starts here /////////////
	// I know this isn't what windows does, but really, an idle thread should never be allowed a lot of of wasted time.  Real time or not.
	17  - -17  // nothing really uses these values
	18  - -17  // virtually unused
	19  - -17  // virtually unused
	20  - -17  // virtually unused
	21  - -17  // virtually unused
	22  - -17  // "Below" normal. do you, a process that demanded real time, really need to be that much slower?
	23  - -17  // virtually unused
	24  - -17  // Normal
	25  - -18  // Above normal - we should probably honor this for real realtime applications/games/etc
	26  - -19  // Realtime highest - likewise this. we should ensure that this is higher than above normal
	27  - -19  // virtually unused
	28  - -19  // virtually unused
	29  - -20  // virtually unused
	30  - -20  // virtually unused
	31  - -20  // abs max

*/