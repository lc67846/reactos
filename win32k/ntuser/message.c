/*
 *  ReactOS W32 Subsystem
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* $Id$
 *
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS kernel
 * PURPOSE:          Messages
 * FILE:             subsys/win32k/ntuser/message.c
 * PROGRAMER:        Casper S. Hornstrup (chorns@users.sourceforge.net)
 * REVISION HISTORY:
 *       06-06-2001  CSH  Created
 */

/* INCLUDES ******************************************************************/

#include <w32k.h>

#define NDEBUG
#include <debug.h>

typedef struct
{
   UINT uFlags;
   UINT uTimeout;
   ULONG_PTR Result;
}
DOSENDMESSAGE, *PDOSENDMESSAGE;

/* FUNCTIONS *****************************************************************/

NTSTATUS FASTCALL
IntInitMessageImpl(VOID)
{
   return STATUS_SUCCESS;
}

NTSTATUS FASTCALL
IntCleanupMessageImpl(VOID)
{
   return STATUS_SUCCESS;
}

#define MMS_SIZE_WPARAM      -1
#define MMS_SIZE_WPARAMWCHAR -2
#define MMS_SIZE_LPARAMSZ    -3
#define MMS_SIZE_SPECIAL     -4
#define MMS_FLAG_READ        0x01
#define MMS_FLAG_WRITE       0x02
#define MMS_FLAG_READWRITE   (MMS_FLAG_READ | MMS_FLAG_WRITE)
typedef struct tagMSGMEMORY
{
   UINT Message;
   UINT Size;
   INT Flags;
}
MSGMEMORY, *PMSGMEMORY;

static MSGMEMORY MsgMemory[] =
   {
      { WM_CREATE, MMS_SIZE_SPECIAL, MMS_FLAG_READWRITE },
      { WM_DDE_ACK, sizeof(KMDDELPARAM), MMS_FLAG_READ },
      { WM_DDE_EXECUTE, MMS_SIZE_WPARAM, MMS_FLAG_READ },
      { WM_GETMINMAXINFO, sizeof(MINMAXINFO), MMS_FLAG_READWRITE },
      { WM_GETTEXT, MMS_SIZE_WPARAMWCHAR, MMS_FLAG_WRITE },
      { WM_NCCALCSIZE, MMS_SIZE_SPECIAL, MMS_FLAG_READWRITE },
      { WM_NCCREATE, MMS_SIZE_SPECIAL, MMS_FLAG_READWRITE },
      { WM_SETTEXT, MMS_SIZE_LPARAMSZ, MMS_FLAG_READ },
      { WM_STYLECHANGED, sizeof(STYLESTRUCT), MMS_FLAG_READ },
      { WM_STYLECHANGING, sizeof(STYLESTRUCT), MMS_FLAG_READWRITE },
      { WM_COPYDATA, MMS_SIZE_SPECIAL, MMS_FLAG_READ },
      { WM_WINDOWPOSCHANGED, sizeof(WINDOWPOS), MMS_FLAG_READ },
      { WM_WINDOWPOSCHANGING, sizeof(WINDOWPOS), MMS_FLAG_READWRITE },
   };

static PMSGMEMORY FASTCALL
FindMsgMemory(UINT Msg)
{
   PMSGMEMORY MsgMemoryEntry;

   /* See if this message type is present in the table */
   for (MsgMemoryEntry = MsgMemory;
         MsgMemoryEntry < MsgMemory + (sizeof(MsgMemory) / sizeof(MSGMEMORY));
         MsgMemoryEntry++)
   {
      if (Msg == MsgMemoryEntry->Message)
      {
         return MsgMemoryEntry;
      }
   }

   return NULL;
}

static UINT FASTCALL
MsgMemorySize(PMSGMEMORY MsgMemoryEntry, WPARAM wParam, LPARAM lParam)
{
   CREATESTRUCTW *Cs;
   PUNICODE_STRING WindowName;
   PUNICODE_STRING ClassName;
   UINT Size;

   _SEH_TRY {
      if (MMS_SIZE_WPARAM == MsgMemoryEntry->Size)
   {
      return (UINT) wParam;
      }
      else if (MMS_SIZE_WPARAMWCHAR == MsgMemoryEntry->Size)
   {
      return (UINT) (wParam * sizeof(WCHAR));
      }
      else if (MMS_SIZE_LPARAMSZ == MsgMemoryEntry->Size)
   {
      return (UINT) ((wcslen((PWSTR) lParam) + 1) * sizeof(WCHAR));
      }
      else if (MMS_SIZE_SPECIAL == MsgMemoryEntry->Size)
   {
      switch(MsgMemoryEntry->Message)
         {
            case WM_CREATE:
            case WM_NCCREATE:
               Cs = (CREATESTRUCTW *) lParam;
               WindowName = (PUNICODE_STRING) Cs->lpszName;
               ClassName = (PUNICODE_STRING) Cs->lpszClass;
               Size = sizeof(CREATESTRUCTW) + WindowName->Length + sizeof(WCHAR);
               if (IS_ATOM(ClassName->Buffer))
               {
                  Size += sizeof(WCHAR) + sizeof(ATOM);
               }
               else
               {
                  Size += sizeof(WCHAR) + ClassName->Length + sizeof(WCHAR);
               }
               return Size;
               break;

            case WM_NCCALCSIZE:
               return wParam ? sizeof(NCCALCSIZE_PARAMS) + sizeof(WINDOWPOS) : sizeof(RECT);
               break;

            case WM_COPYDATA:
               return sizeof(COPYDATASTRUCT) + ((PCOPYDATASTRUCT)lParam)->cbData;

            default:
               assert(FALSE);
               return 0;
               break;
         }
      }
      else
      {
         return MsgMemoryEntry->Size;
      }
   } _SEH_HANDLE {

      DPRINT1("Exception caught in MsgMemorySize()! Status: 0x%x\n", _SEH_GetExceptionCode());
   } _SEH_END;
   return 0;
}

