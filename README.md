Block filter driver
-------------------

This is a barebones filter driver for Linux 3.x driver. This is partially inspired from the misc driver in FGFT project (in this github). A block filter is one that interposes block request. By default this driver interposes /dev/sda1. You can change this in misc.c or provide it as a module parameter.



How it works
------------

misc is a Linux misc device. It registers with the kernel and finds the appropriate block device from the given block device string (like /dev/sda). It then obtains the block device queue and replaces the request function with its own. The dummy request function just calls back the original function. Fairly straightforward.


Backstory
---------

I wanted to use a block filter driver and assumed I would find one over the internet. However, there was none to be found and I wrote one of my own. I am putting this on my github if anyone needs it. NO WARRANTIES. COMPILE TESTED ONLY. 

