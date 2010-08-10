/*
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

*/

#include <windows.h>
#include <windowsx.h>
#include <richedit.h>

#include "resource.h"

#include "blowfish.h"
#include "sha.h"


#define APP_NAME "SafeSext"

// todo:
// multimonitor support =)

/*
** file format:
** SAFESEX + ^Z + 0xBE + 0xEF + 0xF0 + 0x0D
** 2 byte version 00 + 02
** 8 byte CBC IV (should start at (random?), and go up by 1) -- aka file revision
** -- begin blowfish with SHA-1 of passphrase --
** file contents
**   8 bytes, SAFESEX1
**   rtf data, padded to 8 bytes
*/

const unsigned char file_sig[12]={'S','A','F','E','S','E','X',26,0xBE,0xEF,0xF0,0x0D};
const unsigned char data_sig[8]={'S','A','F','E','S','E','X','1'};

unsigned char g_file_revision[8];

unsigned int file_timeout=30000;
int g_keyvalid;
unsigned int g_keytimeouttime;
unsigned char g_shakey[20];


unsigned char g_bf_cbc[8];
BLOWFISH_CTX g_bf;
#define CLEAR_BF { memset(&g_bf,0,sizeof(g_bf)); memset(g_bf_cbc,0,sizeof(g_bf_cbc)); }

int g_text_dirty;


#define MODE_NORMAL 0
#define MODE_ONCE 1
#define MODE_SESSION 2
int g_mode=MODE_NORMAL;

int g_noclose;
int g_active_cnt;
int g_active;
int moved=0;
int config_w=300, config_h=200,config_x=50,config_y=50,config_border=5,
	config_color=RGB(255,255,255), config_bcolor1=RGB(100,100,210), config_bcolor2=0, config_aot=1;

char app_name[32+MAX_PATH];
char text_file[MAX_PATH];
char ini_file[MAX_PATH];
char exe_file[MAX_PATH];
char profilename[256];

void config_read();
int read_text(); // 1 on error
void write_text();
void config_write();

HMENU hmenu_main;
HWND hwnd_rich,hwnd_main;