static FASTCALL NTSTATUS
PackParam(LPARAM *lParamPacked, UINT Msg, WPARAM wParam, LPARAM lParam)
{
   NCCALCSIZE_PARAMS *UnpackedNcCalcsize;
   NCCALCSIZE_PARAMS *PackedNcCalcsize;
   CREATESTRUCTW *UnpackedCs;
   CREATESTRUCTW *PackedCs;
   PUNICODE_STRING WindowName;
   PUNICODE_STRING ClassName;
   UINT Size;
   PCHAR CsData;

   *lParamPacked = lParam;
   if (WM_NCCALCSIZE == Msg && wParam)
   {
      UnpackedNcCalcsize = (NCCALCSIZE_PARAMS *) lParam;
      if (UnpackedNcCalcsize->lppos != (PWINDOWPOS) (UnpackedNcCalcsize + 1))
      {
         PackedNcCalcsize = ExAllocatePoolWithTag(PagedPool,
                            sizeof(NCCALCSIZE_PARAMS) + sizeof(WINDOWPOS),
                            TAG_MSG);
         if (NULL == PackedNcCalcsize)
         {
            DPRINT1("Not enough memory to pack lParam\n");
            return STATUS_NO_MEMORY;
         }
         RtlCopyMemory(PackedNcCalcsize, UnpackedNcCalcsize, sizeof(NCCALCSIZE_PARAMS));
         PackedNcCalcsize->lppos = (PWINDOWPOS) (PackedNcCalcsize + 1);
         RtlCopyMemory(PackedNcCalcsize->lppos, UnpackedNcCalcsize->lppos, sizeof(WINDOWPOS));
         *lParamPacked = (LPARAM) PackedNcCalcsize;
      }
   }
   else if (WM_CREATE == Msg || WM_NCCREATE == Msg)
   {
      UnpackedCs = (CREATESTRUCTW *) lParam;
      WindowName = (PUNICODE_STRING) UnpackedCs->lpszName;
      ClassName = (PUNICODE_STRING) UnpackedCs->lpszClass;
      Size = sizeof(CREATESTRUCTW) + WindowName->Length + sizeof(WCHAR);
      if (IS_ATOM(ClassName->Buffer))
      {
         Size += sizeof(WCHAR) + sizeof(ATOM);
      }
      else
      {
         Size += sizeof(WCHAR) + ClassName->Length + sizeof(WCHAR);
      }
      PackedCs = ExAllocatePoolWithTag(PagedPool, Size, TAG_MSG);
      if (NULL == PackedCs)
      {
         DPRINT1("Not enough memory to pack lParam\n");
         return STATUS_NO_MEMORY;
      }
      RtlCopyMemory(PackedCs, UnpackedCs, sizeof(CREATESTRUCTW));
      CsData = (PCHAR) (PackedCs + 1);
      PackedCs->lpszName = (LPCWSTR) (CsData - (PCHAR) PackedCs);
      RtlCopyMemory(CsData, WindowName->Buffer, WindowName->Length);
      CsData += WindowName->Length;
      *((WCHAR *) CsData) = L'\0';
      CsData += sizeof(WCHAR);
      PackedCs->lpszClass = (LPCWSTR) (CsData - (PCHAR) PackedCs);
      if (IS_ATOM(ClassName->Buffer))
      {
         *((WCHAR *) CsData) = L'A';
         CsData += sizeof(WCHAR);
         *((ATOM *) CsData) = (ATOM)(DWORD_PTR) ClassName->Buffer;
         CsData += sizeof(ATOM);
      }
      else
      {
         *((WCHAR *) CsData) = L'S';
         CsData += sizeof(WCHAR);
         RtlCopyMemory(CsData, ClassName->Buffer, ClassName->Length);
         CsData += ClassName->Length;
         *((WCHAR *) CsData) = L'\0';
         CsData += sizeof(WCHAR);
      }
      ASSERT(CsData == (PCHAR) PackedCs + Size);
      *lParamPacked = (LPARAM) PackedCs;
   }

   return STATUS_SUCCESS;
}

static FASTCALL NTSTATUS
UnpackParam(LPARAM lParamPacked, UINT Msg, WPARAM wParam, LPARAM lParam)
{
   NCCALCSIZE_PARAMS *UnpackedParams;
   NCCALCSIZE_PARAMS *PackedParams;
   PWINDOWPOS UnpackedWindowPos;

   if (lParamPacked == lParam)
   {
      return STATUS_SUCCESS;
   }

   if (WM_NCCALCSIZE == Msg && wParam)
   {
      PackedParams = (NCCALCSIZE_PARAMS *) lParamPacked;
      UnpackedParams = (NCCALCSIZE_PARAMS *) lParam;
      UnpackedWindowPos = UnpackedParams->lppos;
      RtlCopyMemory(UnpackedParams, PackedParams, sizeof(NCCALCSIZE_PARAMS));
      UnpackedParams->lppos = UnpackedWindowPos;
      RtlCopyMemory(UnpackedWindowPos, PackedParams + 1, sizeof(WINDOWPOS));
      ExFreePool((PVOID) lParamPacked);

      return STATUS_SUCCESS;
   }
   else if (WM_CREATE == Msg || WM_NCCREATE == Msg)
   {
      ExFreePool((PVOID) lParamPacked);

      return STATUS_SUCCESS;
   }

   ASSERT(FALSE);

   return STATUS_INVALID_PARAMETER;
}

BOOL
STDCALL
NtUserCallMsgFilter(
   LPMSG msg,
   INT code)
{
   DECLARE_RETURN(BOOL);

   DPRINT("Enter NtUserCallMsgFilter\n");

   UserEnterExclusive();

   if (coHOOK_CallHooks( WH_SYSMSGFILTER, code, 0, (LPARAM)msg))
      RETURN(TRUE);

   RETURN(coHOOK_CallHooks( WH_MSGFILTER, code, 0, (LPARAM)msg));

CLEANUP:
   DPRINT("Leave NtUserCallMsgFilter. ret=%i\n", _ret_);
   UserLeave();
   END_CLEANUP;
}

LRESULT STDCALL
NtUserDispatchMessage(PNTUSERDISPATCHMESSAGEINFO UnsafeMsgInfo)
{
   NTSTATUS Status;
   NTUSERDISPATCHMESSAGEINFO MsgInfo;
   PWINDOW_OBJECT WindowObject;
   LRESULT Result = TRUE;
   DECLARE_RETURN(LRESULT);

   DPRINT("Enter NtUserDispatchMessage\n");
   UserEnterExclusive();

   Status = MmCopyFromCaller(&MsgInfo, UnsafeMsgInfo, sizeof(NTUSERDISPATCHMESSAGEINFO));
   if (! NT_SUCCESS(Status))
   {
      SetLastNtError(Status);
      RETURN(0);
   }

   /* Process timer messages.  FIXME: systimers??? */
   if (WM_TIMER == MsgInfo.Msg.message && 0 != MsgInfo.Msg.lParam)
   {
      LARGE_INTEGER LargeTickCount;
      /* FIXME: Call hooks. */

      /* FIXME: Check for continuing validity of timer. */

      MsgInfo.HandledByKernel = FALSE;
      KeQueryTickCount(&LargeTickCount);
      MsgInfo.Proc = (WNDPROC) MsgInfo.Msg.lParam;
      MsgInfo.Msg.lParam = (LPARAM)LargeTickCount.u.LowPart;
   }
   else if (NULL == MsgInfo.Msg.hwnd)
   {
      MsgInfo.HandledByKernel = TRUE;
      Result = 0;
   }
   else
   {
      /* Get the window object. */
      WindowObject = IntGetWindowObject(MsgInfo.Msg.hwnd);
      if (NULL == WindowObject)
      {
         SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
         MsgInfo.HandledByKernel = TRUE;
         Result = 0;
      }
      else
      {
         if (WindowObject->Queue != UserGetCurrentQueue())
         {
            DPRINT1("Window doesn't belong to the calling thread!\n");
            MsgInfo.HandledByKernel = TRUE;
            Result = 0;
         }
         else
         {
            /* FIXME: Call hook procedures. */

            MsgInfo.HandledByKernel = FALSE;
            Result = 0;
            if (0xFFFF0000 != ((DWORD) WindowObject->WndProcW & 0xFFFF0000))
            {
               if (0xFFFF0000 != ((DWORD) WindowObject->WndProcA & 0xFFFF0000))
               {
                  /* Both Unicode and Ansi winprocs are real, use whatever
                     usermode prefers */
                  MsgInfo.Proc = (MsgInfo.Ansi ? WindowObject->WndProcA
                                  : WindowObject->WndProcW);
               }
               else
               {
                  /* Real Unicode winproc */
                  MsgInfo.Ansi = FALSE;
                  MsgInfo.Proc = WindowObject->WndProcW;
               }
            }
            else
            {
               /* Must have real Ansi winproc */
               MsgInfo.Ansi = TRUE;
               MsgInfo.Proc = WindowObject->WndProcA;
            }
         }
      }
   }
   Status = MmCopyToCaller(UnsafeMsgInfo, &MsgInfo, sizeof(NTUSERDISPATCHMESSAGEINFO));
   if (! NT_SUCCESS(Status))
   {
      SetLastNtError(Status);
      RETURN(0);
   }

   RETURN(Result);

CLEANUP:
   DPRINT("Leave NtUserDispatchMessage. ret=%i\n", _ret_);
   UserLeave();
   END_CLEANUP;
}


