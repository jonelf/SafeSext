SafeSext 0.35.1
---------------
Right now almost exactly the same as SafeSex 0.35 except it's migrated to [Microsoft Visual C++ 2010 Express.](http://www.microsoft.com/express/Downloads/Download-2010.aspx)

To make it compile I added

<code>#ifndef IDC_STATIC

#define IDC_STATIC				-1

#endif</code>

to resource.h and changed
<code>#include "afxres.h"</code>
to
<code>#include "windows.h"</code>
in safesex.rc

To comply to the license below, further changes to the original source can be found by diffing this repository.

Todo: 

* Find the bug that corrupts my longer notes, the same bug that's also the reason for me creating this repository.


SafeSex v0.35
-------------
December 20th, 2002
Copyright (C) 1998-2002 Nullsoft Inc.

In 1998 Nullsoft brought you Sex, a little virtual notepad for "scribbling"
things down. Then we made the UI a little different, but never released it.
Fast forward to 2002, and we added encryption and better profile support to
Sex, and called it SafeSex. 

SafeSex allows you to have some notes that are easily accessible, but relatively
secure. It sits on your screen, waiting for a click, and on a click it will
activate and give you access to your notes. That is, of course, if you enter
the password that your notes are encrypted with. For ease, the password will
be cached in memory, and expire after a user-configurable time.

The notes themselves are stored in RTF, and encrypted when stored to disk. 
When the SafeSex window is not open, the notes are not stored in memory. 
They are encrypted and decrypted on demand.

The encryption used is Blowfish, using the 20 byte SHA-1 of the passphrase
as the key. It uses Blowfish in CBC mode, with the initialization vector being
incremented every time.

SafeSex keeps running in the background, using very little memory and resources,
and automatically keeps itself running across reboots (i.e. if you reboot with
SafeSex running, it will come back on startup)

Todo:

  * Go through and make sure all of the note contents are wiped from the RichEdit's memory blocks when they are freed.
  * Code cleanup


License
-------

  SafeSex 
  Copyright (C) 1998-2002 Nullsoft, Inc.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
