/*
 * SeqDisp.c		(Sequence Display)
 *
 * Functions for the display of the Escape Sequences
 *
 */
#include "Frame.h"
#include "Widget.h"
#include "Rendmode.h"
#include "SeqDisp.h"
#include "Loop.h"
#include "Piedef.h"
#include "Piefunc.h"
#include "PieState.h"
#include "HCI.h"//for font
#include "Audio.h"
#include "Deliverance.h"
#include "WarzoneConfig.h"

#include "Multiplay.h"
#include "GTime.h"
#include "Mission.h"
#include "Script.h"
#include "ScriptTabs.h"
#include "Design.h"
#include "Wrappers.h"

/***************************************************************************/
/*
 *	local Definitions
 */
/***************************************************************************/
#define INCLUDE_AUDIO
#define DUMMY_VIDEO
#define RPL_WIDTH 640
#define RPL_HEIGHT 480
#define RPL_DEPTH 2	//bytes, 16bit
#define RPL_BITS_555 15	//15bit
#define RPL_MASK_555 0x7fff	//15bit
#define RPL_FRAME_TIME frameDuration	//milliseconds
#define STD_FRAME_TIME 40 //milliseconds
#define VIDEO_PLAYBACK_WIDTH 640
#define VIDEO_PLAYBACK_HEIGHT 480
#define VIDEO_PLAYBACK_DELAY 0
#define MAX_TEXT_OVERLAYS 32
#define MAX_SEQ_LIST	  6
#define SUBTITLE_BOX_MIN 430
#define SUBTITLE_BOX_MAX 480


typedef struct {
	char pText[MAX_STR_LENGTH];
	SDWORD x;
	SDWORD y;
	SDWORD startFrame;
	SDWORD endFrame;
	BOOL	bSubtitle;
} SEQTEXT; 

typedef struct {
	char		*pSeq;						//name of the sequence to play
	char		*pAudio;					//name of the wav to play
	BOOL		bSeqLoop;					//loop this sequence
	SDWORD		currentText;				//cuurent number of text messages for this seq
	SEQTEXT		aText[MAX_TEXT_OVERLAYS];	//text data to display for this sequence
} SEQLIST; 
/***************************************************************************/
/*
 *	local Variables
 */
/***************************************************************************/

static BOOL	bBackDropWasAlreadyUp;

BOOL bSeqInit = FALSE;
BOOL bSeqPlaying = FALSE;
BOOL bAudioPlaying = FALSE;
BOOL bHoldSeqForAudio = FALSE;
BOOL bSeq8Bit = TRUE;
BOOL bCDPath = FALSE;
BOOL bHardPath = FALSE;
BOOL bSeqSubtitles = TRUE;
char aCDPath[MAX_STR_LENGTH];
char aHardPath[MAX_STR_LENGTH];
char aVideoName[MAX_STR_LENGTH];
char aAudioName[MAX_STR_LENGTH];
char aTextName[MAX_STR_LENGTH];
char aSubtitleName[MAX_STR_LENGTH];
char* pVideoBuffer = NULL;
char* pVideoPalette = NULL;
SEQLIST aSeqList[MAX_SEQ_LIST];
static SDWORD currentSeq = -1;
static SDWORD currentPlaySeq = -1;
static SDWORD frameDuration = 40;

static BOOL			g_bResumeInGame = FALSE;

static int videoFrameTime = 0;
static	SDWORD frame = 0;

/***************************************************************************/
/*
 *	local ProtoTypes
 */
/***************************************************************************/

void	clearVideoBuffer(iSurface *surface);
void	seq_SetVideoPath(void);

/***************************************************************************/
/*
 *	Source
 */
/***************************************************************************/

 /* Renders a video sequence specified by filename to a buffer
  * STUBBED: RPL video codec removed. Handles SEQUENCE_KILL for IntelMap close.
  */
BOOL	seq_RenderVideoToBuffer( iSurface *pSurface, char *sequenceName, int time, int seqCommand)
{
	UNUSEDPARAMETER(pSurface);
	UNUSEDPARAMETER(sequenceName);
	UNUSEDPARAMETER(time);

	if (seqCommand == SEQUENCE_KILL)
	{
		bSeqPlaying = FALSE;
		return TRUE;
	}

	return FALSE;
}

BOOL	seq_BlitBufferToScreen(char* screen, SDWORD screenStride, SDWORD xOffset, SDWORD yOffset)
{
	UNUSEDPARAMETER(screen);
	UNUSEDPARAMETER(screenStride);
	UNUSEDPARAMETER(xOffset);
	UNUSEDPARAMETER(yOffset);
	return TRUE;
}