BOOL STDCALL
NtUserTranslateMessage(LPMSG lpMsg,
                       HKL dwhkl)
{
   NTSTATUS Status;
   MSG SafeMsg;
   DECLARE_RETURN(BOOL);

   DPRINT("Enter NtUserTranslateMessage\n");
   UserEnterExclusive();

   Status = MmCopyFromCaller(&SafeMsg, lpMsg, sizeof(MSG));
   if(!NT_SUCCESS(Status))
   {
      SetLastNtError(Status);
      RETURN(FALSE);
   }

   RETURN(IntTranslateKbdMessage(&SafeMsg, dwhkl));

CLEANUP:
   DPRINT("Leave NtUserTranslateMessage: ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}


VOID FASTCALL
coUserSendHitTestMessages(PUSER_MESSAGE_QUEUE Queue, LPMSG Msg)
{
   if(!Msg->hwnd || Queue->Input->hCaptureWindow)
   {
      return;
   }

   switch(Msg->message)
   {
      case WM_MOUSEMOVE:
         {
            coUserSendMessage(Msg->hwnd, WM_SETCURSOR, (WPARAM)Msg->hwnd, MAKELPARAM(HTCLIENT, Msg->message));
            break;
         }
      case WM_NCMOUSEMOVE:
         {
            coUserSendMessage(Msg->hwnd, WM_SETCURSOR, (WPARAM)Msg->hwnd, MAKELPARAM(Msg->wParam, Msg->message));
            break;
         }
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_XBUTTONDOWN:
      case WM_LBUTTONDBLCLK:
      case WM_MBUTTONDBLCLK:
      case WM_RBUTTONDBLCLK:
      case WM_XBUTTONDBLCLK:
         {
            WPARAM wParam;
            PSYSTEM_CURSORINFO CurInfo;

            if(!IntGetWindowStationObject(gInteractiveWinSta))
            {
               break;
            }
            CurInfo = UserGetSysCursorInfo(gInteractiveWinSta);
            wParam = (WPARAM)(CurInfo->ButtonsDown);
            ObDereferenceObject(gInteractiveWinSta);

            coUserSendMessage(Msg->hwnd, WM_MOUSEMOVE, wParam, Msg->lParam);
            coUserSendMessage(Msg->hwnd, WM_SETCURSOR, (WPARAM)Msg->hwnd, MAKELPARAM(HTCLIENT, Msg->message));
            break;
         }
      case WM_NCLBUTTONDOWN:
      case WM_NCMBUTTONDOWN:
      case WM_NCRBUTTONDOWN:
      case WM_NCXBUTTONDOWN:
      case WM_NCLBUTTONDBLCLK:
      case WM_NCMBUTTONDBLCLK:
      case WM_NCRBUTTONDBLCLK:
      case WM_NCXBUTTONDBLCLK:
         {
            coUserSendMessage(Msg->hwnd, WM_NCMOUSEMOVE, (WPARAM)Msg->wParam, Msg->lParam);
            coUserSendMessage(Msg->hwnd, WM_SETCURSOR, (WPARAM)Msg->hwnd, MAKELPARAM(Msg->wParam, Msg->message));
            break;
         }
   }
}

BOOL FASTCALL
coUserActivateWindowMouse(PUSER_MESSAGE_QUEUE ThreadQueue, LPMSG Msg, PWINDOW_OBJECT MsgWindow,
                       USHORT *HitTest)
{
   ULONG Result;

   if(*HitTest == (USHORT)HTTRANSPARENT)
   {
      /* eat the message, search again! */
      return TRUE;
   }

   Result = coUserSendMessage(MsgWindow->hSelf, WM_MOUSEACTIVATE, (WPARAM)UserGetParent(MsgWindow), (LPARAM)MAKELONG(*HitTest, Msg->message));
   switch (Result)
   {
      case MA_NOACTIVATEANDEAT:
         return TRUE;
      case MA_NOACTIVATE:
         break;
      case MA_ACTIVATEANDEAT:
         coUserMouseActivateWindow(MsgWindow);
         return TRUE;
      default:
         /* MA_ACTIVATE */
         coUserMouseActivateWindow(MsgWindow);
         break;
   }

   return FALSE;
}

static BOOL FASTCALL
coUserTranslateMouseMessage(PUSER_MESSAGE_QUEUE Queue, LPMSG Msg, USHORT *HitTest, BOOL Remove)
{
   PWINDOW_OBJECT Window;

   if(!(Window = IntGetWindowObject(Msg->hwnd)))
   {
      /* let's just eat the message?! */
      return TRUE;
   }

   if(Queue == Window->Queue && Queue->Input->hCaptureWindow != Window->hSelf)
   {
      /* only send WM_NCHITTEST messages if we're not capturing the window! */
      *HitTest = coUserSendMessage(Window->hSelf, WM_NCHITTEST, 0,
                                MAKELONG(Msg->pt.x, Msg->pt.y));

      if(*HitTest == (USHORT)HTTRANSPARENT)
      {
         PWINDOW_OBJECT DesktopWindow = UserGetDesktopWindow();
         //      HWND hDesktop = IntGetDesktopWindow();

         if(DesktopWindow)
         {
            PWINDOW_OBJECT Wnd;

            coWinPosWindowFromPoint(DesktopWindow, Window->Queue, &Msg->pt, &Wnd);
            if(Wnd)
            {
               if(Wnd != Window)
               {
                  /* post the message to the other window */
                  Msg->hwnd = Wnd->hSelf;
                  if(!(Wnd->hdr.flags & USER_OBJ_DESTROYING))
                  {
                     MsqPostMessage(Wnd->Queue, Msg, FALSE,
                                    Msg->message == WM_MOUSEMOVE ? QS_MOUSEMOVE :
                                    QS_MOUSEBUTTON);
                  }

                  /* eat the message */
                  return TRUE;
               }
            }
         }
      }
   }
   else
   {
      *HitTest = HTCLIENT;
   }

   if(IS_BTN_MESSAGE(Msg->message, DOWN))
   {
      /* generate double click messages, if necessary */
      if ((((*HitTest) != HTCLIENT) ||
            (UserGetClassLong(Window, GCL_STYLE, FALSE) & CS_DBLCLKS)) &&
            MsqIsDblClk(Msg, Remove))
      {
         Msg->message += WM_LBUTTONDBLCLK - WM_LBUTTONDOWN;
      }
   }

   if(Msg->message != WM_MOUSEWHEEL)
   {

      if ((*HitTest) != HTCLIENT)
      {
         Msg->message += WM_NCMOUSEMOVE - WM_MOUSEMOVE;
         if((Msg->message == WM_NCRBUTTONUP) &&
               (((*HitTest) == HTCAPTION) || ((*HitTest) == HTSYSMENU)))
         {
            Msg->message = WM_CONTEXTMENU;
            Msg->wParam = (WPARAM)Window->hSelf;
         }
         else
         {
            Msg->wParam = *HitTest;
         }
         Msg->lParam = MAKELONG(Msg->pt.x, Msg->pt.y);
      }
      else if(Queue->Input->hMoveSize == NULL &&
              Queue->Input->hMenuOwner == NULL)
      {
         /* NOTE: Msg->pt should remain in screen coordinates. -- FiN */
         Msg->lParam = MAKELONG(
                          Msg->pt.x - (WORD)Window->ClientRect.left,
                          Msg->pt.y - (WORD)Window->ClientRect.top);
      }
   }

   return FALSE;
}


/*
 * Internal version of PeekMessage() doing all the work
 */
BOOL FASTCALL
coUserPeekMessage(PUSER_MESSAGE Msg,
               HWND hWnd,
               UINT MsgFilterMin,
               UINT MsgFilterMax,
               UINT RemoveMsg)
{
   LARGE_INTEGER LargeTickCount;
   PUSER_MESSAGE_QUEUE Queue;
   PUSER_MESSAGE Message;
   BOOL Present, RemoveMessages;

   /* The queues and order in which they are checked are documented in the MSDN
      article on GetMessage() */

   Queue = UserGetCurrentQueue();

   if (!Queue)
   {
      return FALSE;
   }

   /* Inspect RemoveMsg flags */
   /* FIXME: The only flag we process is PM_REMOVE - processing of others must still be implemented */
   RemoveMessages = RemoveMsg & PM_REMOVE;

CheckMessages:

   Present = FALSE;

   KeQueryTickCount(&LargeTickCount);
   Queue->LastMsgRead = LargeTickCount.u.LowPart;

   /* Dispatch sent messages here. */
   while ( coMsqDispatchOneSentMessage(Queue))      ;

   /* clear changed bits so we can wait on them if we don't find a message */
   Queue->ChangesBits = 0;

   /* Now look for a quit message. */

   if (Queue->QuitPosted)
   {
      /* According to the PSDK, WM_QUIT messages are always returned, regardless
         of the filter specified */
      Msg->Msg.hwnd = NULL;
      Msg->Msg.message = WM_QUIT;
      Msg->Msg.wParam = Queue->QuitExitCode;
      Msg->Msg.lParam = 0;
      Msg->FreeLParam = FALSE;
      if (RemoveMessages)
      {
         Queue->QuitPosted = FALSE;
      }
      return TRUE;
   }

   /* Now check for normal messages. */
   Present = coMsqFindMessage(Queue,
                            FALSE,
                            RemoveMessages,
                            hWnd,
                            MsgFilterMin,
                            MsgFilterMax,
                            &Message);
   if (Present)
   {

      RtlCopyMemory(Msg, Message, sizeof(USER_MESSAGE));

      if (RemoveMessages)
      {


         MsqDestroyMessage(Message);


      }


      goto MessageFound;
   }

   /* Check for hardware events. */
   Present = coMsqFindMessage(Queue,
                            TRUE,
                            RemoveMessages,
                            hWnd,
                            MsgFilterMin,
                            MsgFilterMax,
                            &Message);

   if (Present)
   {


      RtlCopyMemory(Msg, Message, sizeof(USER_MESSAGE));


      if (RemoveMessages)
      {

         MsqDestroyMessage(Message);

      }

      goto MessageFound;
   }

   /* Check for sent messages again. */
   while (coMsqDispatchOneSentMessage(Queue));

   /* Check for paint messages. */
   if (IntGetPaintMessage(hWnd, MsgFilterMin, MsgFilterMax, QUEUE_2_WTHREAD(Queue), &Msg->Msg, RemoveMessages))
   {

      Msg->FreeLParam = FALSE;
      return TRUE;
   }

   /* Check for WM_(SYS)TIMER messages */
   Present = MsqGetTimerMessage(Queue, hWnd, MsgFilterMin, MsgFilterMax,
                                &Msg->Msg, RemoveMessages);

   if (Present)
   {

      Msg->FreeLParam = FALSE;
      goto MessageFound;
   }

   if(Present)
   {
MessageFound:

      if(RemoveMessages)
      {
         PWINDOW_OBJECT MsgWindow = NULL;

         if(Msg->Msg.hwnd &&
            Msg->Msg.message >= WM_MOUSEFIRST && Msg->Msg.message <= WM_MOUSELAST)
         {
            USHORT HitTest;

            MsgWindow = IntGetWindowObject(Msg->Msg.hwnd);
            ASSERT(MsgWindow != NULL);
            if(coUserTranslateMouseMessage(Queue, &Msg->Msg, &HitTest, TRUE))
               /* FIXME - check message filter again, if the message doesn't match anymore,
                          search again */
            {
               /* eat the message, search again */
               goto CheckMessages;
            }
            if(Queue->Input->hCaptureWindow == NULL)
            {

               coUserSendHitTestMessages(Queue, &Msg->Msg);
               if((Msg->Msg.message != WM_MOUSEMOVE && Msg->Msg.message != WM_NCMOUSEMOVE) &&
                     IS_BTN_MESSAGE(Msg->Msg.message, DOWN) &&
                     coUserActivateWindowMouse(Queue, &Msg->Msg, MsgWindow, &HitTest))
               {

                  /* eat the message, search again */
                  goto CheckMessages;
               }
            }
         }
         else
         {
            coUserSendHitTestMessages(Queue, &Msg->Msg);
         }

         return TRUE;
      }

      USHORT HitTest;
      if((Msg->Msg.hwnd && Msg->Msg.message >= WM_MOUSEFIRST && Msg->Msg.message <= WM_MOUSELAST) &&
            coUserTranslateMouseMessage(Queue, &Msg->Msg, &HitTest, FALSE))
         /* FIXME - check message filter again, if the message doesn't match anymore,
                    search again */
      {
         /* eat the message, search again */
         goto CheckMessages;
      }

      return TRUE;
   }

   return Present;
}

BOOL STDCALL
NtUserPeekMessage(
   PNTUSERGETMESSAGEINFO UnsafeInfo,
   HWND Wnd,
   UINT MsgFilterMin,
   UINT MsgFilterMax,
   UINT RemoveMsg //PM_NOREMOVE|PM_REMOVE|PM_NOYIELD AND PM_QS_INPUT etc.
)
{
   NTSTATUS Status;
   BOOL Present;
   NTUSERGETMESSAGEINFO Info;
   PWINDOW_OBJECT Window;
   PMSGMEMORY MsgMemoryEntry;
   PVOID UserMem;
   UINT Size;
   USER_MESSAGE Msg;
   DECLARE_RETURN(BOOL);

   DPRINT("Enter NtUserPeekMessage\n");
   UserEnterExclusive();

   if (!PsGetWin32Thread()){
      KEBUGCHECK(0);
   }

   /* Validate input */
   if (NULL != Wnd)
   {
      Window = IntGetWindowObject(Wnd);
      if (NULL == Window)
      {
         Wnd = NULL;
      }
   }

   if (MsgFilterMax < MsgFilterMin)
   {
      MsgFilterMin = 0;
      MsgFilterMax = 0;
   }

   Present = coUserPeekMessage(&Msg, Wnd, MsgFilterMin, MsgFilterMax, RemoveMsg);

   if (Present)
   {
      Info.Msg = Msg.Msg;
      /* See if this message type is present in the table */
      MsgMemoryEntry = FindMsgMemory(Info.Msg.message);
      if (NULL == MsgMemoryEntry)
      {
         /* Not present, no copying needed */
         Info.LParamSize = 0;
      }
      else
      {
         /* Determine required size */
         Size = MsgMemorySize(MsgMemoryEntry, Info.Msg.wParam,
                              Info.Msg.lParam);

         /* Allocate required amount of user-mode memory */
         Info.LParamSize = Size;
         UserMem = NULL;
         Status = ZwAllocateVirtualMemory(NtCurrentProcess(), &UserMem, 0,
                                          &Info.LParamSize, MEM_COMMIT, PAGE_READWRITE);
         if (! NT_SUCCESS(Status))
         {
            SetLastNtError(Status);
            RETURN((BOOL) -1);
         }
         /* Transfer lParam data to user-mode mem */
         Status = MmCopyToCaller(UserMem, (PVOID) Info.Msg.lParam, Size);
         if (! NT_SUCCESS(Status))
         {
            ZwFreeVirtualMemory(NtCurrentProcess(), (PVOID *) &UserMem,
                                &Info.LParamSize, MEM_DECOMMIT);

            SetLastNtError(Status);
            RETURN((BOOL) -1);
         }
         Info.Msg.lParam = (LPARAM) UserMem;
      }
      if (RemoveMsg && Msg.FreeLParam && 0 != Msg.Msg.lParam)
      {
         ExFreePool((void *) Msg.Msg.lParam);
      }
      Status = MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERGETMESSAGEINFO));
      if (! NT_SUCCESS(Status))
      {
         SetLastNtError(Status);
         RETURN((BOOL) -1);
      }
   }
   RETURN(Present);

CLEANUP:

   UserStackTrace();
   DPRINT("Leave NtUserPeekMessage, ret=%i\n",_ret_);

   UserLeave();
   END_CLEANUP;
}