HINSTANCE hMainInstance;
BOOL WINAPI ProfilesProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL InitApplication(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


int WINAPI WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow) 
{
  if (lpCmdLine)
  {
    while (*lpCmdLine)
    {
      if (!strnicmp(lpCmdLine,"/ONCE",5) && (lpCmdLine[5] == ' ' || !lpCmdLine[5]))
      {
        lpCmdLine += 5;
        g_mode=MODE_ONCE;
      }
      if (!strnicmp(lpCmdLine,"/ONESESSION",11) && (lpCmdLine[11] == ' ' || !lpCmdLine[11]))
      {
        lpCmdLine += 11;
        g_mode=MODE_SESSION;
      }

      if (!strnicmp(lpCmdLine,"/PROFILE=",9))
      {
        lstrcpyn(profilename,lpCmdLine+9,sizeof(profilename));
        break;
      }
      lpCmdLine++;
    }
  }
  { // load richedit DLL
    static WNDCLASS wc;
    static char str1[]="RichEd20.dll";
    static char str2[]="RichEdit20A";
    if (!LoadLibrary(str1))
    {
      str1[6]='3';
      str2[7]='2';
      LoadLibrary(str1);
    }

    // make richedit20a point to RICHEDIT
    if (!GetClassInfo(NULL,str2,&wc))
    {
      str2[8]=0;
      GetClassInfo(NULL,str2,&wc);
      wc.lpszClassName = str2;
      str2[8]='2';
      RegisterClass(&wc);
    }
  }

  if (!profilename[0])
  {
		char b[MAX_PATH],*p=b;
		GetModuleFileName(hInstance,b,sizeof(b));
		while (*p) p++;
		while (p > b && *p != '.') p--;
		if (p > b) *p = 0;
		while (p >= b && *p != '\\') p--;
    if (p >= b) *p=0;
    lstrcpyn(profilename,++p,sizeof(profilename));

    if (!DialogBox(hInstance,MAKEINTRESOURCE(IDD_PROFILES),GetDesktopWindow(),ProfilesProc))
      return 0;
  }

	MSG msg;
	HACCEL hAccel;
  lstrcpy(app_name,"SafeSext_");
	lstrcpyn(app_name+8,profilename,sizeof(app_name)-8);
	
	if (FindWindow(app_name,NULL)) return 0;

	hMainInstance=hInstance;
	hAccel = LoadAccelerators(hMainInstance,MAKEINTRESOURCE(IDR_ACCELERATOR1));

  config_read();

	if (!InitApplication(hMainInstance)) 
	{
		MessageBox(NULL, "Could not initialize application", NULL, MB_OK);
		return (FALSE);
	}

	if (!InitInstance(hMainInstance, SW_SHOWNA)) 
	{
		MessageBox(NULL, "Could not create window", NULL, MB_OK);
		return (FALSE);
	}

	// message loop
	while (GetMessage(&msg,NULL,0,0)) 
	{
		if (!TranslateAccelerator(hwnd_main,hAccel,&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	} // while(GetMessage...
	
	config_write();
	return 0;
} // WinMain

void set_inactive(HWND hwnd)
{
	if (g_active)
	{
		RECT r,vpr;
		GetWindowRect(hwnd,&r);
		SystemParametersInfo(SPI_GETWORKAREA, 0, &vpr, 0);
		{			
			int xm=0,ym=0;
			g_active=0;
      HDC hdc=GetDC(hwnd);
      RECT textr={0,};
      DrawText(hdc,app_name+8,-1,&textr,DT_CENTER|DT_SINGLELINE|DT_CALCRECT);
      ReleaseDC(hwnd,hdc);
			if ((r.top+r.bottom)/2 >= (vpr.bottom+vpr.top)/2) ym=1;
			if ((r.left+r.right)/2 >= (vpr.right+vpr.left)/2) xm=1;

			SetWindowPos(hwnd,0,xm?r.right-(textr.right-textr.left+10):r.left,ym?r.bottom-(textr.bottom-textr.top+5):r.top,(textr.right-textr.left)+10,textr.bottom-textr.top+5,SWP_NOZORDER|SWP_NOACTIVATE);		
			config_w=r.right-r.left;
			config_h=r.bottom-r.top;
			config_x=r.left;
			config_y=r.top;
			ShowWindow(hwnd_rich,SW_HIDE);
		}
	}
  g_active_cnt=0;
}


BOOL InitApplication(HINSTANCE hInstance)
{
	WNDCLASS wc;	
	wc.style = CS_DBLCLKS|CS_VREDRAW|CS_HREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = app_name;
	if (!RegisterClass(&wc)) return FALSE;
	return TRUE;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	int style = 0;
  int exStyle = WS_EX_TOOLWINDOW|(config_aot?WS_EX_TOPMOST:0);
	HWND hwnd;					
	hwnd = CreateWindowEx(exStyle,app_name,app_name+8,style,0,0,1,1,NULL, NULL,hInstance,NULL);
	
	if (!hwnd) return FALSE;
	
	if (nCmdShow ==	SW_SHOWMAXIMIZED || nCmdShow == SW_MAXIMIZE)
		ShowWindow(hwnd,SW_SHOWNORMAL);
	else
		ShowWindow(hwnd,nCmdShow);

	return TRUE;
}
WNDPROC Rich_OldWndProc;

static BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
static void OnDestroy(HWND hwnd);
static void OnClose(HWND hwnd);
static UINT OnNCHitTest(HWND hwnd, int x, int y);
static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
static void OnSize(HWND hwnd, UINT state, int cx, int cy);
static void OnMove(HWND hwnd, int x, int y);
static void OnPaint(HWND hwnd);
static void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
static void OnGetMinMaxInfo(HWND hwnd, LPMINMAXINFO lpMinMaxInfo);

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) 
	{
		HANDLE_MSG(hwnd,WM_CREATE,OnCreate);
		HANDLE_MSG(hwnd,WM_DESTROY,OnDestroy);
		HANDLE_MSG(hwnd,WM_CLOSE,OnClose);
		HANDLE_MSG(hwnd,WM_PAINT,OnPaint);
		HANDLE_MSG(hwnd,WM_MOVE,OnMove);
		HANDLE_MSG(hwnd,WM_SIZE,OnSize);
		HANDLE_MSG(hwnd,WM_COMMAND,OnCommand);
		HANDLE_MSG(hwnd,WM_NCHITTEST,OnNCHitTest);
		HANDLE_MSG(hwnd,WM_LBUTTONDOWN,OnLButtonDown);
    HANDLE_MSG(hwnd,WM_GETMINMAXINFO,OnGetMinMaxInfo);
    case WM_NOTIFY:
    {
      LPNMHDR l=(LPNMHDR)lParam; 
      if (l->hwndFrom == hwnd_rich && l->code == EN_LINK)
      {
         ENLINK *el=(ENLINK*)lParam;
         if(el->msg==WM_LBUTTONUP) 
         {
           char tmp[1024];
           HWND h=l->hwndFrom;
           CHARRANGE c;
           SendMessage(h,EM_EXGETSEL,0,(LPARAM)&c);
           if (c.cpMin == c.cpMax && c.cpMax-c.cpMin < sizeof(tmp)-4)
           {
             TEXTRANGE r;
             r.chrg=el->chrg;
             r.lpstrText=tmp;
             SendMessage(h,EM_GETTEXTRANGE,0,(LPARAM)&r);
             ShellExecute(NULL,"open",tmp,NULL,".",0);
           }
         }
      }
    }
    return 0;
    case WM_ENDSESSION:
      if (wParam)
      {
        OnClose(NULL); // null means dont remove our regkey
      }
    return 0;
    case WM_TIMER:
      if (!g_noclose)
      {
        g_noclose++;
        if (g_active)
        {
          if (--g_active_cnt < 0)
          {
            RECT r;
            POINT p;
            GetWindowRect(hwnd,&r);
            GetCursorPos(&p);
            if (p.x<r.left-32 || p.x>r.right+32 || p.y<r.top-32 || p.y>r.bottom+32)
            {
              if (!(GetAsyncKeyState(MK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(MK_RBUTTON)&0x8000))
              {
		            if (moved)
		            {
			            moved=0;
			            config_write();
		            }
		            write_text();

	              SetWindowLong(hwnd_rich,GWL_WNDPROC,(int)Rich_OldWndProc);
                DestroyWindow(hwnd_rich);
                hwnd_rich=0;
                set_inactive(hwnd);
                if (g_mode==MODE_ONCE) PostMessage(hwnd,WM_CLOSE,0,0);
              }
            }
            else if (g_keyvalid) g_keytimeouttime=GetTickCount()+file_timeout;
          }
        }
        else if (g_keytimeouttime < GetTickCount())
        {
          g_keyvalid=0;
          memset(g_shakey,0,sizeof(g_shakey));
          if (g_mode==MODE_SESSION) PostMessage(hwnd,WM_CLOSE,0,0);
        }
        g_noclose--;
      }
    return 0;
		default: break;
	}
	return (DefWindowProc(hwnd, uMsg, wParam, lParam));

}

LRESULT CALLBACK Rich_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg==WM_CHAR || uMsg == WM_KEYDOWN)
  {
    g_text_dirty=1;
  }
	if (uMsg==WM_RBUTTONUP)
	{
		POINT p;
		GetCursorPos(&p);
    g_noclose++;
    CheckMenuItem(hmenu_main,ID_AOT,config_aot?MF_CHECKED:MF_UNCHECKED);
		TrackPopupMenu(hmenu_main,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd_main,NULL);
    g_noclose--;
	}
	return CallWindowProc(Rich_OldWndProc,hwnd,uMsg,wParam,lParam);
}

static void OnGetMinMaxInfo(HWND hwnd, LPMINMAXINFO lpMinMaxInfo)
{
  lpMinMaxInfo->ptMinTrackSize.x=50;
  lpMinMaxInfo->ptMinTrackSize.y=20;
}