void	clearVideoBuffer(iSurface *surface)
{
UDWORD	i;
UDWORD	*toClear;

	toClear = (UDWORD *)surface->buffer;
	for (i=0; i<(UDWORD)(surface->size/4); i++)
	{
		*toClear++ = (UDWORD)0xFCFCFCFC;
	}
}
 
BOOL seq_ReleaseVideoBuffers(void)
{
	FREE(pVideoBuffer);
	FREE(pVideoPalette);
	return TRUE;
}
 
BOOL seq_SetupVideoBuffers(void)
{
	SDWORD c,mallocSize;
	UBYTE r,g,b;
	//assume 320 * 240 * 16bit playback surface
	mallocSize = (RPL_WIDTH*RPL_HEIGHT*RPL_DEPTH);
	if ((pVideoBuffer = MALLOC(mallocSize)) == NULL)
	{
		return FALSE;
	}

	mallocSize = 1<<(RPL_BITS_555);//palette only used in 555mode
	if ((pVideoPalette = MALLOC(mallocSize)) == NULL)
	{
		return FALSE;
	}

	//Assume 555 RGB buffer for 8 bit rendering
	c = 0;
	for(r = 0 ; r < 32 ; r++)
	{
	LOADBARCALLBACK();	//	loadingScreenCallback();

		for(g = 0 ; g < 32 ; g++)
		{
			for(b = 0 ; b < 32 ; b++)
			{
				pVideoPalette[(SDWORD)c] = (char)pal_GetNearestColour((uint8)(r<<3),(uint8)(g<<3),(uint8)(b<<3));
				c++;
			}
		}
	}

	return TRUE;
}

void seq_SetVideoPath(void)
{
	WIN32_FIND_DATA findData;
	HANDLE	fileHandle;
	/* now set up the hard disc path */
	if (!bHardPath)
	{
		strcpy(aHardPath, "sequences\\");
		fileHandle = FindFirstFile("sequences\\*.rpl",&findData);
		if (fileHandle == INVALID_HANDLE_VALUE)
		{
			bHardPath = FALSE;
		}
		else
		{
			bHardPath = TRUE;
			FindClose(fileHandle);
		}
	}
}



BOOL SeqEndCallBack( AUDIO_SAMPLE *psSample )
{
	psSample;
	bAudioPlaying = FALSE;
	dbg_printf("************* briefing ended **************\n");

	return TRUE;
}

//full screen video functions
// STUBBED: RPL video codec removed. Returns FALSE so seqDispCDOK fires CALL_VIDEO_QUIT.
BOOL seq_StartFullScreenVideo(char* videoName, char* audioName)
{
	UNUSEDPARAMETER(videoName);
	UNUSEDPARAMETER(audioName);
	return FALSE;
}

// STUBBED: RPL video codec removed. Returns FALSE to signal video finished.
BOOL seq_UpdateFullScreenVideo(CLEAR_MODE *pbClear)
{
	UNUSEDPARAMETER(pbClear);
	return FALSE;
}

BOOL seq_StopFullScreenVideo(void)
{
	if (!seq_AnySeqLeft())
	{
		if (loop_GetVideoMode() > 0)
		{
			loop_ClearVideoPlaybackMode();
		}
	}

	if (!seq_AnySeqLeft())
	{
		if ( g_bResumeInGame == TRUE )
		{
			resetDesignPauseState();
			intAddReticule();
			g_bResumeInGame = FALSE;
		}
	}

	return TRUE;
}

BOOL seq_GetVideoSize(SDWORD* pWidth, SDWORD* pHeight)
{
	*pWidth = 0;
	*pHeight = 0;
	return FALSE;
}

#define BUFFER_WIDTH 600
#define FOLLOW_ON_JUSTIFICATION 160
#define MIN_JUSTIFICATION 40