static BOOL FASTCALL
coUserWaitMessage(HWND Wnd,
               UINT MsgFilterMin,
               UINT MsgFilterMax)
{
   PUSER_MESSAGE_QUEUE ThreadQueue;
   NTSTATUS Status;
   USER_MESSAGE Msg;

   ThreadQueue = UserGetCurrentQueue();

   //FIXME: bug her. vi kaller MsqWaitForNewMessages i infinite loop
   /*
   2k\ntuser\msgqueue.c:1720) MsqGetTimerMessage queue 8ca4bca0 msg 9daa5c60 restart FALSE
   2k\ntuser\msgqueue.c:1723) Current time 127664311829300000
   2k\ntuser\msgqueue.c:1759) No timer pending
   2k\ntuser\msgqueue.c:1823) MsqGetFirstTimerExpiry queue 8ca4bca0 wndfilter 0 msgfiltermin 0 msgfiltermax 0 expiry 9daa5c14
   2k\ntuser\msgqueue.c:1720) MsqGetTimerMessage queue 8ca4bca0 msg 9daa5c60 restart FALSE
   2k\ntuser\msgqueue.c:1723) Current time 127664311829500000
   2k\ntuser\msgqueue.c:1759) No timer pending
   2k\ntuser\msgqueue.c:1823) MsqGetFirstTimerExpiry queue 8ca4bca0 wndfilter 0 msgfiltermin 0 msgfiltermax 0 expiry 9daa5c14
   2k\ntuser\msgqueue.c:1720) MsqGetTimerMessage queue 8ca4bca0 msg 9daa5c60 restart FALSE
   2k\ntuser\msgqueue.c:1723) Current time 127664311829800000
   */

   do
   {
      if (coUserPeekMessage(&Msg, Wnd, MsgFilterMin, MsgFilterMax, PM_NOREMOVE))
      {
         return TRUE;
      }

      /* Nothing found. Wait for new messages. */
      Status = coMsqWaitForNewMessages(ThreadQueue, Wnd, MsgFilterMin, MsgFilterMax);
   }
   while ((STATUS_WAIT_0 <= Status && Status <= STATUS_WAIT_63) || STATUS_TIMEOUT == Status);

   SetLastNtError(Status);

   return FALSE;
}