static void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	if (!g_active)
	{
    g_active_cnt=5;
		  g_active=1;
		  SetWindowPos(hwnd,0,config_x,config_y,config_w,config_h,SWP_NOZORDER|SWP_NOACTIVATE);
		  ShowWindow(hwnd_rich,SW_SHOWNA);
		SetForegroundWindow(hwnd_rich);

	  hwnd_rich=CreateWindowEx(WS_EX_CLIENTEDGE,"RichEdit20A","",
		  WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_HSCROLL|WS_VSCROLL,
		  config_border,config_border*3+3,config_w-config_border*2,config_h-config_border*4-3,
		  hwnd, NULL,hMainInstance,NULL);
    SendMessage(hwnd_rich, EM_SETEVENTMASK, 0, ENM_LINK);
    SendMessage(hwnd_rich, EM_AUTOURLDETECT, 1, 0);
	  Rich_OldWndProc = (WNDPROC) SetWindowLong(hwnd_rich,GWL_WNDPROC,(int)Rich_WndProc);
	  SendMessage(hwnd_rich,EM_SETBKGNDCOLOR,FALSE,config_color);
    int a;
    g_noclose++;
	  if ((a=read_text()))
    {
	    SetWindowLong(hwnd_rich,GWL_WNDPROC,(int)Rich_OldWndProc);
      DestroyWindow(hwnd_rich);
      hwnd_rich=0;
      set_inactive(hwnd);      
      if (a==2) 
      {
        SendMessage(hwnd,WM_CLOSE,0,0);
      }
    }
    g_noclose--;
	}
}

static BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
	hwnd_main=hwnd;
	hmenu_main=LoadMenu(hMainInstance,MAKEINTRESOURCE(IDR_MENU1));
	hmenu_main=GetSubMenu(hmenu_main,0);
  {
    HKEY key;
    if (RegCreateKey(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",&key) == ERROR_SUCCESS)
    {
	    RegSetValueEx(key,app_name,0,REG_SZ,(unsigned char *)exe_file,strlen(exe_file)+1);
      RegCloseKey(key);
    }
  }
	SetWindowLong(hwnd,GWL_STYLE,GetWindowLong(hwnd,GWL_STYLE)&~(WS_CAPTION));
	SetWindowPos(hwnd, 0, 0,0, 0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_DRAWFRAME|SWP_NOACTIVATE);
	SetWindowPos(hwnd, 0, config_x, config_y, config_w, config_h, SWP_NOACTIVATE|SWP_NOZORDER);
  SetTimer(hwnd,23,500,NULL);
	g_active=1;
	set_inactive(hwnd_main);
  if (g_mode != MODE_NORMAL)
    OnLButtonDown(hwnd,0,0,0,0);
	return 1;
}

static void OnDestroy(HWND hwnd)
{
}

static void OnClose(HWND hwnd)
{
  HKEY key;
  if (hwnd) if (RegOpenKey(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",&key) == ERROR_SUCCESS) 
  {
    RegDeleteValue(key,app_name);
    RegCloseKey(key);
  }

  if (g_active)
  {
    g_noclose++;
	  write_text();
    g_noclose--;
    SetWindowLong(hwnd_rich,GWL_WNDPROC,(int)Rich_OldWndProc);
    DestroyWindow(hwnd_rich);
    hwnd_rich=0;
    set_inactive(hwnd);
  }

	PostQuitMessage(0);
}


static UINT OnNCHitTest(HWND hwnd, int x, int y)
{
	POINT p;
	RECT r;
	p.x=x;p.y=y;
	GetClientRect(hwnd,&r);
	ScreenToClient(hwnd,&p);
	if (g_active)
	{
		if (p.x <= config_border && p.y <= config_border*5) return HTTOPLEFT;
		if (p.x <= config_border*5 && p.y <= config_border*3+5) return HTTOPLEFT;
		if (p.x >= r.right-config_border && p.y >= r.bottom-config_border*3) return HTBOTTOMRIGHT;
		if (p.x >= r.right-config_border*3 && p.y >= r.bottom-config_border) return HTBOTTOMRIGHT;
		if (p.x >= r.right-config_border && p.y <= config_border*5) return HTTOPRIGHT;
		if (p.x >= r.right-config_border*3 && p.y <= config_border*3+5) return HTTOPRIGHT;
		if (p.x <= config_border && p.y >= r.bottom-config_border*3) return HTBOTTOMLEFT;
		if (p.x <= config_border*3 && p.y >= r.bottom-config_border) return HTBOTTOMLEFT;
		if (p.y <= config_border) return HTTOP;
		if (p.y <= config_border*3+5) return HTCAPTION;
		if (p.x <= config_border) return HTLEFT;
		if (p.y >= r.bottom-config_border) return HTBOTTOM;
		if (p.x >= r.right-config_border) return HTRIGHT;
	}
	return HTCLIENT;
}

BOOL WINAPI TimeoutProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_INITDIALOG)
  {
    SetDlgItemInt(hwndDlg,IDC_EDIT1,file_timeout/1000,FALSE);
    return 1;
  }
  if (uMsg == WM_COMMAND)
  {
    if (LOWORD(wParam) == IDOK)
    {
      BOOL b;
      file_timeout=GetDlgItemInt(hwndDlg,IDC_EDIT1,&b,FALSE)*1000;
      if (!b || file_timeout > 0x10000000) file_timeout=0x10000000;
      g_text_dirty=1;
      g_keytimeouttime=GetTickCount()+file_timeout;
      EndDialog(hwndDlg,0);
    }
    else if (LOWORD(wParam) == IDCANCEL)
    {
      EndDialog(hwndDlg,1);
    }
  }
  if (uMsg == WM_CLOSE)
  {
      EndDialog(hwndDlg,2);
  }
  return 0;
}

BOOL WINAPI PasswdProc2(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_INITDIALOG)
  {
    return 1;
  }
  if (uMsg == WM_COMMAND)
  {
    if (LOWORD(wParam) == IDOK)
    {
      char buf[1024],buf2[1024];
      memset(buf,0,sizeof(buf));
      GetDlgItemText(hwndDlg,IDC_EDIT1,buf,sizeof(buf)-1);
      SHAify s;
      s.add((unsigned char *)buf,strlen(buf));
      memset(buf,0,sizeof(buf));
      unsigned char b[20];
      s.final(b);
      s.reset();
      if (memcmp(g_shakey,b,20))
      {
        MessageBox(hwndDlg,"Invalid old passphrase",APP_NAME,MB_OK);
        return 0;
      }
      GetDlgItemText(hwndDlg,IDC_EDIT2,buf,sizeof(buf)-1);
      GetDlgItemText(hwndDlg,IDC_EDIT3,buf2,sizeof(buf2)-1);
      if (strcmp(buf,buf2))
      {
        MessageBox(hwndDlg,"New passphrases do not match",APP_NAME,MB_OK);
        return 0;
      }
      s.add((unsigned char *)buf,strlen(buf));
      memset(buf,0,sizeof(buf));
      s.final(g_shakey);
      s.reset();
      g_text_dirty=1;
      EndDialog(hwndDlg,0);
    }
    else if (LOWORD(wParam) == IDCANCEL)
    {
      EndDialog(hwndDlg,1);
    }
  }
  if (uMsg == WM_CLOSE)
  {
      EndDialog(hwndDlg,2);
  }
  return 0;
}


