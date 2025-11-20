/*
 * FILE: load_dictionary.h
 *
 * WHAT:
 * Defines the interface for the Dictionary Loading subsystem.
 * This module is responsible for initializing the application's primary data
 * structures by reading the master word list from disk.
 *
 * WHY:
 * Separating I/O logic from the Main loop allows for cleaner error handling
 * and modular testing. It also manages dependencies like `load_used_words`
 * to ensure the dictionary is filtered correctly upon initialization.
 */

#pragma once
#ifndef LOAD_DICTIONARY_H
#define LOAD_DICTIONARY_H
#include "wordle_types.h"
#include "load_used_words.h"

 /*
  * FUNCTION: load_dictionary
  *
  * WHAT:
  * The primary bootstrap function for data initialization.
  * 1. Loads "Used Words" (past solutions) from the web/cache (OPTIONAL).
  * 2. Reads the "AllWords.txt" master file from disk.
  * 3. Allocates memory for the dictionary array.
  * 4. Parses lines into `dictionary_entry_t` structures.
  * 5. Calculates initial Entropy for all words (Pre-computation).
  *
  * PARAMETERS:
  * - pp_dictionary: Double pointer to receive the allocated dictionary array.
  * - p_dictionary_count: Pointer to receive the total number of loaded words.
  * - should_filter_history: If true, downloads used words and excludes them.
  * If false, loads the full dictionary (Fresh Universe).
  *
  * RETURNS:
  * - true if loading was successful.
  * - false if file I/O failed or memory allocation failed.
  *
  * WHY:
  * This encapsulates the complex setup process. The main function simply calls
  * this once, and if it returns true, the application is ready to run.
  */
bool load_dictionary(dictionary_entry_t** pp_dictionary, int* p_dictionary_count, bool should_filter_history);

#endif