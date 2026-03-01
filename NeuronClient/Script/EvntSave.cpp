#include "Frame.h"
#include "Script.h"
#include "EvntSave.h"

// the event save file header
using EVENT_SAVE_HDR = struct _event_save_header
{
  STRING aFileType[4];
  UDWORD version;
};

// save the context information for the script system
static BOOL eventSaveContext(UBYTE* pBuffer, UDWORD* pSize)
{
  UDWORD valSize;
  //not hashed	char *pScriptID;
  UDWORD hashedName;
  UWORD* pValSize;

  UDWORD size = 0;
  SDWORD numContext = 0;
  UBYTE* pPos = pBuffer;

  // reserve space to store how many contexts are saved
  if (pBuffer != nullptr)
    pPos += sizeof(SWORD);
  size += sizeof(SWORD);

  // go through the context list
  for (SCRIPT_CONTEXT* psCCont = psContList; psCCont != nullptr; psCCont = psCCont->psNext)
  {
    numContext += 1;

    // save the context info
    //nothashed if (!resGetIDfromData("SCRIPT", psCCont->psCode, &hashedName))
    if (!resGetHashfromData("SCRIPT", psCCont->psCode, &hashedName))
    {
      DBERROR(("eventSaveContext: couldn't find script resource id"));
      return FALSE;
    }
    SDWORD numVars = psCCont->psCode->numGlobals + psCCont->psCode->arraySize;

    if (pBuffer != nullptr)
    {
      *((UDWORD*)pPos) = hashedName;
      pPos += sizeof(UDWORD);

      *((SWORD*)pPos) = static_cast<SWORD>(numVars);
      pPos += sizeof(SWORD);

      *pPos = static_cast<UBYTE>(psCCont->release);
      pPos += sizeof(UBYTE);
    }

    size += sizeof(UDWORD) + sizeof(SWORD) + sizeof(UBYTE);

    // save the context variables
    for (VAL_CHUNK* psCVals = psCCont->psGlobals; psCVals != nullptr; psCVals = psCVals->psNext)
    {
      for (SDWORD i = 0; i < CONTEXT_VALS; i += 1)
      {
        INTERP_VAL* psVal = psCVals->asVals + i;

        // store the variable type
        if (pBuffer != nullptr)
        {
          ASSERT_TEXT(psVal->type < SWORD_MAX, "eventSaveContext: variable type number too big");
          *((SWORD*)pPos) = static_cast<SWORD>(psVal->type);
          pPos += sizeof(SWORD);
        }
        size += sizeof(SWORD);

        // store the variable value
        if (psVal->type < VAL_USERTYPESTART)
        {
          // internal type - just store the DWORD value
          if (pBuffer != nullptr)
          {
            *((UDWORD*)pPos) = static_cast<UDWORD>(psVal->v.ival);
            pPos += sizeof(UDWORD);
          }

          size += sizeof(UDWORD);
        }
        else
        {
          // user defined type
          SCR_VAL_SAVE saveFunc = asScrTypeTab[psVal->type - VAL_USERTYPESTART].saveFunc;

          ASSERT_TEXT(saveFunc != NULL, "eventSaveContext: no save function for type {}\n", static_cast<int>(psVal->type));

          // reserve some space to store how many bytes the value uses
          if (pBuffer != nullptr)
          {
            pValSize = (UWORD*)pPos;
            pPos += sizeof(UWORD);
          }
          size += sizeof(UWORD);

          if (!saveFunc(psVal->type, static_cast<UDWORD>(psVal->v.ival), pPos, &valSize))
          {
            DBERROR(("eventSaveContext: couldn't get variable value size"));
            return FALSE;
          }

          if (pBuffer != nullptr)
          {
            *pValSize = static_cast<UWORD>(valSize);
            pPos += valSize;
          }
          size += valSize;
        }

        numVars -= 1;
        if (numVars <= 0)
        {
          // done all the variables
          ASSERT_TEXT(psCVals->psNext == NULL, "eventSaveContext: number of context variables does !match the script code");
          break;
        }
      }
    }
    ASSERT_TEXT(numVars == 0, "eventSaveContext: number of context variables does !match the script code");
  }

  // actually store how many contexts have been saved
  if (pBuffer != nullptr)
    *((SWORD*)pBuffer) = static_cast<SWORD>(numContext);
  *pSize = size;

  return TRUE;
}