static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	int *c=NULL;
	switch (id)
	{
    case ID_CHPASS:
      if (g_keyvalid)
      {
        g_noclose++;
        DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_DIALOG2),hwnd_main,PasswdProc2);
        g_noclose--;
      }
    return;
    case ID_SETTIMEOUTS:
      if (g_keyvalid)
      {
        g_noclose++;
        DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_DIALOG3),hwnd_main,TimeoutProc);
        g_noclose--;
      }
    return;
    case ID_PROFILEMANAGER:
      {
        if (g_active)
        {
		      if (moved)
		      {
			      moved=0;
			      config_write();
		      }
		      write_text();

	        SetWindowLong(hwnd_rich,GWL_WNDPROC,(int)Rich_OldWndProc);
          DestroyWindow(hwnd_rich);
          hwnd_rich=0;
          set_inactive(hwnd);
        }
        char buf[1024];
        GetModuleFileName(hMainInstance,buf,sizeof(buf));
        ShellExecute(hwnd,"open",buf,"",".",SW_SHOW);
      }
    return;
    case ID_AOT:
      {
        config_aot=!config_aot;
        SetWindowPos(hwnd,config_aot?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
      }
    return;
    case IDM_FORGET:
      if (g_active)
      {
		    if (moved)
		    {
			    moved=0;
			    config_write();
		    }
		    write_text();

	      SetWindowLong(hwnd_rich,GWL_WNDPROC,(int)Rich_OldWndProc);
        DestroyWindow(hwnd_rich);
        hwnd_rich=0;
        set_inactive(hwnd);

        g_keyvalid=0;
        memset(g_shakey,0,sizeof(g_shakey));
      }
    return;
		case IDM_ABOUT:
      g_noclose++;
			MessageBox(hwnd,APP_NAME " v0.35.1\n"
				            "Totally based on v0.35 of SafeSex\n"
				            "Copyright (C) 1998-2002, Nullsoft Inc.\n"
                    "Contains Blowfish and SHA-1\n\nSafeSex is a secureish notetaker thing.","About " APP_NAME,MB_OK);
      g_noclose--;
		return;
		case IDM_CLOSE:
			PostQuitMessage(0);
		return;
		case IDM_FONT:
			if (hwnd_rich) 
      {
        g_noclose++;
        g_text_dirty=1;

				LOGFONT lf={0,};
				CHOOSEFONT cf={sizeof(cf),0,0,0,0,
					CF_EFFECTS|CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT,
					0,};
				CHARFORMAT fmt={sizeof(fmt),};
				cf.hwndOwner=hwnd;
				cf.lpLogFont=&lf;
				SendMessage(hwnd_rich,EM_GETCHARFORMAT,1,(LPARAM)&fmt);
				if (fmt.dwMask & CFM_FACE) strcpy(lf.lfFaceName,fmt.szFaceName);
				else lf.lfFaceName[0]=0;
				if (fmt.dwMask & CFM_SIZE) lf.lfHeight=fmt.yHeight/15;
				else lf.lfHeight=0;
				if (fmt.dwMask & CFM_COLOR) cf.rgbColors=fmt.crTextColor;
				else cf.rgbColors=0xffffff;
				lf.lfItalic=(fmt.dwEffects&CFE_ITALIC)?1:0;
				lf.lfWeight=(fmt.dwEffects&CFE_BOLD)?FW_BOLD:FW_NORMAL;
				lf.lfUnderline=(fmt.dwEffects&CFE_UNDERLINE)?1:0;
				lf.lfStrikeOut=(fmt.dwEffects&CFE_STRIKEOUT)?1:0;
				lf.lfCharSet=DEFAULT_CHARSET;
				lf.lfOutPrecision=OUT_DEFAULT_PRECIS;
				lf.lfClipPrecision=CLIP_DEFAULT_PRECIS;
				lf.lfQuality=DEFAULT_QUALITY;
				lf.lfPitchAndFamily=fmt.bPitchAndFamily;
				
				if (ChooseFont(&cf))
				{
					fmt.dwMask=CFM_BOLD|CFM_COLOR|CFM_ITALIC|CFM_STRIKEOUT|CFM_UNDERLINE;
					if (lf.lfFaceName[0]) fmt.dwMask|=CFM_FACE;
					if (lf.lfHeight) fmt.dwMask|=CFM_SIZE;
					fmt.dwEffects=0;
					if (lf.lfItalic) fmt.dwEffects |= CFE_ITALIC;
					if (lf.lfUnderline) fmt.dwEffects |= CFE_UNDERLINE;
					if (lf.lfStrikeOut) fmt.dwEffects |= CFE_STRIKEOUT;
					if (lf.lfWeight!=FW_NORMAL)fmt.dwEffects |= CFE_BOLD;
					fmt.yHeight = cf.iPointSize*2;
					fmt.crTextColor=cf.rgbColors;
					fmt.bPitchAndFamily=lf.lfPitchAndFamily;
					fmt.bCharSet = lf.lfCharSet;
					strcpy(fmt.szFaceName,lf.lfFaceName);
					SendMessage(hwnd_rich,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&fmt);
				}
        g_noclose--;
			}
		return;
		case IDM_BGCOLOR:
			if (!c) c=&config_color;
      if (!hwnd_rich) return;
		case IDM_B_BGC:
			if (!c) c=&config_bcolor1;
		case IDM_B_FGC:
			if (!c) c=&config_bcolor2;
			{
				static COLORREF custcolors[16];
				CHOOSECOLOR cs;
				cs.lStructSize = sizeof(cs);
				cs.hwndOwner = hwnd;
				cs.hInstance = 0;
				cs.rgbResult=*c;
				cs.lpCustColors = custcolors;
				cs.Flags = CC_RGBINIT|CC_FULLOPEN;
        g_noclose++;
				if (ChooseColor(&cs))
				{
					*c=cs.rgbResult;
					config_write();
					SendMessage(hwnd_rich,EM_SETBKGNDCOLOR,FALSE,config_color);
					InvalidateRect(hwnd,NULL,0);
				}
        g_noclose--;
			}
		return;
	}
}

