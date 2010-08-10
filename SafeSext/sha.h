/*
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
*/

#ifndef _SHA_H_
#define _SHA_H_


#define SHA_OUTSIZE 20

// sha

class SHAify {

public:
  SHAify();
  void add(unsigned char *data, int datalen);
  void final(unsigned char *out);
  void reset();

private:

  unsigned long H[5];
  unsigned long W[80];
  int lenW;
  unsigned long size[2];
};

#endif//_SHA_H_