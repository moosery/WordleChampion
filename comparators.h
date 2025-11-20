/*
 * FILE: comparators.h
 *
 * WHAT:
 * Defines the comparison functions used by the C standard library `qsort` function.
 * These comparators dictate the order in which the dictionary is viewed by the solver.
 *
 * WHY:
 * Sorting is central to the "Hybrid" logic.
 * - To find the "Best Math" word, we sort by Entropy (High to Low).
 * - To find the "Most Common" word, we sort by Rank (Low to High).
 * - To optimize Hard Mode scanning, we physically move eliminated words to the end.
 * By swapping comparators, we can instantly change the bot's priority system without
 * rewriting the selection logic.
 */

#pragma once
#ifndef COMPARATORS_H
#define COMPARATORS_H
#include "wordle_types.h"

 /*
  * FUNCTION: compare_dictionary_entries_by_entropy_desc
  *
  * WHAT:
  * Sorts a list of pointers to dictionary entries based on:
  * 1. Game State: Non-eliminated words come first.
  * 2. Entropy: Higher values come first.
  * 3. Tie-Breakers: Rank, then Noun/Verb type, then Alphabetical.
  *
  * WHY:
  * Used to generate the "Entropy View". This puts the mathematically strongest
  * candidates at index 0.
  */
int compare_dictionary_entries_by_entropy_desc(const void* p1, const void* p2);

/*
 * FUNCTION: compare_dictionary_entries_by_rank_desc
 *
 * WHAT:
 * Sorts a list of pointers to dictionary entries based on:
 * 1. Game State: Non-eliminated words come first.
 * 2. Frequency Rank: Lower values (more common) come first.
 * 3. Tie-Breakers: Entropy, then Noun/Verb type, then Alphabetical.
 *
 * WHY:
 * Used to generate the "Rank View". This puts the most common English words
 * at index 0.
 */
int compare_dictionary_entries_by_rank_desc(const void* p1, const void* p2);

/*
 * FUNCTION: compare_master_entries_eliminated_then_alpha
 *
 * WHAT:
 * Sorts the PHYSICAL array of `dictionary_entry_t` structs (not pointers).
 * 1. Game State: Valid words first, Eliminated words last.
 * 2. Alphabetical: A-Z.
 *
 * WHY:
 * Critical for Hard Mode optimization. By physically moving invalid words to the
 * end of the array, we can effectively "resize" the dictionary by just reducing
 * the `count` variable, without re-allocating memory.
 */
int compare_master_entries_eliminated_then_alpha(const void* p1, const void* p2);

/*
 * FUNCTION: compare_dictionary_entries_by_entropy_no_filter_desc
 *
 * WHAT:
 * Sorts a list of pointers based primarily on Entropy, IGNORING the
 * `is_eliminated` flag for the primary sort key.
 *
 * WHY:
 * Used in Normal Mode. In Normal Mode, the best guess is often a word that
 * is already eliminated (e.g., "SLATE") because it splits the remaining
 * possibilities better than any valid word. This comparator ensures we see
 * the best mathematical splitters regardless of their valid status.
 */
int compare_dictionary_entries_by_entropy_no_filter_desc(const void* p1, const void* p2);

#endif