static void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
	moved=1;
	SetWindowPos(hwnd_rich, 0, config_border,config_border*3+3, cx-config_border*2,cy-config_border*4-3, SWP_NOACTIVATE|SWP_NOZORDER);
}

static void OnMove(HWND hwnd, int x, int y)
{
	moved=1;
}

static void OnPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc;
	HBRUSH hBrush;
	HPEN hPen;
  HGDIOBJ hOldPen,hOldBrush;
	RECT r;
	hdc = BeginPaint(hwnd,&ps);
	GetClientRect(hwnd,&r);

	hPen=CreatePen(PS_SOLID,0,config_bcolor2);
	{
		LOGBRUSH lb={BS_SOLID,};
		lb.lbColor=config_bcolor1;
		hBrush=CreateBrushIndirect(&lb); 
	}
	SetTextColor(hdc,config_bcolor2);
	SetBkColor(hdc,config_bcolor1);

	hOldPen=SelectObject(hdc,hPen);
	hOldBrush=SelectObject(hdc,hBrush);
	Rectangle(hdc,r.left,r.top,r.right,r.bottom);
	r.top++;
	DrawText(hdc,app_name+8,-1,&r,DT_CENTER|DT_SINGLELINE|(g_active?DT_TOP:DT_VCENTER));
	SelectObject(hdc,hOldPen);
	SelectObject(hdc,hOldBrush);
	DeleteObject(hPen);
	DeleteObject(hBrush);
	EndPaint(hwnd,&ps);
}


static int _r_i(char *name, int def)
{
	return GetPrivateProfileInt(APP_NAME,name,def,ini_file);
}
#define RI(x) (( x ) = _r_i(#x,( x )))
static void _w_i(char *name, int d)
{
	char str[120];
	wsprintf(str,"%d",d);
	WritePrivateProfileString(APP_NAME,name,str,ini_file);
}
#define WI(x) _w_i(#x,( x ))



void config_read()
{
	char *p;
	GetModuleFileName(hMainInstance,ini_file,sizeof(ini_file));

  exe_file[0]='\"';
	strcpy(exe_file+1,ini_file);
  strcat(exe_file,"\" /PROFILE=");
  strcat(exe_file,profilename);


	p=ini_file+strlen(ini_file);
	while (p >= ini_file && *p != '\\') p--;
  strcpy(++p,profilename);

  strcpy(text_file,ini_file);
  strcat(text_file,".sex");

  strcat(ini_file,".ini");

  RI(config_aot);
	RI(config_x);
	RI(config_y);
	RI(config_w);
	RI(config_h);
	//RI(config_border);
	RI(config_color);
	RI(config_bcolor1);
	RI(config_bcolor2);
}

static HANDLE esFile;

DWORD CALLBACK esCb(DWORD dwCookie, LPBYTE pbBuff, LONG cb, DWORD *pcb)
{
  if (cb > 8) cb &= ~7;
	if (dwCookie == 1) // write
	{
    *pcb=0;

    while (cb > 0)
    {
      DWORD d;
      int l=cb;
      if (l>8)l=8;
      unsigned char buf[8];

      memset(buf,0,8);
      memcpy(buf,pbBuff,l);

      int x;
      for (x = 0; x < 8; x ++) buf[x]^=g_bf_cbc[x];
      Blowfish_Encrypt(&g_bf,(unsigned long *)buf,(unsigned long *)(buf+4));
      for (x = 0; x < 8; x ++) g_bf_cbc[x]^=buf[x];

  		if (!WriteFile(esFile,buf,8,&d,NULL) || d != 8) return 1;

      cb-=l;
      pbBuff+=l;
      *pcb+=l;
    }
	}
	else // read
	{
    char buf[8*128];
    if (cb > sizeof(buf)) cb=sizeof(buf);
    memset(buf,0,sizeof(buf));
		if (!ReadFile(esFile,buf,cb,pcb,NULL)) return 0;

    DWORD a;
    for (a = 0; a < *pcb; a += 8)
    {
      memcpy(pbBuff+a,buf+a,8);
      Blowfish_Decrypt(&g_bf,(unsigned long *)(pbBuff+a),(unsigned long *)(pbBuff+a+4));
      int x;
      for (x = 0; x < 8; x ++)
      {
        pbBuff[a+x] ^= g_bf_cbc[x];
        g_bf_cbc[x]^=buf[a+x];
      }
    }
    while (*pcb > 0 && pbBuff[*pcb-1] == 0) (*pcb)--;
	}
	return 0;
}
 
