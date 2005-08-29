#ifndef _WIN32K_HOOK_H
#define _WIN32K_HOOK_H

#define HOOK_THREAD_REFERENCED	(0x1)

typedef struct tagHOOK
{
/*---------- USER_OBJECT_HDR --------------*/
  HHOOK hSelf;
  LONG refs;
  BYTE hdrFlags;
/*---------- USER_OBJECT_HDR --------------*/
   
  LIST_ENTRY Chain;          /* Hook chain entry */
  PETHREAD   Thread;         /* Thread owning the hook */
  int        HookId;         /* Hook table index */
  HOOKPROC   Proc;           /* Hook function */
  BOOLEAN    Ansi;           /* Is it an Ansi hook? */
  ULONG      Flags;          /* Some internal flags */
  UNICODE_STRING ModuleName; /* Module name for global hooks */
} HOOK, *PHOOK;

#define NB_HOOKS (WH_MAXHOOK-WH_MINHOOK+1)

typedef struct tagHOOKTABLE
{
  LIST_ENTRY Hooks[NB_HOOKS];  /* array of hook chains */
  UINT       Counts[NB_HOOKS]; /* use counts for each hook chain */
} HOOKTABLE, *PHOOKTABLE;


#endif /* _WIN32K_HOOK_H */

/* EOF */
