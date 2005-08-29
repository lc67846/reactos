#ifndef _WIN32K_NTUSER_H
#define _WIN32K_NTUSER_H


typedef struct _USER_OBJECT_HDR
{
   HANDLE hSelf;
   LONG refs;
   BYTE flags;
} USER_OBJECT_HDR, *PUSER_OBJECT_HDR;


typedef struct _USER_REFERENCE_ENTRY
{
   SINGLE_LIST_ENTRY Entry;
   PUSER_OBJECT_HDR hdr;
} USER_REFERENCE_ENTRY, *PUSER_REFERENCE_ENTRY;


extern char* _file;
extern DWORD _line;
extern DWORD _locked;

extern FAST_MUTEX UserLock;

#define DECLARE_RETURN(type) type _ret_
#define RETURN(value) { _ret_ = value; goto _cleanup_; }
#define CLEANUP /*unreachable*/ ASSERT(FALSE); _cleanup_
#define END_CLEANUP return _ret_;

#if 0

#define UserEnterShared() { \
 UUserEnterShared(); ASSERT(InterlockedIncrement(&_locked) > 0); \
 }
 #define UserEnterExclusive() {\
  UUserEnterExclusive(); ASSERT(InterlockedIncrement(&_locked) > 0); \
  }

#define UserLeave() { ASSERT(InterlockedDecrement(&_locked) >= 0);  \
 UUserLeave(); }

#endif


VOID FASTCALL UserStackTrace();

#define UserEnterShared() \
{ \
   DPRINT1("try lock, %s, %i (%i)\n",__FILE__,__LINE__, _locked); \
   ASSERT(UserLock.Owner != KeGetCurrentThread()); \
   UUserEnterShared(); \
   ASSERT(InterlockedIncrement(&_locked) == 1 /*> 0*/); \
   DPRINT("got lock, %s, %i (%i)\n",__FILE__,__LINE__, _locked); \
}

#define UserEnterExclusive() \
{ \
  /* DPRINT1("try lock, %s, %i (%i)\n",__FILE__,__LINE__, _locked);*/ \
   if (UserLock.Owner == KeGetCurrentThread()){ \
      DPRINT1("file %s, line %i\n",_file, _line); \
      ASSERT(FALSE); \
   }  \
   UUserEnterExclusive(); \
   ASSERT(InterlockedIncrement(&_locked) == 1 /*> 0*/); \
   _file = __FILE__; _line = __LINE__; \
  /* DPRINT("got lock, %s, %i (%i)\n",__FILE__,__LINE__, _locked);*/ \
}

#define UserLeave() \
{ \
   ASSERT(InterlockedDecrement(&_locked) == 0/*>= 0*/); \
   /*DPRINT("unlock, %s, %i (%i)\n",__FILE__,__LINE__, _locked);*/ \
   if (UserLock.Owner != KeGetCurrentThread()) { \
     DPRINT1("file %s, line %i\n",_file, _line); \
     ASSERT(FALSE); \
   } \
   _file = __FILE__; _line = __LINE__; \
   UUserLeave(); \
}
 




#define GetWnd(hwnd) IntGetWindowObject(hwnd)




#if 0
#define IntLockUserShared() {if(_locked){ DPRINT1("last %s, %i\n",_file,_line);} \
 IIntLockUserShared(); \
  _locked++; _file = __FILE__; _line = __LINE__; \
  }
  
 #define IntUserEnterExclusive() {if(_locked){ DPRINT1("last %s, %i\n",_file,_line);} \
  IIntUserEnterExclusive(); \
  _locked++; _file = __FILE__; _line = __LINE__; \
  }


#define IntUserLeave() { if(!_locked){ DPRINT1("not locked %s, %i\n",__FILE__,__LINE__);} \
 _locked--; IIntUserLeave(); }
#endif

NTSTATUS FASTCALL InitUserImpl(VOID);
VOID FASTCALL UninitUser(VOID);
VOID FASTCALL UUserEnterShared(VOID);
VOID FASTCALL UUserEnterExclusive(VOID);
VOID FASTCALL UUserLeave(VOID);
BOOL FASTCALL UserIsEntered();










#endif /* _WIN32K_NTUSER_H */

/* EOF */
