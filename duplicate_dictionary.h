/*
 * FILE: duplicate_dictionary.h
 *
 * WHAT:
 * Defines the interface for creating "Views" of the master dictionary.
 * A "View" is a lightweight array of pointers pointing to the heavy data
 * in the master dictionary.
 *
 * WHY:
 * To optimize decision making, we need to look at the data in different ways
 * simultaneously.
 * - The "Entropy View" lets us pick the most informative words.
 * - The "Rank View" lets us pick the most common words.
 * - The "Alpha View" (Implicit) helps with searching.
 *
 * duplicating the actual data structures would be memory inefficient and slow.
 * Creating sorted arrays of pointers is extremely fast and cache-friendly.
 */

#pragma once
#ifndef DUPLICATE_DICTIONARY_H
#define DUPLICATE_DICTIONARY_H
#include "wordle_types.h"

 /*
  * FUNCTION: duplicate_dictionary_pointers
  *
  * WHAT:
  * Allocates a new array of pointers (`dictionary_entry_t*`) and initializes
  * them to point sequentially at the `p_source_dictionary`. It then sorts
  * this new array using the provided comparison function.
  *
  * PARAMETERS:
  * - p_source_dictionary: The master array containing the actual data.
  * - source_dictionary_count: Number of items in the master array.
  * - pp_target_pointer_array: Output. Will point to the new array of pointers.
  * - compare_func: The `qsort` comparator to determine the View's order.
  *
  * RETURNS:
  * - true if allocation and sorting succeeded.
  * - false if inputs were invalid or memory allocation failed.
  *
  * WHY:
  * This generic function powers the entire "Hybrid" strategy engine. It allows
  * us to generate the `p_entropy_sorted` and `p_rank_sorted` views dynamically.
  */
bool duplicate_dictionary_pointers(const dictionary_entry_t* p_source_dictionary,
    int source_dictionary_count,
    dictionary_pointer_array_t* pp_target_pointer_array,
    int (*compare_func)(const void*, const void*));
#endif