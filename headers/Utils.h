/* 
 * File:   Utils.h
 * Author: rbuck
 *
 * Created on November 17, 2010, 3:26 PM
 */

#ifndef _UTILS_H
#define	_UTILS_H

#include <assert.h>
#include <time.h>
#include <string>
#include <stdio.h>
#include <vector>
#include <fstream>

#include "defines.h"

using namespace std;

class Utils {
public:
	/**
	 * Sets the timespec struct passed in to -seconds- and -nanoseconds- into
	 * the future.
	 *
	 * @param seconds the # of seconds to set the timespec forward
	 * @param nanoseconds the # of nanoseconds to set the timespec forward
	 * @param ts pointer to the timespec to set into the future
	 */
    static void get_timespec_future_time(int seconds, long int nanoseconds, struct timespec* ts);

    /**
     * Converts an integer to a string. Do not pass something bigger than an int
     * in (like uint, long, ulong, etc.) even though the compiler will let you.
     * If you do you will get back "-1".
     *
     * @param i an integer
     * @return the passed integer as a string
     */
    static string itoa(int i);

    /**
     * Reads each line in file and stores it in a vector
     * Ignores lines which begin with a '#' character
     * Ignores empty lines
     * Thanks to: http://www.cplusplus.com/forum/beginner/8388/
     *
     * @param file The file to open
     * @return A vector which each line is representative of on line in the file
     */
    static vector<string> read_file(string& file);
};

#endif	/* _UTILS_H */

