#include "ClParse.h"
#include "Cheat.h"
#include "Direct.h"
#include "Frame.h"
#include "FrontEnd.h"
#include "Loadsave.h"
#include "MultiInt.h"
#include "Multiplay.h"
#include "Netplay.h"
#include "PieClip.h"
#include "PieState.h"
#include "WarzoneConfig.h"
#include "WinMain.h"

BOOL scanGameSpyFlags(LPSTR gflag, LPSTR value);

// whether to start windowed
BOOL clStartWindowed;
// whether to play the intro video
BOOL clIntroVideo;

// let the end user into debug mode....
BOOL bAllowDebugMode = FALSE;

// note that render mode must come before resolution flag.
BOOL ParseCommandLine(char* psCmdLine, BOOL bGlideDllPresent)
{
  char seps[] = " ,\t\n";
  char* tokenType;
  char* token;
  BOOL bCrippleD3D = FALSE; // Disable higher resolutions for d3D
  char seps2[] = "\"";
  char cl[255];
  char cl2[255];
  unsigned char* pXor;

  /* get first token: */
  tokenType = strtok(psCmdLine, seps);
  // for cheating
  sprintf(cl, "%s", "VR^\\WZ^KVQXL\\^SSFH^XXZM");
  pXor = xorString((unsigned char*)cl);
  sprintf(cl2, "%s%s", "-", cl);

  /* loop through command line */
  while (tokenType != NULL)
  {
    if (_stricmp(tokenType, "-window") == 0)
    {
#ifdef	_DEBUG
      clStartWindowed = TRUE;
#else
      clStartWindowed = TRUE;
#endif
    }
    else if (_stricmp(tokenType, "-intro") == 0)
      SetGameMode(GS_VIDEO_MODE);
    else if (_stricmp(tokenType, "-D3D") == 0)
    {
      war_SetRendMode(REND_MODE_HAL);
      pie_SetDirect3DDeviceName("Direct3D HAL");
      //			bCrippleD3D = TRUE;
      pie_SetVideoBufferWidth(640);
      pie_SetVideoBufferHeight(480);
    }

    else if (_stricmp(tokenType, "-MMX") == 0)
    {
      war_SetRendMode(REND_MODE_RGB);
      pie_SetDirect3DDeviceName("RGB Emulation");
      pie_SetVideoBufferWidth(640);
      pie_SetVideoBufferHeight(480);
    }
    else if (_stricmp(tokenType, "-RGB") == 0)
    {
      war_SetRendMode(REND_MODE_RGB);
      pie_SetDirect3DDeviceName("RGB Emulation");
      pie_SetVideoBufferWidth(640);
      pie_SetVideoBufferHeight(480);
    }
    else if (_stricmp(tokenType, "-REF") == 0)
    {
      war_SetRendMode(REND_MODE_REF);
      pie_SetDirect3DDeviceName("Reference Rasterizer");
      pie_SetVideoBufferWidth(640);
      pie_SetVideoBufferHeight(480);
    }
    else if (_stricmp(tokenType, "-title") == 0)
      SetGameMode(GS_TITLE_SCREEN);
    else if (_stricmp(tokenType, "-game") == 0)
    {
      // find the game name
      token = strtok(NULL, seps);
      if (token == NULL)
      {
        DBERROR(("Unrecognised -game name\n"));
        return FALSE;
      }
      strncpy(pLevelName, token, 254);
      SetGameMode(GS_NORMAL);
    }
    else if (_stricmp(tokenType, "-savegame") == 0)
    {
      // find the game name
      token = strtok(NULL, seps);
      if (token == NULL)
      {
        DBERROR(("Unrecognised -savegame name\n"));
        return FALSE;
      }
      strcpy(saveGameName, "savegame\\");
      strncat(saveGameName, token, 240);
      SetGameMode(GS_SAVEGAMELOAD);
    }
    else if (_stricmp(tokenType, "-datapath") == 0)
    {
      // find the quoted path name
      token = strtok(NULL, seps);
      if (token == NULL)
      {
        DBERROR(("Unrecognised datapath\n"));
        return FALSE;
      }
      resSetBaseDir(token);

      if (_chdir(token) != 0)
        DBERROR(("Path !found: %s\n", token));
    }

    //#ifndef COVERMOUNT
    else if (_stricmp(tokenType, cl2) == 0)
      bAllowDebugMode = TRUE;
      //#endif
    else if (_stricmp(tokenType, "-640") == 0) // Temporary - this will be switchable in game
    {
      pie_SetVideoBufferWidth(640);
      pie_SetVideoBufferHeight(480);
    }
    else if (_stricmp(tokenType, "-800") == 0)
    {
      pie_SetVideoBufferWidth(800);
      pie_SetVideoBufferHeight(600);
    }
    else if (_stricmp(tokenType, "-960") == 0)
    {
      pie_SetVideoBufferWidth(960);
      pie_SetVideoBufferHeight(720);
    }
    else if (_stricmp(tokenType, "-1024") == 0)
    {
      pie_SetVideoBufferWidth(1024);
      pie_SetVideoBufferHeight(768);
    }
    else if (_stricmp(tokenType, "-1152") == 0)
    {
      pie_SetVideoBufferWidth(1152);
      pie_SetVideoBufferHeight(864);
    }
    else if (_stricmp(tokenType, "-1280") == 0)
    {
      pie_SetVideoBufferWidth(1280);
      pie_SetVideoBufferHeight(1024);
    }
    else if (_stricmp(tokenType, "-1280x720") == 0)
    {
      pie_SetVideoBufferWidth(1280);
      pie_SetVideoBufferHeight(720);
    }
    else if (_stricmp(tokenType, "-1920") == 0 || _stricmp(tokenType, "-1920x1080") == 0)
    {
      pie_SetVideoBufferWidth(1920);
      pie_SetVideoBufferHeight(1080);
    }
    else if (_stricmp(tokenType, "-2560x1440") == 0)
    {
      pie_SetVideoBufferWidth(2560);
      pie_SetVideoBufferHeight(1440);
    }
    else if (_stricmp(tokenType, "-noTranslucent") == 0)
      war_SetTranslucent(FALSE);
    else if (_stricmp(tokenType, "-noAdditive") == 0)
      war_SetAdditive(FALSE);
    else if (_stricmp(tokenType, "-noFog") == 0)
      pie_SetFogCap(FOG_CAP_NO);
    else if (_stricmp(tokenType, "-greyFog") == 0)
      pie_SetFogCap(FOG_CAP_GREY);
    else if (_stricmp(tokenType, "-2meg") == 0)
      pie_SetTexCap(TEX_CAP_2M);
    else if (_stricmp(tokenType, "-seqSmall") == 0)
      war_SetSeqMode(SEQ_SMALL);
    else if (_stricmp(tokenType, "-seqSkip") == 0)
      war_SetSeqMode(SEQ_SKIP);
    else if (_stricmp(tokenType, "-disableLobby") == 0)
      bDisableLobby = TRUE;
      /*		else if ( _stricmp( tokenType,"-routeLimit") == 0)
		{
			// find the actual maximum routing limit
			token = strtok( NULL, seps );
			if (token == NULL)
			{
				DBERROR( ("Unrecognised -routeLimit value\n") );
				return FALSE;
			}

			if (!openWarzoneKey())
			{
				DBERROR(("Couldn't open registry for -routeLimit"));
				return FALSE;
			}
			fpathSetMaxRoute(atoi(token));
			setWarzoneKeyNumeric("maxRoute",(DWORD)(fpathGetMaxRoute()));
			closeWarzoneKey();
		}*/

      // gamespy flags
    else if (_stricmp(tokenType, "+host") == 0 // host a multiplayer.
      || _stricmp(tokenType, "+connect") == 0 || _stricmp(tokenType, "+name") == 0 || _stricmp(tokenType, "+ip") == 0 ||
      _stricmp(tokenType, "+maxplayers") == 0 || _stricmp(tokenType, "+hostname") == 0)
    {
      token = strtok(NULL, seps2);
      scanGameSpyFlags(tokenType, token);
    }
    // end of gamespy

    else
    {
      // ignore (gamespy requirement)
      //	DBERROR( ("Unrecognised command-line token %s\n", tokenType) );
      //	return FALSE;
    }

    /* Get next token: */
    tokenType = strtok(NULL, seps);
  }

  /* Hack to disable higher resolution requests in d3d for the demo */
  if (bCrippleD3D)
  {
    pie_SetVideoBufferWidth(640);
    pie_SetVideoBufferHeight(480);
  }

  // look for any gamespy flags in the command line.

  return TRUE;
}