BOOL WINAPI PasswdProc1_new(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_INITDIALOG)
  {
    SetWindowText(hwndDlg,APP_NAME " Passphrase Set");
    EnableWindow(GetDlgItem(hwndDlg,IDC_EDIT1),0);
    return 1;
  }
  if (uMsg == WM_COMMAND)
  {
    if (LOWORD(wParam) == IDOK)
    {
      char buf[1024];
      char buf2[1024];
      memset(buf,0,sizeof(buf));
      GetDlgItemText(hwndDlg,IDC_EDIT2,buf,sizeof(buf)-1);
      GetDlgItemText(hwndDlg,IDC_EDIT3,buf2,sizeof(buf2)-1);
      if (strcmp(buf,buf2))
      {
        MessageBox(hwndDlg,"New passphrases do not match",APP_NAME,MB_OK);
        return 0;
      }
      SHAify s;
      s.add((unsigned char *)buf,strlen(buf));
      memset(buf,0,sizeof(buf));
      memset(buf2,0,sizeof(buf2));
      s.final(g_shakey);
      s.reset();
      g_keyvalid=1;
      g_keytimeouttime=GetTickCount()+file_timeout;
      EndDialog(hwndDlg,0);
    }
    else if (LOWORD(wParam) == IDCANCEL)
    {
      EndDialog(hwndDlg,1);
    }
  }
  if (uMsg == WM_CLOSE)
  {
      EndDialog(hwndDlg,2);
  }
  return 0;
}

BOOL WINAPI PasswdProc1(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_INITDIALOG)
  {
    return 1;
  }
  if (uMsg == WM_COMMAND)
  {
    if (LOWORD(wParam) == IDOK)
    {
      char buf[1024];
      memset(buf,0,sizeof(buf));
      GetDlgItemText(hwndDlg,IDC_EDIT1,buf,sizeof(buf)-1);
      SHAify s;
      s.add((unsigned char *)buf,strlen(buf));
      memset(buf,0,sizeof(buf));
      s.final(g_shakey);
      s.reset();
      g_keyvalid=1;
      g_keytimeouttime=GetTickCount()+file_timeout;
      EndDialog(hwndDlg,0);
    }
    else if (LOWORD(wParam) == IDCANCEL)
    {
      EndDialog(hwndDlg,1);
    }
  }
  if (uMsg == WM_CLOSE)
  {
      EndDialog(hwndDlg,2);
  }
  return 0;
}

int GetPasswordFromUser(int ispass) // 1 on cancel, 2 on close
{
  if (!ispass)
    return DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_DIALOG2),hwnd_main,PasswdProc1_new);
  return DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_DIALOG1),hwnd_main,PasswdProc1);
}