BOOL STDCALL
NtUserGetMessage(
   PNTUSERGETMESSAGEINFO UnsafeInfo,
   HWND Wnd,
   UINT MsgFilterMin,
   UINT MsgFilterMax
)
/*
 * FUNCTION: Get a message from the calling thread's message queue.
 * ARGUMENTS:
 *      UnsafeMsg - Pointer to the structure which receives the returned message.
 *      Wnd - Window whose messages are to be retrieved.
 *      MsgFilterMin - Integer value of the lowest message value to be
 *                     retrieved.
 *      MsgFilterMax - Integer value of the highest message value to be
 *                     retrieved.
 */
{
   BOOL GotMessage;
   NTUSERGETMESSAGEINFO Info;
   NTSTATUS Status;
   PWINDOW_OBJECT Window;
   PMSGMEMORY MsgMemoryEntry;
   PVOID UserMem;
   UINT Size;
   USER_MESSAGE Msg;
   DECLARE_RETURN(BOOL);

   DPRINT("Enter NtUserGetMessage\n");
   UserEnterExclusive();

   /* Validate input */
   if (NULL != Wnd)
   {
      Window = IntGetWindowObject(Wnd);
      if(!Window)
         Wnd = NULL;
   }

   if (MsgFilterMax < MsgFilterMin)
   {
      MsgFilterMin = 0;
      MsgFilterMax = 0;
   }

   do
   {
      GotMessage = coUserPeekMessage(&Msg, Wnd, MsgFilterMin, MsgFilterMax, PM_REMOVE);
      if (GotMessage)
      {
         Info.Msg = Msg.Msg;
         /* See if this message type is present in the table */
         MsgMemoryEntry = FindMsgMemory(Info.Msg.message);
         if (NULL == MsgMemoryEntry)
         {
            /* Not present, no copying needed */
            Info.LParamSize = 0;
         }
         else
         {
            /* Determine required size */
            Size = MsgMemorySize(MsgMemoryEntry, Info.Msg.wParam,
                                 Info.Msg.lParam);
            /* Allocate required amount of user-mode memory */
            Info.LParamSize = Size;
            UserMem = NULL;
            Status = ZwAllocateVirtualMemory(NtCurrentProcess(), &UserMem, 0,
                                             &Info.LParamSize, MEM_COMMIT, PAGE_READWRITE);

            if (! NT_SUCCESS(Status))
            {
               SetLastNtError(Status);
               RETURN((BOOL) -1);
            }
            /* Transfer lParam data to user-mode mem */
            Status = MmCopyToCaller(UserMem, (PVOID) Info.Msg.lParam, Size);
            if (! NT_SUCCESS(Status))
            {
               ZwFreeVirtualMemory(NtCurrentProcess(), (PVOID *) &UserMem,
                                   &Info.LParamSize, MEM_DECOMMIT);
               SetLastNtError(Status);
               RETURN((BOOL) -1);
            }
            Info.Msg.lParam = (LPARAM) UserMem;
         }

         if (Msg.FreeLParam && 0 != Msg.Msg.lParam)
         {
            ExFreePool((void *) Msg.Msg.lParam);
         }

         Status = MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERGETMESSAGEINFO));
         if (! NT_SUCCESS(Status))
         {
            SetLastNtError(Status);
            RETURN((BOOL) -1);
         }
      }
      else if (! coUserWaitMessage(Wnd, MsgFilterMin, MsgFilterMax))
      {
         RETURN((BOOL) -1);
      }


   }
   while (! GotMessage);


   RETURN(WM_QUIT != Info.Msg.message);