// ////////////////////////////////////////////////////////
// gamespy flags
BOOL scanGameSpyFlags(LPSTR gflag, LPSTR value)
{
  static UBYTE count = 0;
  //	UDWORD val;
  LPVOID finalconnection;

  //#ifdef COVERMOUNT
  //	return TRUE;
  //#endif

  count++;
  if (count == 1)
  {
    lobbyInitialise();
    bDisableLobby = TRUE; // dont reset everything on boot!
    gameSpy.bGameSpy = TRUE;
  }

  if (_stricmp(gflag, "+host") == 0) // host a multiplayer.
  {
    NetPlay.bHost = 1;
    game.bytesPerSec = INETBYTESPERSEC;
    game.packetsPerSec = INETPACKETS;
    NETsetupTCPIP(&finalconnection, "");
    NETselectProtocol(finalconnection);
  }
  else if (_stricmp(gflag, "+connect") == 0) // join a multiplayer.
  {
    NetPlay.bHost = 0;
    game.bytesPerSec = INETBYTESPERSEC;
    game.packetsPerSec = INETPACKETS;
    NETsetupTCPIP(&finalconnection, value);
    NETselectProtocol(finalconnection);
    // gflag is add to con to.
  }
  else if (_stricmp(gflag, "+name") == 0) // player name.
    strcpy((char*)sPlayer, value);
  else if (_stricmp(gflag, "+hostname") == 0) // game name.
    strcpy(game.name, value);

  /*!used from here on..
+ip
+maxplayers
+game
+team
+skin
+playerid
+password
tokenType = strtok( NULL, seps );
*/
  return TRUE;
}
