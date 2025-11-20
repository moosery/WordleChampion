/*
 * FILE: comparators.cpp
 *
 * WHAT:
 * Implements the comparison logic used by the standard library `qsort` function.
 * This file defines the rules for ordering dictionary entries based on various
 * criteria: Entropy, Frequency Rank, Game State (Eliminated), and Morphology.
 *
 * WHY:
 * Sorting is the primary mechanism for decision making in this solver.
 * To "Pick the best word," we simply sort the list and pick index 0.
 * Therefore, the logic inside these comparators dictates the bot's entire strategy.
 *
 * TIE-BREAKING:
 * A critical aspect here is Determinism. If two words have the exact same
 * entropy (e.g., 4.3215), we must have a stable rule to decide which comes first.
 * We chain comparisons (Entropy -> Rank -> Attributes -> Alpha) to ensure the
 * sort order is always identical run-to-run.
 */

#include "comparators.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

 // --- Internal Helper Functions ---
 // These calculate the difference between specific fields.
 // Positive result = Entry 1 > Entry 2.
 // Negative result = Entry 1 < Entry 2.
 // Zero = Equal.

 /*
  * HELPER: noun_type_diff
  *
  * WHAT:
  * Compares noun types based on a preference order:
  * Pronoun (R) > Singular (S) > Not a Noun (N) > Plural (P).
  *
  * WHY:
  * Plurals are weak guesses in Wordle (often end in S, which is common but
  * structurally boring). Pronouns and Singular nouns are stronger answers.
  */
static int noun_type_diff(const dictionary_entry_t* entry1, const dictionary_entry_t* entry2)
{
    static const char order[] = { 'R', 'S', 'N', 'P' };
    int posA = 4; int posB = 4;
    for (int i = 0; i < 4; ++i)
    {
        if (entry1->noun_type == order[i]) posA = i;
        if (entry2->noun_type == order[i]) posB = i;
    }
    return posA - posB;
}

/*
 * HELPER: verb_type_diff
 *
 * WHAT:
 * Compares verb types based on preference:
 * Not a Verb (N) > Present (P) > 3rd Person (S) > Past (T).
 *
 * WHY:
 * Past tense (ED) and 3rd Person (S) are weak guesses. Base forms are better.
 */
static int verb_type_diff(const dictionary_entry_t* entry1, const dictionary_entry_t* entry2)
{
    static const char order[] = { 'N', 'P', 'S', 'T' };
    int posA = 4; int posB = 4;
    for (int i = 0; i < 4; ++i)
    {
        if (entry1->verb_type == order[i]) posA = i;
        if (entry2->verb_type == order[i]) posB = i;
    }
    return posA - posB;
}

/*
 * HELPER: entropy_diff
 *
 * WHAT:
 * Compares floating point Entropy.
 * Note: Returns 1 if E1 < E2 because we want DESCENDING order (High to Low).
 */
static int entropy_diff(const dictionary_entry_t* entry1, const dictionary_entry_t* entry2)
{
    if (entry1->entropy < entry2->entropy) return 1;
    else if (entry1->entropy > entry2->entropy) return -1;
    else return 0;
}

/*
 * HELPER: dup_diff
 *
 * WHAT:
 * Penalizes words with duplicate letters.
 *
 * WHY:
 * Words with unique letters (e.g., "WORLD") test 5 distinct characters.
 * Words with duplicates (e.g., "EMMYS") test fewer. We prefer unique.
 */
static int dup_diff(const dictionary_entry_t* entry1, const dictionary_entry_t* entry2)
{
    if (entry1->contains_duplicate_letters == false && entry2->contains_duplicate_letters == true) return(-1);
    else if (entry2->contains_duplicate_letters == false && entry1->contains_duplicate_letters == true) return(1);
    else return(0);
}

/*
 * HELPER: rank_diff
 *
 * WHAT:
 * Compares frequency rank.
 * Note: Returns 1 if R1 < R2 because we want DESCENDING order.
 * (Assuming higher rank int = higher frequency).
 */
static int rank_diff(const dictionary_entry_t* entry1, const dictionary_entry_t* entry2)
{
    if (entry1->frequency_rank < entry2->frequency_rank) return 1;
    else if (entry1->frequency_rank > entry2->frequency_rank) return -1;
    else return 0;
}

/*
 * HELPER: eliminated_diff
 *
 * WHAT:
 * Checks the game state flag `is_eliminated`.
 * Non-eliminated (valid) words always come before Eliminated words.
 */
static int eliminated_diff(const dictionary_entry_t* entry1, const dictionary_entry_t* entry2)
{
    if (entry1->is_eliminated && !entry2->is_eliminated) return 1;
    if (!entry1->is_eliminated && entry2->is_eliminated) return -1;
    return 0;
}

/*
 * HELPER: compare_with_entropy_tie_breaker
 *
 * WHAT:
 * The master tie-breaking chain for Entropy sorts.
 * If Entropy is equal, decide based on:
 * 1. Duplicates (prefer unique)
 * 2. Noun Type (prefer singular)
 * 3. Verb Type (prefer base form)
 * 4. Frequency Rank (prefer common)
 * 5. Alphabetical (Arbitrary final resolver)
 */
static int compare_with_entropy_tie_breaker(const dictionary_entry_t* entry1, const dictionary_entry_t* entry2)
{
    int result = dup_diff(entry1, entry2);
    if (result != 0) return result;
    result = noun_type_diff(entry1, entry2);
    if (result != 0) return result;
    result = verb_type_diff(entry1, entry2);
    if (result != 0) return result;
    result = rank_diff(entry1, entry2);
    if (result != 0) return result;
    return strncmp(entry1->word, entry2->word, WORDLE_WORD_LENGTH);
}

