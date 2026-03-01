/*
 * config.h
 * load && save favourites to the registry.
 */

extern BOOL loadConfig				(BOOL bResourceAvailable);
extern BOOL loadRenderMode			(VOID);
extern BOOL saveConfig				(VOID);
extern BOOL getWarzoneKeyNumeric	(const char *pName,DWORD *val);
extern BOOL openWarzoneKey			(VOID);
extern BOOL closeWarzoneKey			(VOID);
extern BOOL setWarzoneKeyNumeric	(const char *pName,DWORD val);

extern BOOL	bAllowSubtitles;