// add a string at x,y or add string below last line if x and y are 0
BOOL seq_AddTextForVideo(UBYTE* pText, SDWORD xOffset, SDWORD yOffset, SDWORD startFrame, SDWORD endFrame, SDWORD bJustify, UDWORD PSXSeqNumber)
{
	SDWORD sourceLength, currentLength;
	char* currentText;
	SDWORD justification;
static SDWORD lastX;

	iV_SetFont(WFont);

	ASSERT((aSeqList[currentSeq].currentText < MAX_TEXT_OVERLAYS,
		"seq_AddTextForVideo: too many text lines"));

	sourceLength = strlen(pText); 
	currentLength = sourceLength;
	currentText = &(aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].pText[0]);

	//if the string is bigger than the buffer get the last end of the last fullword in the buffer
	if (currentLength >= MAX_STR_LENGTH)
	{
		currentLength = MAX_STR_LENGTH - 1;
		//get end of the last word
		while((pText[currentLength] != ' ') && (currentLength > 0))
		{
			currentLength--;
		}
		currentLength--;
	}
	
	memcpy(currentText,pText,currentLength);
	currentText[currentLength] = 0;//terminate the string what ever
	
	//check the string is shortenough to print
	//if not take a word of the end and try again
	while(iV_GetTextWidth(currentText) > BUFFER_WIDTH)
	{
		currentLength--;
		while((pText[currentLength] != ' ') && (currentLength > 0))
		{
			currentLength--;
		}
		currentText[currentLength] = 0;//terminate the string what ever
	}
	currentText[currentLength] = 0;//terminate the string what ever

	//check if x and y are 0 and put text on next line
	if (((xOffset == 0) && (yOffset == 0)) && (currentLength > 0))
	{
		aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].x = lastX;
		//	aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText-1].x;
		aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].y = aSeqList[currentSeq].
			aText[aSeqList[currentSeq].currentText-1].y + iV_GetTextLineSize();
	}
	else
	{
		aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].x = xOffset + D_W;
		aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].y = yOffset + D_H;
	}
	lastX = aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].x;

	if ((bJustify) && (currentLength == sourceLength))
	{
		//justify this text
		justification = BUFFER_WIDTH - iV_GetTextWidth(currentText);
		if ((bJustify == SEQ_TEXT_JUSTIFY) && (justification > MIN_JUSTIFICATION))
		{
			aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].x += (justification/2);
		}
		else if ((bJustify == SEQ_TEXT_FOLLOW_ON) && (justification > FOLLOW_ON_JUSTIFICATION))
		{

		}
	}


	//set start and finish times for the objects	
	aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].startFrame = startFrame;
	aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].endFrame = endFrame;
	aSeqList[currentSeq].aText[aSeqList[currentSeq].currentText].bSubtitle = bJustify;

	aSeqList[currentSeq].currentText++;
	if (aSeqList[currentSeq].currentText >= MAX_TEXT_OVERLAYS)
	{
		aSeqList[currentSeq].currentText = 0;
	}

	//check text is okay on the screen
	if (currentLength < sourceLength)
	{
		//RECURSE x= 0 y = 0 for nextLine
		if (bJustify == SEQ_TEXT_JUSTIFY)
		{
			bJustify = SEQ_TEXT_POSITION;
		}
		seq_AddTextForVideo(&pText[currentLength + 1], 0, 0, startFrame, endFrame, bJustify,0);
	}
	return TRUE;
}

BOOL seq_ClearTextForVideo(void)
{
	SDWORD i, j;

	for (j=0; j < MAX_SEQ_LIST; j++)
	{
		for(i=0;i<MAX_TEXT_OVERLAYS;i++)
		{
			aSeqList[j].aText[i].pText[0] = 0;
			aSeqList[j].aText[i].x = 0;
			aSeqList[j].aText[i].y = 0;
			aSeqList[j].aText[i].startFrame = 0;
			aSeqList[j].aText[i].endFrame = 0;
			aSeqList[j].aText[i].bSubtitle = 0;
		}
		aSeqList[j].currentText = 0;
	}
	return TRUE;
}

BOOL	seq_AddTextFromFile(STRING *pTextName, BOOL bJustify)
{
	UBYTE *pTextBuffer, *pCurrentLine, *pText;
	UDWORD fileSize;
	HANDLE	fileHandle;
	WIN32_FIND_DATA findData;
	BOOL endOfFile = FALSE;
	SDWORD xOffset, yOffset, startFrame, endFrame;
	UBYTE* seps	= "\n";

	strcpy(aTextName,"sequenceAudio\\");
	strcat(aTextName,pTextName);
	
/*
	fileHandle = FindFirstFile(aTextName,&findData);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	FindClose(fileHandle);
*/	
	if (loadFileToBufferNoError(aTextName, DisplayBuffer, displayBufferSize, &fileSize) == FALSE)
	{
		return FALSE;
	}

	pTextBuffer = DisplayBuffer;
	pCurrentLine = strtok(pTextBuffer,seps);
	while(pCurrentLine != NULL)
	{
		if (*pCurrentLine != '/')
		{
			if (sscanf(pCurrentLine,"%d %d %d %d", &xOffset, &yOffset, &startFrame, &endFrame) == 4)
			{
				//get the text
				pText = strrchr(pCurrentLine,'"');
				ASSERT ((pText != NULL,"seq_AddTextFromFile error parsing text file"));
				if (pText != NULL)
				{
					*pText = (UBYTE)0;
				}
				pText = strchr(pCurrentLine,'"');
				ASSERT ((pText != NULL,"seq_AddTextFromFile error parsing text file"));
				if (pText != NULL)
				{
					seq_AddTextForVideo(&pText[1], xOffset, yOffset, startFrame, endFrame, bJustify,0);
				}
			}
		}
		//get next line
		pCurrentLine = strtok(NULL,seps);
	}
	return TRUE;
}



