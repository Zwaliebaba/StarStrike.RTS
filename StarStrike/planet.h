#pragma once

struct galaxy_seed
{
  unsigned char a;/* 6c */
  unsigned char b;/* 6d */
  unsigned char c;/* 6e */
  unsigned char d;/* 6f */
  unsigned char e;/* 70 */
  unsigned char f;/* 71 */
};

struct planet_data
{
  int government;
  int economy;
  int techlevel;
  int population;
  int productivity;
  int radius;
};

char *describe_planet(galaxy_seed);
void capitalise_name(char *name);
void name_planet(char *gname, galaxy_seed glx);
galaxy_seed find_planet(int cx, int cy);
int find_planet_number(galaxy_seed planet);
void waggle_galaxy(galaxy_seed *glx_ptr);
void describe_inhabitants(char *str, galaxy_seed planet);
void generate_planet_data(planet_data *pl, galaxy_seed planet_seed);
void set_current_planet(galaxy_seed new_planet);

galaxy_seed _find_planet(int cx, int cy, galaxy_seed glx);