CLEANUP:
   DPRINT("Leave NtUserGetMessage\n");
   UserLeave();
   END_CLEANUP;
}

DWORD
STDCALL
NtUserMessageCall(
   DWORD Unknown0,
   DWORD Unknown1,
   DWORD Unknown2,
   DWORD Unknown3,
   DWORD Unknown4,
   DWORD Unknown5,
   DWORD Unknown6)
{
   UNIMPLEMENTED

   return 0;
}

static NTSTATUS FASTCALL
CopyMsgToKernelMem(MSG *KernelModeMsg, MSG *UserModeMsg, PMSGMEMORY MsgMemoryEntry)
{
   NTSTATUS Status;

   PVOID KernelMem;
   UINT Size;

   *KernelModeMsg = *UserModeMsg;

   /* See if this message type is present in the table */
   if (NULL == MsgMemoryEntry)
   {
      /* Not present, no copying needed */
      return STATUS_SUCCESS;
   }

   /* Determine required size */
   Size = MsgMemorySize(MsgMemoryEntry, UserModeMsg->wParam, UserModeMsg->lParam);

   if (0 != Size)
   {
      /* Allocate kernel mem */
      KernelMem = ExAllocatePoolWithTag(PagedPool, Size, TAG_MSG);
      if (NULL == KernelMem)
      {
         DPRINT1("Not enough memory to copy message to kernel mem\n");
         return STATUS_NO_MEMORY;
      }
      KernelModeMsg->lParam = (LPARAM) KernelMem;

      /* Copy data if required */
      if (0 != (MsgMemoryEntry->Flags & MMS_FLAG_READ))
      {
         Status = MmCopyFromCaller(KernelMem, (PVOID) UserModeMsg->lParam, Size);
         if (! NT_SUCCESS(Status))
         {
            DPRINT1("Failed to copy message to kernel: invalid usermode buffer\n");
            ExFreePool(KernelMem);
            return Status;
         }
      }
      else
      {
         /* Make sure we don't pass any secrets to usermode */
         RtlZeroMemory(KernelMem, Size);
      }
   }
   else
   {
      KernelModeMsg->lParam = 0;
   }

   return STATUS_SUCCESS;
}

static NTSTATUS FASTCALL
CopyMsgToUserMem(MSG *UserModeMsg, MSG *KernelModeMsg)
{
   NTSTATUS Status;
   PMSGMEMORY MsgMemoryEntry;
   UINT Size;

   /* See if this message type is present in the table */
   MsgMemoryEntry = FindMsgMemory(UserModeMsg->message);
   if (NULL == MsgMemoryEntry)
   {
      /* Not present, no copying needed */
      return STATUS_SUCCESS;
   }

   /* Determine required size */
   Size = MsgMemorySize(MsgMemoryEntry, UserModeMsg->wParam, UserModeMsg->lParam);

   if (0 != Size)
   {
      /* Copy data if required */
      if (0 != (MsgMemoryEntry->Flags & MMS_FLAG_WRITE))
      {
         Status = MmCopyToCaller((PVOID) UserModeMsg->lParam, (PVOID) KernelModeMsg->lParam, Size);
         if (! NT_SUCCESS(Status))
         {
            DPRINT1("Failed to copy message from kernel: invalid usermode buffer\n");
            ExFreePool((PVOID) KernelModeMsg->lParam);
            return Status;
         }
      }

      ExFreePool((PVOID) KernelModeMsg->lParam);
   }

   return STATUS_SUCCESS;
}

