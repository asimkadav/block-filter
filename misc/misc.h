/*
*
* Copyright (c) 2013, Asim Kadav, asimkadav@gmail.com
* Updated 2025 for modern Linux kernels (5.0+)
*
*  A simple block filter driver
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef _BLOCK_FILTER_H
#define _BLOCK_FILTER_H

#include <linux/ioctl.h>

/*
 * IOCTL commands for the block filter control interface
 * Available via /dev/blockfilter
 */

/* Get statistics (returns number of BIOs processed) */
#define MISC_GET _IOR('B', 0x01, unsigned long)

/* Control operation (reserved for future use) */
#define MISC_PUT _IOW('B', 0x02, unsigned long)

#endif /* _BLOCK_FILTER_H */
