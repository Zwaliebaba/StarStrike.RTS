/* 
 * mplayer.c
 * mplayer specific code. Keep it out of the main game!
 */

#include "Frame.h"
#include "Droid.h"
#include "Netplay.h"
#include "Multiplay.h"
#include "Multistat.h"

#include "Mpdpxtra.h"

// submission fields.
// obveall stats , based on local store 
#define MP_SCORE		1
#define MP_KILLS		2
#define MP_PLAYS		3
#define MP_WINS			4
#define MP_LOSES		5

// stats about this game only. More secure.
#define MP_GAME_SCORE	6
#define MP_GAME_KILLS	7	
#define MP_GAME_WIN		8			// should be 0 or 1.
#define MP_GAME_LOSE	9			// should be 0 or 1.

// submit score routines.
BOOL mplayerSubmit(void)
{
	PLAYERSTATS stats,stats2;
	MPPLAYERID	mpID;

	if(!NetPlay.bLobbyLaunched)
	{
		return FALSE;
	}

	// just do it, it wont moan if it can't.....
	if( MPDPXTRA_Init() == MPDPXTRAERR_OK)
	{
		stats = getMultiStats(selectedPlayer,FALSE);

		// should use MPDPXTRA_AddScoreResultEx
		// and also use MPDPXTRA_DPIDToMPPLAYERID when the player joins.

		loadMultiStats(NetPlay.players[selectedPlayer].name, &stats2);




		//  submit to server.
		MPDPXTRA_SaveScoreResults();

		MPDPXTRA_Destroy();
	}

	return TRUE;
}