BOOL STDCALL
NtUserPostMessage(HWND hWnd,
                  UINT Msg,
                  WPARAM wParam,
                  LPARAM lParam)
{
   DECLARE_RETURN(BOOL);

   DPRINT("Enter NtUserPostMessage\n");
   UserEnterExclusive();

   RETURN(UserPostMessage(hWnd, Msg, wParam, lParam));

CLEANUP:
   DPRINT("Leave NtUserPostMessage, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}




BOOL FASTCALL
UserPostMessage(HWND Wnd,
                UINT Msg,
                WPARAM wParam,
                LPARAM lParam)
{
   PWINDOW_OBJECT Window;
   MSG UserModeMsg, KernelModeMsg;
   LARGE_INTEGER LargeTickCount;
   NTSTATUS Status;
   PMSGMEMORY MsgMemoryEntry;

   if (WM_QUIT == Msg)
   {
      MsqPostQuitMessage(&PsGetWin32Thread()->Queue, wParam);
   }
   else if (Wnd == HWND_BROADCAST)
   {
      HWND *List;
      PWINDOW_OBJECT DesktopWindow;
      ULONG i;

      DesktopWindow = UserGetDesktopWindow();
      List = IntWinListChildren(DesktopWindow);

      if (List != NULL)
      {
         for (i = 0; List[i]; i++){
            //FIXME: ops! recursion!
            UserPostMessage(List[i], Msg, wParam, lParam);
         }
         ExFreePool(List);
      }
   }
   else
   {
      Window = IntGetWindowObject(Wnd);
      if (NULL == Window)
      {
         SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
         return(FALSE);
      }
      if(Window->hdr.flags & USER_OBJ_DESTROYING)
      {
         DPRINT1("Attempted to post message to window 0x%x that is being destroyed!\n", Wnd);
         /* FIXME - last error code? */
         return(FALSE);
      }

      UserModeMsg.hwnd = Wnd;
      UserModeMsg.message = Msg;
      UserModeMsg.wParam = wParam;
      UserModeMsg.lParam = lParam;
      MsgMemoryEntry = FindMsgMemory(UserModeMsg.message);
      Status = CopyMsgToKernelMem(&KernelModeMsg, &UserModeMsg, MsgMemoryEntry);
      if (! NT_SUCCESS(Status))
      {
         SetLastWin32Error(ERROR_INVALID_PARAMETER);
         return(FALSE);
      }
      UserGetCursorLocation(UserGetCurrentWinSta(), &KernelModeMsg.pt);
      KeQueryTickCount(&LargeTickCount);
      KernelModeMsg.time = LargeTickCount.u.LowPart;
      MsqPostMessage(Window->Queue, &KernelModeMsg,
                     NULL != MsgMemoryEntry && 0 != KernelModeMsg.lParam,
                     QS_POSTMESSAGE);
   }

   return(TRUE);

}



BOOL STDCALL
NtUserPostThreadMessage(DWORD idThread,
                        UINT Msg,
                        WPARAM wParam,
                        LPARAM lParam)
{
   MSG UserModeMsg, KernelModeMsg;
   PETHREAD peThread;
   PW32THREAD WThread;
   NTSTATUS Status;
   PMSGMEMORY MsgMemoryEntry;
   DECLARE_RETURN(BOOL);

   DPRINT("Enter NtUserPostThreadMessage\n");
   UserEnterExclusive();

   Status = PsLookupThreadByThreadId((HANDLE)idThread,&peThread);

   if( Status == STATUS_SUCCESS )
   {
      WThread = peThread->Tcb.Win32Thread;
      if( !WThread)
      {
         ObDereferenceObject( peThread );
         RETURN(FALSE);
      }

      UserModeMsg.hwnd = NULL;
      UserModeMsg.message = Msg;
      UserModeMsg.wParam = wParam;
      UserModeMsg.lParam = lParam;
      MsgMemoryEntry = FindMsgMemory(UserModeMsg.message);
      Status = CopyMsgToKernelMem(&KernelModeMsg, &UserModeMsg, MsgMemoryEntry);
      if (! NT_SUCCESS(Status))
      {
         ObDereferenceObject( peThread );
         SetLastWin32Error(ERROR_INVALID_PARAMETER);
         RETURN(FALSE);
      }
      MsqPostMessage(&WThread->Queue, &KernelModeMsg,
                     NULL != MsgMemoryEntry && 0 != KernelModeMsg.lParam,
                     QS_POSTMESSAGE);
      ObDereferenceObject( peThread );
      RETURN(TRUE);
   }
   else
   {
      SetLastNtError( Status );
      RETURN(FALSE);
   }

CLEANUP:
   DPRINT("Leave NtUserPostThreadMessage, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}

DWORD STDCALL
NtUserQuerySendMessage(DWORD Unknown0)
{
   UNIMPLEMENTED;

   return 0;
}

LRESULT FASTCALL
coUserSendMessage(HWND hWnd,
               UINT Msg,
               WPARAM wParam,
               LPARAM lParam)
{
   ULONG_PTR Result = 0;

   if(coUserSendMessageTimeout(hWnd, Msg, wParam, lParam, SMTO_NORMAL, 0, &Result))
   {
      return (LRESULT)Result;
   }

   return 0;
}

static LRESULT FASTCALL
coUserSendMessageTimeoutSingle(HWND hWnd,
                            UINT Msg,
                            WPARAM wParam,
                            LPARAM lParam,
                            UINT uFlags,
                            UINT uTimeout,
                            ULONG_PTR *uResult)
{
   ULONG_PTR Result;
   NTSTATUS Status;
   PWINDOW_OBJECT Window;
   PMSGMEMORY MsgMemoryEntry;
   INT lParamBufferSize;
   LPARAM lParamPacked;
   PUSER_MESSAGE_QUEUE Queue;

   /* FIXME: Call hooks. */
   Window = IntGetWindowObject(hWnd);
   if (!Window)
   {
      SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
      return FALSE;
   }

   Queue = UserGetCurrentQueue();

   if (Queue && Window->Queue == Queue)
   {
      if (QUEUE_2_WTHREAD(Queue)->IsExiting)
      {
         /* Never send messages to exiting threads */
         return FALSE;
      }

      /* See if this message type is present in the table */
      MsgMemoryEntry = FindMsgMemory(Msg);
      if (NULL == MsgMemoryEntry)
      {
         lParamBufferSize = -1;
      }
      else
      {
         lParamBufferSize = MsgMemorySize(MsgMemoryEntry, wParam, lParam);
      }

      if (! NT_SUCCESS(PackParam(&lParamPacked, Msg, wParam, lParam)))
      {
         DPRINT1("Failed to pack message parameters\n");
         return FALSE;
      }
      if (0xFFFF0000 != ((DWORD) Window->WndProcW & 0xFFFF0000))
      {
         Result = (ULONG_PTR)coUserCallWindowProc(Window->WndProcW, FALSE, hWnd, Msg, wParam,
                                               lParamPacked,lParamBufferSize);
      }
      else
      {
         Result = (ULONG_PTR)coUserCallWindowProc(Window->WndProcA, TRUE, hWnd, Msg, wParam,
                                               lParamPacked,lParamBufferSize);
      }

      if(uResult)
      {
         *uResult = Result;
      }

      if (! NT_SUCCESS(UnpackParam(lParamPacked, Msg, wParam, lParam)))
      {
         DPRINT1("Failed to unpack message parameters\n");
         return TRUE;
      }

      return TRUE;
   }

   if(uFlags & SMTO_ABORTIFHUNG && MsqIsHung(Window->Queue))
   {
      /* FIXME - Set a LastError? */
      return FALSE;
   }

   if(Window->hdr.flags & USER_OBJ_DESTROYING)
   {
      /* FIXME - last error? */
      DPRINT1("Attempted to send message to window 0x%x that is being destroyed!\n", hWnd);
      return FALSE;
   }

   Status = coMsqSendMessage(Window->Queue, hWnd, Msg, wParam, lParam,
                           uTimeout, (uFlags & SMTO_BLOCK), FALSE, uResult);

   if (STATUS_TIMEOUT == Status)
   {
      /* MSDN says GetLastError() should return 0 after timeout */
      SetLastWin32Error(0);
      return FALSE;
   }
   else if (! NT_SUCCESS(Status))
   {
      SetLastNtError(Status);
      return FALSE;
   }

   return TRUE;
}

LRESULT FASTCALL
coUserSendMessageTimeout(HWND hWnd,
                      UINT Msg,
                      WPARAM wParam,
                      LPARAM lParam,
                      UINT uFlags,
                      UINT uTimeout,
                      ULONG_PTR *uResult)
{
   PWINDOW_OBJECT DesktopWindow;
   HWND *Children;
   HWND *Child;



   if (HWND_BROADCAST != hWnd)
   {
      return coUserSendMessageTimeoutSingle(hWnd, Msg, wParam, lParam, uFlags, uTimeout, uResult);
   }

   DesktopWindow = UserGetDesktopWindow();
   if (NULL == DesktopWindow)
   {
      SetLastWin32Error(ERROR_INTERNAL_ERROR);
      return 0;
   }
   Children = IntWinListChildren(DesktopWindow);

   if (NULL == Children)
   {
      return 0;
   }

   for (Child = Children; NULL != *Child; Child++)
   {
      coUserSendMessageTimeoutSingle(*Child, Msg, wParam, lParam, uFlags, uTimeout, uResult);
   }

   ExFreePool(Children);

   return (LRESULT) TRUE;
}




LRESULT FASTCALL
coUserDoSendMessage(HWND Wnd,
                 UINT Msg,
                 WPARAM wParam,
                 LPARAM lParam,
                 PDOSENDMESSAGE dsm,
                 PNTUSERSENDMESSAGEINFO UnsafeInfo)
{
   LRESULT Result = TRUE;
   NTSTATUS Status;
   PWINDOW_OBJECT Window;
   NTUSERSENDMESSAGEINFO Info;
   MSG UserModeMsg;
   MSG KernelModeMsg;
   PMSGMEMORY MsgMemoryEntry;



   RtlZeroMemory(&Info, sizeof(NTUSERSENDMESSAGEINFO));

   /* FIXME: Call hooks. */
   if (HWND_BROADCAST != Wnd)
   {
      Window = IntGetWindowObject(Wnd);
      if (NULL == Window)
      {
         /* Tell usermode to not touch this one */
         Info.HandledByKernel = TRUE;
         MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERSENDMESSAGEINFO));
         SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
         return 0;
      }
   }

   /* FIXME: Check for an exiting window. */

   /* See if the current thread can handle the message */
   if (HWND_BROADCAST != Wnd && UserGetCurrentQueue() &&
         Window->Queue == UserGetCurrentQueue())
   {
      /* Gather the information usermode needs to call the window proc directly */
      Info.HandledByKernel = FALSE;
      if (0xFFFF0000 != ((DWORD) Window->WndProcW & 0xFFFF0000))
      {
         if (0xFFFF0000 != ((DWORD) Window->WndProcA & 0xFFFF0000))
         {
            /* Both Unicode and Ansi winprocs are real, see what usermode prefers */
            Status = MmCopyFromCaller(&(Info.Ansi), &(UnsafeInfo->Ansi),
                                      sizeof(BOOL));
            if (! NT_SUCCESS(Status))
            {
               Info.Ansi = ! Window->Unicode;
            }
            Info.Proc = (Info.Ansi ? Window->WndProcA : Window->WndProcW);
         }
         else
         {
            /* Real Unicode winproc */
            Info.Ansi = FALSE;
            Info.Proc = Window->WndProcW;
         }
      }
      else
      {
         /* Must have real Ansi winproc */
         Info.Ansi = TRUE;
         Info.Proc = Window->WndProcA;
      }
   }
   else
   {
      /* Must be handled by other thread */
      Info.HandledByKernel = TRUE;
      UserModeMsg.hwnd = Wnd;
      UserModeMsg.message = Msg;
      UserModeMsg.wParam = wParam;
      UserModeMsg.lParam = lParam;
      MsgMemoryEntry = FindMsgMemory(UserModeMsg.message);
      Status = CopyMsgToKernelMem(&KernelModeMsg, &UserModeMsg, MsgMemoryEntry);
      if (! NT_SUCCESS(Status))
      {
         MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERSENDMESSAGEINFO));
         SetLastWin32Error(ERROR_INVALID_PARAMETER);
         return (dsm ? 0 : -1);
      }
      if(!dsm)
      {
         Result = coUserSendMessage(KernelModeMsg.hwnd, KernelModeMsg.message,
                                 KernelModeMsg.wParam, KernelModeMsg.lParam);
      }
      else
      {
         Result = coUserSendMessageTimeout(KernelModeMsg.hwnd, KernelModeMsg.message,
                                        KernelModeMsg.wParam, KernelModeMsg.lParam,
                                        dsm->uFlags, dsm->uTimeout, &dsm->Result);
      }
      Status = CopyMsgToUserMem(&UserModeMsg, &KernelModeMsg);
      if (! NT_SUCCESS(Status))
      {
         MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERSENDMESSAGEINFO));
         SetLastWin32Error(ERROR_INVALID_PARAMETER);
         return(dsm ? 0 : -1);
      }
   }

   Status = MmCopyToCaller(UnsafeInfo, &Info, sizeof(NTUSERSENDMESSAGEINFO));
   if (! NT_SUCCESS(Status))
   {
      SetLastWin32Error(ERROR_INVALID_PARAMETER);
   }

   return (LRESULT)Result;
}

