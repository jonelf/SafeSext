SafeSex v0.35
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
  - Go through and make sure all of the note contents are wiped 
    from the RichEdit's memory blocks when they are freed.
  - Code cleanup


<eof>