// load the context information for the script system
static BOOL eventLoadContext(SDWORD version, UBYTE* pBuffer, UDWORD* pSize)
{
  SCRIPT_CONTEXT* psCCont;
  INTERP_VAL* psVal;

  UDWORD size = 0;
  UBYTE* pPos = pBuffer;

  // get the number of contexts in the save file
  SDWORD numContext = *((SWORD*)pPos);
  pPos += sizeof(SWORD);
  size += sizeof(SWORD);

  // go through the contexts
  for (SDWORD context = 0; context < numContext; context += 1)
  {
    // get the script code
    auto pScriptID = (char*)pPos;
    auto psCode = static_cast<SCRIPT_CODE*>(resGetData("SCRIPT", pScriptID));
    pPos += strlen(pScriptID) + 1;

    // check the number of variables
    SDWORD numVars = psCode->numGlobals + psCode->arraySize;
    if (numVars != *((SWORD*)pPos))
    {
      DBERROR(("eventLoadContext: number of context variables does !match the script code"));
      return FALSE;
    }
    pPos += sizeof(SWORD);

    SWORD release = *pPos;
    pPos += sizeof(UBYTE);

    // create the context
    if (!eventNewContext(psCode, static_cast<CONTEXT_RELEASE>(release), &psCCont))
      return FALSE;

    // bit of a hack this - note the id of the context to link it to the triggers
    psContList->id = static_cast<SWORD>(context);

    size += strlen(pScriptID) + 1 + sizeof(SWORD) + sizeof(UBYTE);

    // set the context variables
    for (SDWORD i = 0; i < numVars; i += 1)
    {
      // get the variable type
      auto type = static_cast<INTERP_TYPE>(*((SWORD*)pPos));
      pPos += sizeof(SWORD);
      size += sizeof(SWORD);

      // get the variable value
      if (type < VAL_USERTYPESTART)
      {
        // internal type - just get the DWORD value
        UDWORD data = *((UDWORD*)pPos);
        pPos += sizeof(UDWORD);
        size += sizeof(UDWORD);

        // set the value in the context
        if (!eventSetContextVar(psCCont, static_cast<UDWORD>(i), type, data))
        {
          DBERROR(("eventLoadContext: couldn't set variable value"));
          return FALSE;
        }
      }
      else
      {
        // user defined type
        SCR_VAL_LOAD loadFunc = asScrTypeTab[type - VAL_USERTYPESTART].loadFunc;

        ASSERT_TEXT(loadFunc != NULL, "eventLoadContext: no load function for type {}\n", static_cast<int>(type));

        UDWORD valSize = *((UWORD*)pPos);
        pPos += sizeof(UWORD);
        size += sizeof(UWORD);

        // get the value pointer so that the loadFunc can write directly
        // into the variables data space.
        if (!eventGetContextVal(psCCont, static_cast<UDWORD>(i), &psVal))
        {
          DBERROR(("eventLoadContext: couldn't find variable in context"));
          return FALSE;
        }

        if (!loadFunc(version, type, pPos, valSize, (UDWORD*)&(psVal->v.ival)))
        {
          DBERROR(("eventLoadContext: couldn't get variable value"));
          return FALSE;
        }

        pPos += valSize;
        size += valSize;
      }
    }
  }

  *pSize = size;

  return TRUE;
}

