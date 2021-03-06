#pragma once

// Defines a generic 'map' interface...

#ifndef MAP_TYPE
#define MAP_TYPE SKIPLIST
#endif

#if MAPTYPE == SKIPLIST

#include "skiplist.h"

#define map skiplist
#define miter sliter

#define m_create sl_create
#define m_set sl_set
#define m_app sl_app
#define m_get sl_get
#define m_del sl_del
#define m_find sl_find
#define m_findkey sl_findkey
#define m_is_nextkey sl_is_nextkey
#define m_nextkey sl_nextkey
#define m_first sl_first
#define m_next sl_next
#define m_done sl_done
#define m_destroy sl_destroy

#endif
