#include "pch.h"
#include "dirlist.h"

Dirlist *dirlist_append(Dirlist *prev_elem, const char *filename)
{
  Dirlist* cur_elem = static_cast<Dirlist*>(malloc(sizeof(Dirlist)));
  cur_elem->filename = static_cast<char *>(malloc(strlen(filename) + 1));
  strcpy(cur_elem->filename, filename);

  if (prev_elem != nullptr)
  {
    cur_elem->next = prev_elem->next;
    prev_elem->next = cur_elem;
  }
  else cur_elem->next = nullptr;
  return cur_elem;
}

Dirlist *dirlist_create(const char *filename) { return dirlist_append(nullptr, filename); }

void dirlist_free(Dirlist *dirlist)
{
  Dirlist *next;

  for (Dirlist* cur_elem = dirlist; cur_elem != nullptr; cur_elem = next)
  {
    next = cur_elem->next;
    free(cur_elem->filename);
    free(cur_elem);
  }
}

// ripped from TuxRacer
Dirlist *get_dir_file_list(char *dirname, char *pattern)
{
  char path[MAX_PATH];
  Dirlist *dirlist = nullptr;
  Dirlist *cur_elem = nullptr;
  HANDLE hFind;
  WIN32_FIND_DATAA finddata;

  _snprintf(path, MAX_PATH, "%s\\%s", dirname, pattern);

  if ((hFind = FindFirstFileA(path, &finddata)) == INVALID_HANDLE_VALUE) return nullptr;

  dirlist = dirlist_create(finddata.cFileName);
  cur_elem = dirlist;

  while (FindNextFileA(hFind, &finddata)) { cur_elem = dirlist_append(cur_elem, finddata.cFileName); }

  if (!FindClose(hFind)) DebugTrace("Couldn't close directory %s", dirname);

  return dirlist;
}