// load the context information for the script system
static BOOL eventLoadContextHashed(SDWORD version, UBYTE* pBuffer, UDWORD* pSize)
{
  SCRIPT_CONTEXT* psCCont;
  //not hashed	char *pScriptID;
  INTERP_VAL* psVal;

  UDWORD size = 0;
  UBYTE* pPos = pBuffer;

  // get the number of contexts in the save file
  SDWORD numContext = *((SWORD*)pPos);
  pPos += sizeof(SWORD);
  size += sizeof(SWORD);

  // go through the contexts
  for (SDWORD context = 0; context < numContext; context += 1)
  {
    // get the script code
    //notHashed		pScriptID = (char *)pPos;
    //notHashed		psCode = resGetData("SCRIPT", pScriptID);
    //notHashed		pPos += strlen(pScriptID) + 1;
    UDWORD hashedName = *((UDWORD*)pPos);
    pPos += sizeof(UDWORD);
    auto psCode = static_cast<SCRIPT_CODE*>(resGetDataFromHash("SCRIPT", hashedName));

    // check the number of variables
    SDWORD numVars = psCode->numGlobals + psCode->arraySize;
    if (numVars != *((SWORD*)pPos))
    {
      DBERROR(("eventLoadContext: number of context variables does !match the script code"));
      return FALSE;
    }
    pPos += sizeof(SWORD);

    SWORD release = *pPos;
    pPos += sizeof(UBYTE);

    // create the context
    if (!eventNewContext(psCode, static_cast<CONTEXT_RELEASE>(release), &psCCont))
      return FALSE;

    // bit of a hack this - note the id of the context to link it to the triggers
    psContList->id = static_cast<SWORD>(context);

    size += sizeof(UDWORD) + sizeof(SWORD) + sizeof(UBYTE);

    // set the context variables
    for (SDWORD i = 0; i < numVars; i += 1)
    {
      // get the variable type
      auto type = static_cast<INTERP_TYPE>(*((SWORD*)pPos));
      pPos += sizeof(SWORD);
      size += sizeof(SWORD);

      // get the variable value
      if (type < VAL_USERTYPESTART)
      {
        // internal type - just get the DWORD value
        UDWORD data = *((UDWORD*)pPos);
        pPos += sizeof(UDWORD);
        size += sizeof(UDWORD);

        // set the value in the context
        if (!eventSetContextVar(psCCont, static_cast<UDWORD>(i), type, data))
        {
          DBERROR(("eventLoadContext: couldn't set variable value"));
          return FALSE;
        }
      }
      else
      {
        // user defined type
        SCR_VAL_LOAD loadFunc = asScrTypeTab[type - VAL_USERTYPESTART].loadFunc;

        ASSERT_TEXT(loadFunc != NULL, "eventLoadContext: no load function for type {}\n", static_cast<int>(type));

        UDWORD valSize = *((UWORD*)pPos);
        pPos += sizeof(UWORD);
        size += sizeof(UWORD);

        // get the value pointer so that the loadFunc can write directly
        // into the variables data space.
        if (!eventGetContextVal(psCCont, static_cast<UDWORD>(i), &psVal))
        {
          DBERROR(("eventLoadContext: couldn't find variable in context"));
          return FALSE;
        }

        if (!loadFunc(version, type, pPos, valSize, (UDWORD*)&(psVal->v.ival)))
        {
          DBERROR(("eventLoadContext: couldn't get variable value"));
          return FALSE;
        }

        pPos += valSize;
        size += valSize;
      }
    }
  }

  *pSize = size;

  return TRUE;
}

// return the index of a context
BOOL eventGetContextIndex(SCRIPT_CONTEXT* psContext, SDWORD* pIndex)
{
  SDWORD index = 0;
  for (SCRIPT_CONTEXT* psCurr = psContList; psCurr != nullptr; psCurr = psCurr->psNext)
  {
    if (psCurr == psContext)
    {
      *pIndex = index;
      return TRUE;
    }
    index += 1;
  }

  return FALSE;
}

// find a context from it's id number
BOOL eventFindContext(SDWORD id, SCRIPT_CONTEXT** ppsContext)
{
  for (SCRIPT_CONTEXT* psCurr = psContList; psCurr != nullptr; psCurr = psCurr->psNext)
  {
    if (psCurr->id == id)
    {
      *ppsContext = psCurr;
      return TRUE;
    }
  }

  return FALSE;
}

// save a list of triggers
BOOL eventSaveTriggerList(ACTIVE_TRIGGER* psList, UBYTE* pBuffer, UDWORD* pSize)
{
  SDWORD context;

  UDWORD size = 0;
  UBYTE* pPos = pBuffer;

  // reserve some space for the number of triggers
  if (pBuffer != nullptr)
    pPos += sizeof(SDWORD);
  size += sizeof(SDWORD);

  SDWORD numTriggers = 0;
  for (ACTIVE_TRIGGER* psCurr = psList; psCurr != nullptr; psCurr = psCurr->psNext)
  {
    numTriggers += 1;

    if (pBuffer != nullptr)
    {
      *((UDWORD*)pPos) = psCurr->testTime;
      pPos += sizeof(UDWORD);
      if (!eventGetContextIndex(psCurr->psContext, &context))
      {
        DBERROR(("eventSaveTriggerList: couldn't find context"));
        return FALSE;
      }
      *((SWORD*)pPos) = static_cast<SWORD>(context);
      pPos += sizeof(SWORD);
      *((SWORD*)pPos) = psCurr->type;
      pPos += sizeof(SWORD);
      *((SWORD*)pPos) = psCurr->trigger;
      pPos += sizeof(SWORD);
      *((UWORD*)pPos) = psCurr->event;
      pPos += sizeof(UWORD);
      *((UWORD*)pPos) = psCurr->offset;
      pPos += sizeof(UWORD);
    }
    size += sizeof(UDWORD) + sizeof(SWORD) * 3 + sizeof(UWORD) * 2;
  }
  if (pBuffer != nullptr)
    *((SDWORD*)pBuffer) = numTriggers;

  *pSize = size;

  return TRUE;
}