/*
 * HELPER: compare_with_rank_tie_breaker
 *
 * WHAT:
 * The master tie-breaking chain for Rank sorts.
 * If Rank is equal, decide based on:
 * 1. Duplicates
 * 2. Noun/Verb types
 * 3. Entropy (prefer higher info)
 * 4. Alphabetical
 */
static int compare_with_rank_tie_breaker(const dictionary_entry_t* entry1, const dictionary_entry_t* entry2)
{
    int result = dup_diff(entry1, entry2);
    if (result != 0) return result;
    result = noun_type_diff(entry1, entry2);
    if (result != 0) return result;
    result = verb_type_diff(entry1, entry2);
    if (result != 0) return result;
    result = entropy_diff(entry1, entry2);
    if (result != 0) return result;
    return strncmp(entry1->word, entry2->word, WORDLE_WORD_LENGTH);
}

// --- EXPORTED COMPARATORS ---

/*
 * FUNCTION: compare_dictionary_entries_by_entropy_desc
 *
 * WHAT:
 * Sorts pointers to dictionary entries.
 * Primary Key: Game State (Valid > Invalid).
 * Secondary Key: Entropy (High > Low).
 * Tertiary Key: Tie-Breaker Chain.
 *
 * WHY:
 * This creates the "Smart View" of the dictionary. The words at the top
 * are the mathematically best guesses that are still valid.
 */
int compare_dictionary_entries_by_entropy_desc(const void* p1, const void* p2)
{
    // Note: p1 is a void* pointer to a pointer to a dictionary_entry_t
    const dictionary_entry_t* entry1 = *(const dictionary_entry_t**)p1;
    const dictionary_entry_t* entry2 = *(const dictionary_entry_t**)p2;

    int result = eliminated_diff(entry1, entry2);
    if (result != 0) return result;

    result = entropy_diff(entry1, entry2);
    if (result != 0) return result;

    return compare_with_entropy_tie_breaker(entry1, entry2);
}

/*
 * FUNCTION: compare_dictionary_entries_by_rank_desc
 *
 * WHAT:
 * Sorts pointers to dictionary entries.
 * Primary Key: Game State (Valid > Invalid).
 * Secondary Key: Frequency Rank (High > Low).
 * Tertiary Key: Tie-Breaker Chain.
 *
 * WHY:
 * This creates the "Common View". Used by hybrid strategies to find words
 * that might not be mathematically perfect but are very likely to be the answer
 * because they are common English words.
 */
int compare_dictionary_entries_by_rank_desc(const void* p1, const void* p2)
{
    const dictionary_entry_t* entry1 = *(const dictionary_entry_t**)p1;
    const dictionary_entry_t* entry2 = *(const dictionary_entry_t**)p2;

    int result = eliminated_diff(entry1, entry2);
    if (result != 0) return result;

    result = rank_diff(entry1, entry2);
    if (result != 0) return result;

    return compare_with_rank_tie_breaker(entry1, entry2);
}

/*
 * FUNCTION: compare_master_entries_eliminated_then_alpha
 *
 * WHAT:
 * Sorts ACTUAL STRUCTURES (not pointers).
 * Primary Key: Game State (Valid > Invalid).
 * Secondary Key: Alphabetical.
 *
 * WHY:
 * This is used to physically rearrange the master `g_p_dictionary` array.
 * By pushing all `is_eliminated=true` words to the end of the array, we can
 * treat the first `N` elements as the "Active Dictionary". This allows us
 * to shrink the loop count in `calculate_entropy` rather than skipping checks.
 */
int compare_master_entries_eliminated_then_alpha(const void* p1, const void* p2)
{
    // Note: p1 is a void* pointer DIRECTLY to the struct, not a double pointer.
    const dictionary_entry_t* entry1 = (const dictionary_entry_t*)p1;
    const dictionary_entry_t* entry2 = (const dictionary_entry_t*)p2;

    if (entry1->is_eliminated && !entry2->is_eliminated) return 1;
    if (!entry1->is_eliminated && entry2->is_eliminated) return -1;

    return strncmp(entry1->word, entry2->word, WORDLE_WORD_LENGTH);
}

/*
 * FUNCTION: compare_dictionary_entries_by_entropy_no_filter_desc
 *
 * WHAT:
 * Sorts pointers based on Entropy, IGNORING the eliminated flag for the primary sort.
 *
 * WHY:
 * Used in "Normal Mode". In Normal Mode, we often want to guess a word that
 * we know is wrong (Eliminated) because it has huge Entropy (e.g. "SLATE").
 * This comparator bubbles those high-value burner words to the top.
 */
int compare_dictionary_entries_by_entropy_no_filter_desc(const void* p1, const void* p2)
{
    const dictionary_entry_t* entry1 = *(const dictionary_entry_t**)p1;
    const dictionary_entry_t* entry2 = *(const dictionary_entry_t**)p2;

    // 1. Primary: Entropy
    int result = entropy_diff(entry1, entry2);
    if (result != 0) return result;

    // 2. Secondary: Priority to Valid Words
    // If entropies are equal (e.g. 0.0 at endgame), we MUST pick the valid word!
    // This prevents the bot from picking a useless burner word when a winner exists.
    result = eliminated_diff(entry1, entry2);
    if (result != 0) return result;

    // 3. Tie-Breakers
    return compare_with_entropy_tie_breaker(entry1, entry2);
}