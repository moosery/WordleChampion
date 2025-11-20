/*
 * FILE: load_used_words.h
 *
 * WHAT:
 * Defines the interface for the "Used Words" subsystem.
 * This module is responsible for loading the list of words that have already
 * been the "Word of the Day" in past Wordle games.
 *
 * WHY:
 * To achieve a 100% win rate in the real world, we must treat "Used Words"
 * differently. Depending on the configuration, we might want to exclude them
 * entirely (because the NYT rarely repeats words) or load them for simulation
 * purposes. This module creates a global, accessible list of these words.
 */

#pragma once
#ifndef LOAD_USED_WORDS_H
#define LOAD_USED_WORDS_H
#include "wordle_types.h"

 /*
  * FUNCTION: load_used_words
  *
  * WHAT:
  * The primary loader function for the Used Words list.
  * It connects to the external data source (web scraper or local cache),
  * allocates memory, and populates the global buffer.
  *
  * PARAMETERS:
  * - pp_used_words: Output pointer. Will point to the allocated character buffer
  * containing the words (packed 5 bytes per word, no null terminators in array).
  * - p_used_word_count: Output pointer. Will hold the integer count of words loaded.
  *
  * RETURNS:
  * - true if loading was successful.
  * - false if network/disk I/O failed.
  */
bool load_used_words(char** pp_used_words, int* p_used_word_count);

/*
 * GLOBALS: Used Word State
 *
 * WHAT:
 * Global storage for the used word list.
 *
 * WHY:
 * - g_p_used_words: A dense, contiguous array of characters. We do not use
 * `dictionary_entry_t` structs here because we only need the raw text
 * for simple `strncmp` filtering during the main dictionary load.
 * - g_used_word_count: The number of entries in the buffer.
 */
#ifdef MAIN
char* g_p_used_words = NULL;
int g_used_word_count = 0;
#else
extern char* g_p_used_words;
extern int g_used_word_count;
#endif

#endif