// load a list of triggers
BOOL eventLoadTriggerList(SDWORD version, UBYTE* pBuffer, UDWORD* pSize)
{
  SCRIPT_CONTEXT* psContext;

  version = version;

  UDWORD size = 0;
  UBYTE* pPos = pBuffer;

  // get the number of triggers
  SDWORD numTriggers = *((SDWORD*)pPos);
  pPos += sizeof(SDWORD);
  size += sizeof(SDWORD);

  for (SDWORD i = 0; i < numTriggers; i += 1)
  {
    UDWORD time = *((UDWORD*)pPos);
    pPos += sizeof(UDWORD);

    SDWORD context = *((SWORD*)pPos);
    pPos += sizeof(SWORD);
    if (!eventFindContext(context, &psContext))
    {
      DBERROR(("eventLoadTriggerList: couldn't find context"));
      return FALSE;
    }

    SDWORD type = *((SWORD*)pPos);
    pPos += sizeof(SWORD);

    SDWORD trigger = *((SWORD*)pPos);
    pPos += sizeof(SWORD);

    UDWORD event = *((UWORD*)pPos);
    pPos += sizeof(UWORD);

    UDWORD offset = *((UWORD*)pPos);
    pPos += sizeof(UWORD);

    size += sizeof(UDWORD) + sizeof(SWORD) * 3 + sizeof(UWORD) * 2;

    if (!eventLoadTrigger(time, psContext, type, trigger, event, offset))
      return FALSE;
  }

  *pSize = size;

  return TRUE;
}

// Save the state of the event system
BOOL eventSaveState(SDWORD version, UBYTE** ppBuffer, UDWORD* pFileSize)
{
  UDWORD size;

  UDWORD totalSize = sizeof(EVENT_SAVE_HDR);

  // find the size of the context save
  if (!eventSaveContext(nullptr, &size))
    return FALSE;
  totalSize += size;

  // find the size of the trigger save
  if (!eventSaveTriggerList(psTrigList, nullptr, &size))
    return FALSE;
  totalSize += size;

  // find the size of the callback trigger save
  if (!eventSaveTriggerList(psCallbackList, nullptr, &size))
    return FALSE;
  totalSize += size;

  // Allocate the buffer to save to
  auto pBuffer = static_cast<UBYTE*>(MALLOC(totalSize));
  if (pBuffer == nullptr)
  {
    DBERROR(("eventSaveState: out of memory"));
    return FALSE;
  }
  UBYTE* pPos = pBuffer;

  // set the header
  auto psHdr = (EVENT_SAVE_HDR*)pPos;
  psHdr->aFileType[0] = 'e';
  psHdr->aFileType[1] = 'v';
  psHdr->aFileType[2] = 'n';
  psHdr->aFileType[3] = 't';
  psHdr->version = version;

  pPos += sizeof(EVENT_SAVE_HDR);

  // save the contexts
  if (!eventSaveContext(pPos, &size))
    return FALSE;
  pPos += size;

  // save the triggers
  if (!eventSaveTriggerList(psTrigList, pPos, &size))
    return FALSE;
  pPos += size;

  // save the callback triggers
  if (!eventSaveTriggerList(psCallbackList, pPos, &size))
    return FALSE;
  pPos += size;

  *ppBuffer = pBuffer;
  *pFileSize = totalSize;

  return TRUE;
}

// Load the state of the event system
BOOL eventLoadState(UBYTE* pBuffer, UDWORD fileSize, BOOL bHashed)
{
  UDWORD size;

  UBYTE* pPos = pBuffer;
  UDWORD totalSize = 0;

  // Get the header
  auto psHdr = (EVENT_SAVE_HDR*)pPos;
  if (strncmp(psHdr->aFileType, "evnt", 4) != 0)
  {
    DBERROR(("eventLoadState: invalid file header"));
    return FALSE;
  }

  UDWORD version = psHdr->version;
  pPos += sizeof(EVENT_SAVE_HDR);
  totalSize += sizeof(EVENT_SAVE_HDR);

  // load the event contexts
  if (bHashed)
  {
    if (!eventLoadContextHashed(version, pPos, &size))
      return FALSE;
  }
  else
  {
    if (!eventLoadContext(version, pPos, &size))
      return FALSE;
  }

  pPos += size;
  totalSize += size;

  // load the normal triggers
  if (!eventLoadTriggerList(version, pPos, &size))
    return FALSE;
  pPos += size;
  totalSize += size;

  // load the callback triggers
  if (!eventLoadTriggerList(version, pPos, &size))
    return FALSE;

  totalSize += size;

  if (totalSize != fileSize)
  {
    DBERROR(("eventLoadState: corrupt save file"));
    return FALSE;
  }

  return TRUE;
}