int read_text()
{
  int reqpass=0;
again:
  CLEAR_BF
  if (!g_keyvalid && reqpass) 
  {
    int a=GetPasswordFromUser(1);
    if (a == 2)
    {
      if (MessageBox(hwnd_main,"Quit " APP_NAME "?",APP_NAME,MB_YESNO)==IDNO) goto again;
    }
    if (a) return a;
  }
  int err=0;
  g_text_dirty=0;
	esFile=CreateFile(text_file,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (esFile!=INVALID_HANDLE_VALUE)
	{
    char buf[16]={0,};
    DWORD d;
    ReadFile(esFile,buf,sizeof(file_sig),&d,NULL);
    if (d < sizeof(file_sig)) err=2;
    else if (memcmp(buf,file_sig,sizeof(file_sig))) err=1;
    else
    {
      int ver=1;
      ReadFile(esFile,buf,2,&d,NULL);
      if (buf[0] == 2) { ver=2; buf[0]=1; }
      if (buf[0] != 1 || buf[1] != 0) err=1;
      else
      {
        if (!ReadFile(esFile,g_file_revision,sizeof(g_file_revision),&d,NULL) || d != sizeof(g_file_revision))
          err=1;
        else
        {
          // see if we can get shit from the file
          Blowfish_Init(&g_bf,g_shakey,sizeof(g_shakey));
          memcpy(g_bf_cbc,g_file_revision,sizeof(g_bf_cbc));
          if (!ReadFile(esFile,buf,ver>=2?16:8,&d,NULL) || d != (DWORD)(ver>=2?16:8))
          {
            err=1;
          }
          else
          {
            char tmp[16];
            memcpy(tmp,buf,16);
            Blowfish_Decrypt(&g_bf,(unsigned long *)buf,(unsigned long *)(buf+4));
            if (ver >= 2)
              Blowfish_Decrypt(&g_bf,(unsigned long *)(buf+8),(unsigned long *)(buf+12));
            int x;
            for (x = 0; x < 8; x ++)
            {
              buf[x]^=g_bf_cbc[x];
              g_bf_cbc[x]^=tmp[x];
            }
            if (memcmp(data_sig,buf,8))
            {
              CloseHandle(esFile);
              g_keyvalid=0;
              if (reqpass) MessageBox(hwnd_main,"Invalid Passphrase",APP_NAME,MB_OK);
              reqpass=1;
              goto again;
            }
            if (ver >= 2)
            {
              for (x = 0; x < 8; x ++)
              {
                buf[x+8]^=g_bf_cbc[x];
                g_bf_cbc[x]^=tmp[x+8];
              }
              memcpy(&file_timeout,buf+8,4);
            }
            else 
              file_timeout=30000;

            g_keytimeouttime=GetTickCount()+file_timeout;

		        EDITSTREAM es;
		        es.dwCookie=0;
		        es.pfnCallback=(EDITSTREAMCALLBACK)esCb;
		        SendMessage(hwnd_rich,EM_STREAMIN,SF_RTF,(LPARAM) &es);
          }
        }
      }
    }
		CloseHandle(esFile);
	}
	else err=2;

  CLEAR_BF

  if (err == 1)
  {
    if (MessageBox(hwnd_main,"Error reading .dat, data may be lost!!!\r\nTry again?", APP_NAME,MB_OKCANCEL) == IDOK) goto again;
    return 1;
  }

  if (err==2)
	{
  again2:
    int a=GetPasswordFromUser(0);
    if (a == 2)
    {
      if (MessageBox(hwnd_main,"Quit " APP_NAME "?",APP_NAME,MB_YESNO)==IDNO) goto again2;
    }
    if (a) return a;

    g_text_dirty=1;
		CHARFORMAT fmt={sizeof(fmt),};
		fmt.dwMask=CFM_COLOR;
		fmt.crTextColor=RGB(255,255,255);
		SendMessage(hwnd_rich,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&fmt);
    memset(g_file_revision,0,sizeof(g_file_revision));
	}
  return 0;
}

void write_text()
{
  int err=0;
  if (!g_text_dirty||!hwnd_rich) return;
  
  CLEAR_BF

  if (!g_keyvalid)
  {
pagain:
    CLEAR_BF
    if (MessageBox(hwnd_main,"Error, passphrase somehow expired, enter your passphrase in the following dialog, Or hit cancel to abort.",APP_NAME " Warning",MB_OKCANCEL|MB_ICONEXCLAMATION) == IDCANCEL)
      return;
    if (GetPasswordFromUser(1)) goto pagain;
  }

again:
	esFile=CreateFile(text_file,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (esFile) 
	{
    int x;
    DWORD l;
    unsigned char ver[2]={2,0};
    for (x = 0; x < 8 && !++g_file_revision[x]; x ++); // increment the version
    if (!WriteFile(esFile,file_sig,sizeof(file_sig),&l,NULL) || l != sizeof(file_sig) ||
        !WriteFile(esFile,ver,sizeof(ver),&l,NULL) || l != sizeof(ver) ||
        !WriteFile(esFile,g_file_revision,sizeof(g_file_revision),&l,NULL) || l != sizeof(g_file_revision)
      ) err=1;
    else
    {
      unsigned char buf[8];
      memcpy(buf,data_sig,8);
      Blowfish_Init(&g_bf,g_shakey,sizeof(g_shakey));
      memcpy(g_bf_cbc,g_file_revision,sizeof(g_bf_cbc));
      DWORD d;
      int x;

      for (x = 0; x < 8; x ++) buf[x]^=g_bf_cbc[x];
      Blowfish_Encrypt(&g_bf,(unsigned long *)buf,(unsigned long *)(buf+4));
      for (x = 0; x < 8; x ++) g_bf_cbc[x]^=buf[x];
      WriteFile(esFile,buf,8,&d,NULL);

      memset(buf,0,8);
      memcpy(buf,&file_timeout,4);
      for (x = 0; x < 8; x ++) buf[x]^=g_bf_cbc[x];
      Blowfish_Encrypt(&g_bf,(unsigned long *)buf,(unsigned long *)(buf+4));
      for (x = 0; x < 8; x ++) g_bf_cbc[x]^=buf[x];
      WriteFile(esFile,buf,8,&d,NULL);

		  EDITSTREAM es;
		  es.dwCookie=1;
		  es.pfnCallback=(EDITSTREAMCALLBACK)esCb;
		  SendMessage(hwnd_rich,EM_STREAMOUT,SF_RTF,(LPARAM) &es);
    }
		CloseHandle(esFile);
	} 
  else 
  {
    err=1;
  }

  CLEAR_BF

  if (err) 
  {
    if (MessageBox(hwnd_main,"Error writing .dat, data will be lost!!!\r\nTry again?", APP_NAME,MB_OKCANCEL) == IDOK) goto again;
  }
}

void config_write()
{
	RECT r;
	if (g_active) 
	{
		GetWindowRect(hwnd_main,&r);
		config_x=r.left;
		config_y=r.top;
		config_w=r.right-r.left;
		config_h=r.bottom-r.top;
	}
	WI(config_x);
	WI(config_y);
	WI(config_w);
	WI(config_h);
  WI(config_aot);
//	WI(config_border);
	WI(config_color);
	WI(config_bcolor1);
	WI(config_bcolor2);
}


////////////////////////////// profile selection dialog //////////////////////////////////////////


static int pep_mode; //copy,rename,create
static char pep_n[128];

static BOOL WINAPI ProfEditProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      SetWindowText(hwndDlg,pep_mode?pep_mode==2?"Create Profile":"Rename Profile":"Copy Profile");
      SetDlgItemText(hwndDlg,IDC_EDIT1,pep_n);
      SetDlgItemText(hwndDlg,IDC_EDIT2,pep_mode != 1 ? "new profile" : pep_n);
    return 0;
    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDOK:
        case IDCANCEL:
          GetDlgItemText(hwndDlg,IDC_EDIT2,pep_n,sizeof(pep_n));
          EndDialog(hwndDlg,LOWORD(wParam)==IDCANCEL);
        return 0;
      }
    return 0;
  }
  return 0;
}