LRESULT STDCALL
NtUserSendMessageTimeout(HWND hWnd,
                         UINT Msg,
                         WPARAM wParam,
                         LPARAM lParam,
                         UINT uFlags,
                         UINT uTimeout,
                         ULONG_PTR *uResult,
                         PNTUSERSENDMESSAGEINFO UnsafeInfo)
{
   DOSENDMESSAGE dsm;
   LRESULT Result;
   DECLARE_RETURN(BOOL);

   DPRINT("Enter NtUserSendMessageTimeout\n");
   UserEnterExclusive();

   dsm.uFlags = uFlags;
   dsm.uTimeout = uTimeout;
   Result = coUserDoSendMessage(hWnd, Msg, wParam, lParam, &dsm, UnsafeInfo);
   if(uResult != NULL && Result != 0)
   {
      NTSTATUS Status;

      Status = MmCopyToCaller(uResult, &dsm.Result, sizeof(ULONG_PTR));
      if(!NT_SUCCESS(Status))
      {
         SetLastWin32Error(ERROR_INVALID_PARAMETER);
         RETURN(FALSE);
      }
   }
   RETURN(Result);

CLEANUP:
   DPRINT("Leave NtUserSendMessageTimeout, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}

LRESULT STDCALL
NtUserSendMessage(HWND Wnd,
                  UINT Msg,
                  WPARAM wParam,
                  LPARAM lParam,
                  PNTUSERSENDMESSAGEINFO UnsafeInfo)
{
   DECLARE_RETURN(BOOL);

   DPRINT("Enter NtUserSendMessage\n");
   UserEnterExclusive();

   RETURN(coUserDoSendMessage(Wnd, Msg, wParam, lParam, NULL, UnsafeInfo));

CLEANUP:
   DPRINT("Leave NtUserSendMessage, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}

BOOL STDCALL
NtUserSendMessageCallback(HWND hWnd,
                          UINT Msg,
                          WPARAM wParam,
                          LPARAM lParam,
                          SENDASYNCPROC lpCallBack,
                          ULONG_PTR dwData)
{
   UNIMPLEMENTED;

   return 0;
}




BOOL STDCALL
NtUserSendNotifyMessage(HWND hWnd,
                        UINT Msg,
                        WPARAM wParam,
                        LPARAM lParam)
{
   DECLARE_RETURN(BOOL);

   DPRINT("NtUserSendNotifyMessage\n");
   UserEnterExclusive();

UNIMPLEMENTED;

   RETURN(FALSE);

CLEANUP:
   DPRINT("Leave NtUserSendNotifyMessage, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}





BOOL STDCALL
NtUserWaitMessage(VOID)
{
   DECLARE_RETURN(BOOL);

   DPRINT("EnterNtUserWaitMessage\n");
   UserEnterExclusive();

   RETURN(coUserWaitMessage(NULL, 0, 0));

CLEANUP:
   DPRINT("Leave NtUserWaitMessage, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}

DWORD STDCALL
NtUserGetQueueStatus(BOOL ClearChanges)
{
   PUSER_MESSAGE_QUEUE Queue;
   DWORD Result;
   DECLARE_RETURN(DWORD);

   DPRINT("Enter NtUserGetQueueStatus\n");
   UserEnterExclusive();

   Queue = UserGetCurrentQueue();

   Result = MAKELONG(Queue->QueueBits, Queue->ChangesBits);
   if (ClearChanges)
   {
      //FIXME: correct? check with wine
      Queue->ChangesBits = 0;
   }

   RETURN(Result);

CLEANUP:
   DPRINT("Leave NtUserGetQueueStatus, ret=%i\n",_ret_);
   UserLeave();
   END_CLEANUP;
}

BOOL STDCALL
IntInitMessagePumpHook()
{
   PsGetCurrentThread()->Tcb.Win32Thread->MessagePumpHookValue++;
   return TRUE;
}

BOOL STDCALL
IntUninitMessagePumpHook()
{
   if (PsGetCurrentThread()->Tcb.Win32Thread->MessagePumpHookValue <= 0)
   {
      return FALSE;
   }
   PsGetCurrentThread()->Tcb.Win32Thread->MessagePumpHookValue--;
   return TRUE;
}

/* EOF */
