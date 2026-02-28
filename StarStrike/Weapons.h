#pragma once

/*
 * Weapons.h
 *
 * Definitions for the weapons
 *
 */

typedef struct _weapon
{
	UDWORD			nStat;				// The stats for the weapon type
	UDWORD			hitPoints;
	UDWORD			ammo;
	UDWORD			lastFired;			// When the weapon last fired
	UDWORD			recoilValue;
} WEAPON;