//clear the sequence list
void seq_ClearSeqList(void)
{
	SDWORD i;
	
	seq_ClearTextForVideo();
	for(i=0;i<MAX_SEQ_LIST;i++)
	{
		aSeqList[i].pSeq = NULL;
	}
	currentSeq = -1;
	currentPlaySeq = -1;
}

//add a sequence to the list to be played
void seq_AddSeqToList(STRING *pSeqName, STRING *pAudioName, STRING *pTextName, BOOL bLoop, UDWORD PSXSeqNumber)
{
	SDWORD strLen;
	currentSeq++;
	

	if ((currentSeq) >=  MAX_SEQ_LIST)
	{
		ASSERT((FALSE, "seq_AddSeqToList: too many sequences"));
		return;
	}
#ifdef SEQ_LOOP
	bLoop = TRUE;
#endif

	//OK so add it to the list
	aSeqList[currentSeq].pSeq = pSeqName;
	aSeqList[currentSeq].pAudio = pAudioName;
	aSeqList[currentSeq].bSeqLoop = bLoop;
	if (pTextName != NULL)
	{
		seq_AddTextFromFile(pTextName, FALSE);//SEQ_TEXT_POSITION);//ordinary text not justified
	}

	if (bSeqSubtitles)
	{
		//check for a subtitle file
		strLen = strlen(pSeqName);
		ASSERT((strLen < MAX_STR_LENGTH,"seq_AddSeqToList: sequence name error"));
		strcpy(aSubtitleName,pSeqName);
		aSubtitleName[strLen - 4] = 0;
		strcat(aSubtitleName,".txt");
		seq_AddTextFromFile(aSubtitleName, TRUE);//SEQ_TEXT_JUSTIFY);//subtitles centre justified
	}
}

/*checks to see if there are any sequences left in the list to play*/
BOOL seq_AnySeqLeft(void)
{
	UBYTE		nextSeq;

	nextSeq = (UBYTE)(currentPlaySeq+1);

	//check haven't reached end
	if (nextSeq > MAX_SEQ_LIST)
	{
		return FALSE;
	}
	else if (aSeqList[nextSeq].pSeq)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void seqDispCDOK( void )
{
	BOOL	bPlayedOK;

	if ( bBackDropWasAlreadyUp == FALSE )
	{
		screen_StopBackDrop();
	}

	currentPlaySeq++;
	if (currentPlaySeq >= MAX_SEQ_LIST)
	{
		bPlayedOK = FALSE;
	}
	else
	{
		bPlayedOK = seq_StartFullScreenVideo( aSeqList[currentPlaySeq].pSeq,
											  aSeqList[currentPlaySeq].pAudio );
	}

	if ( bPlayedOK == FALSE )
	{
        //don't do the callback if we're playing the win/lose video
        if (!getScriptWinLoseVideo())
        {
    		eventFireCallbackTrigger(CALL_VIDEO_QUIT);
        }
        else
        {
            displayGameOver(getScriptWinLoseVideo() == PLAY_WIN);
        }
	}
}

/*returns the next sequence in the list to play*/
void seq_StartNextFullScreenVideo(void)
{
	seqDispCDOK();
}

void seq_SetSubtitles(BOOL bNewState)
{
	bSeqSubtitles = bNewState;
}

BOOL seq_GetSubtitles(void)
{
	return bSeqSubtitles;
}


/*play a video now and clear all other videos, front end use only*/
/*
BOOL seq_PlayVideo(char* pSeq, char* pAudio)
{
	seq_ClearSeqList();//other wise me might trigger these videos when we finish
	seq_StartFullScreenVideo(pSeq, pAudio);
	while (loop_GetVideoStatus())
	{
		videoLoop();
	}
	return TRUE;
}
*/