/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#include "kbhit.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

int _kbhit()
{
    // what this function adds on top of getchar is not having to wait
    // for an enter key
    struct termios oldTermios;
    struct termios newTermios;

    // get the terminal's attributes
    // we are interested in temporarily setting the control
    // characters (c_cc)
    // and the control modes (c_cflag) so we can read in our character.
    tcgetattr(STDIN_FILENO, &oldTermios);
    // save the current terminal attributes
    newTermios = oldTermios;

    // VMIN and VTIME are used in non-canonical mode
    // our read operation will return if there is one char to read
    newTermios.c_cc[VMIN] = 1;
    // timer in tenths of seconds- time to wait
    newTermios.c_cc[VTIME] = 0;
    // turn off echo, canonical input, signals
    newTermios.c_lflag &= ~(ICANON | ECHO | ISIG);
    // set our new temporary variables for standard input right now
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    // see if there is a character there
    int ch = getchar();

    // reset the terminal attributes for standard input right now
    // go back to canonical mode
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

    if (ch != EOF)
    {
        // put the character back
        ungetc(ch, stdin);
        // return that there is a character to be read
        return 1;
    }

    return 0;
}