BOOL WINAPI ProfilesProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
  switch (uMsg)
  {
    case WM_INITDIALOG:

      SetWindowText(hwndDlg,APP_NAME " Profile Manager");

      {
        HWND hwndList=GetDlgItem(hwndDlg,IDC_LIST1);
        WIN32_FIND_DATA fd;
        HANDLE h;

        int gotDefault=0;
        char tmp[1024],*p=tmp;
		    GetModuleFileName(hMainInstance,tmp,sizeof(tmp));
		    while (*p) p++;
    		while (p >= tmp && *p != '\\') p--;
        *++p=0;
        strcat(tmp,"*.sex");
        h=FindFirstFile(tmp,&fd);
        if (h)
        {
          do
          {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
              char *p=fd.cFileName;
              while (*p) p++;
              while (p >= fd.cFileName && *p != '.') p--;
              if (p > fd.cFileName)
              {
                *p=0;
                if (strlen(fd.cFileName) < sizeof(profilename))
                {
                  SendMessage(hwndList,LB_ADDSTRING,0,(LPARAM)fd.cFileName);
                  gotDefault=1;
                }
              }
            }
          }
          while (FindNextFile(h,&fd));
          FindClose(h);
        }
        if (!gotDefault) 
        {
          SendMessage(hwndList,LB_ADDSTRING,0,(LPARAM)profilename);
        }
        int l=SendMessage(hwndList,LB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)profilename);
        if (l == LB_ERR) l=0;
        SendMessage(hwndList,LB_SETCURSEL,(WPARAM)l,0);
      }
    return 0;
    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDC_RENAME:
        case IDC_CLONE:
          {
            int l=SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETCURSEL,0,0);
            if (l != LB_ERR)
            {
              pep_mode=LOWORD(wParam) == IDC_RENAME;
              SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETTEXT,(WPARAM)l,(LPARAM)pep_n);
              char oldfn[1024+1024];
              char *p=oldfn;
		          GetModuleFileName(hMainInstance,oldfn,sizeof(oldfn));
		          while (*p) p++;
    		      while (p >= oldfn && *p != '\\') p--;
              *++p=0;
              strcat(oldfn,pep_n);
              strcat(oldfn,".sex");

              if (!DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_PROFILE_MOD),hwndDlg,ProfEditProc) && pep_n[0])
              {
                char tmp[1024+1024];
                strcpy(tmp,"SafeSext_");
                strcat(tmp,pep_n);
	              if (FindWindow(tmp,NULL)) 
                {
                  MessageBox(hwndDlg,"Can't copy or rename profile of that name, currently running",APP_NAME " Error",MB_OK|MB_ICONEXCLAMATION);
                  return 0;
                }
                char *p=tmp;
		            GetModuleFileName(hMainInstance,tmp,sizeof(tmp));
		            while (*p) p++;
    		        while (p >= tmp && *p != '\\') p--;
                *++p=0;
                strcat(tmp,pep_n);
                strcat(tmp,".sex");
                if (stricmp(tmp,oldfn))
                {
                  BOOL ret;
                  if (LOWORD(wParam) == IDC_RENAME) ret=MoveFile(oldfn,tmp);
                  else ret=CopyFile(oldfn,tmp,TRUE);
                  if (ret)
                  {
                    strcpy(tmp+strlen(tmp)-3,"ini");
                    strcpy(oldfn+strlen(oldfn)-3,"ini");

                    if (LOWORD(wParam) == IDC_RENAME) MoveFile(oldfn,tmp);
                    else CopyFile(oldfn,tmp,FALSE);


                    if (LOWORD(wParam) == IDC_RENAME) SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_DELETESTRING,(WPARAM)l,0);

                    l=SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_ADDSTRING,0,(WPARAM)pep_n);
                    SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_SETCURSEL,(WPARAM)l,0);
                  }
                  else MessageBox(hwndDlg,LOWORD(wParam) == IDC_RENAME ? "Error renaming profile" : "Error copying profile",APP_NAME " Error",MB_OK|MB_ICONEXCLAMATION);
                }
              }
            }
          }
        return 0;
        case IDC_CREATE:
          {
            pep_mode=2;
            strcpy(pep_n,"NULL");
            if (!DialogBox(hMainInstance,MAKEINTRESOURCE(IDD_PROFILE_MOD),hwndDlg,ProfEditProc) && pep_n[0])
            {
              char tmp[1024+1024];
              strcpy(tmp,"SafeSext_");
              strcat(tmp,pep_n);
	            if (FindWindow(tmp,NULL)) 
              {
                MessageBox(hwndDlg,"Can't create profile of that name, already running",APP_NAME " Error",MB_OK|MB_ICONEXCLAMATION);
                return 0;
              }

              char *p=tmp;
		          GetModuleFileName(hMainInstance,tmp,sizeof(tmp));
		          while (*p) p++;
    		      while (p >= tmp && *p != '\\') p--;
              *++p=0;
              strcat(tmp,pep_n);
              strcat(tmp,".sex");
              HANDLE h=CreateFile(tmp,0,0,NULL,CREATE_NEW,0,NULL);
              if (h != INVALID_HANDLE_VALUE)
              {
                CloseHandle(h);
                int l=SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_ADDSTRING,0,(WPARAM)pep_n);
                SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_SETCURSEL,(WPARAM)l,0);
              }
              else MessageBox(hwndDlg,"Error creating profile",APP_NAME " Error",MB_OK|MB_ICONEXCLAMATION);
            }
          }
        return 0;
        case IDC_DELETE:
          {
            int l=SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETCURSEL,0,0);
            if (l != LB_ERR)
            {
              char tmp[1024+1024];
              strcpy(tmp,"SafeSext_");
              SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETTEXT,(WPARAM)l,(LPARAM)(tmp+strlen(tmp)));
	            if (FindWindow(tmp,NULL)) 
              {
                MessageBox(hwndDlg,"Can't delete profile of that name, currently running",APP_NAME " Error",MB_OK|MB_ICONEXCLAMATION);
                return 0;
              }
              char *p=tmp;
		          GetModuleFileName(hMainInstance,tmp,sizeof(tmp));
		          while (*p) p++;
    		      while (p >= tmp && *p != '\\') p--;
              *++p=0;
              SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETTEXT,(WPARAM)l,(LPARAM)(tmp+strlen(tmp)));
              strcat(tmp,".sex");
              if (DeleteFile(tmp))
              {
                strcpy(tmp+strlen(tmp)-3,"ini");
                DeleteFile(tmp);
                SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_DELETESTRING,l,0);
              }
              else MessageBox(hwndDlg,"Error deleting profile",APP_NAME " Error",MB_OK|MB_ICONEXCLAMATION);              
            }
          }
        return 0;
        case IDC_LIST1:
          if (HIWORD(wParam) != LBN_DBLCLK) return 0;
        case IDOK:
          {
            int l=SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETCURSEL,0,0);
            if (l != LB_ERR)
            {
              SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETTEXT,(WPARAM)l,(LPARAM)profilename);
              EndDialog(hwndDlg,1); 
            }
          }
        return 0;
        case IDCANCEL: 
          {
            int l=SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETCURSEL,0,0);
            if (l != LB_ERR)
            {
              SendDlgItemMessage(hwndDlg,IDC_LIST1,LB_GETTEXT,(WPARAM)l,(LPARAM)profilename);
            }
          }
          EndDialog(hwndDlg,0); 
        return 0;
      }
    return 0;
  }

  return 0;
}
