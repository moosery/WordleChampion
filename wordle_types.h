/*
 * FILE: wordle_types.h
 *
 * WHAT:
 * Defines the core data structures and types used throughout the application.
 * This includes the main dictionary entry definition, global constants, and
 * helper types for sorting and pointers.
 *
 * WHY:
 * A centralized type definition ensures consistency across the Logic, Data,
 * and Strategy layers. It also defines the "Global Master Dictionary" pointers
 * which are accessed by the Monte Carlo simulation and Main loop.
 */

#pragma once
#ifndef WORDLE_TYPES_H
#define WORDLE_TYPES_H 

#include <stdlib.h>
#include <stdbool.h> 

/*
 * CONSTANTS: Game Constraints
 *
 * WHAT:
 * Defines the physical limits of the game and memory allocation.
 *
 * WHY:
 * WORDLE_WORD_LENGTH is set to 5 for standard Wordle.
 * MAX_DICTIONARY_WORDS creates a safe upper bound for static array allocations
 * if needed, though most heavy lifting is done via dynamic malloc.
 */
const int WORDLE_WORD_LENGTH = 5;
const int MAX_DICTIONARY_WORDS = 10000; 


/*
 * STRUCT: dictionary_entry_t
 *
 * WHAT:
 * Represents a single word in the dictionary and all its associated metadata.
 *
 * WHY:
 * Instead of parallel arrays (one for words, one for rank, one for entropy),
 * we bundle everything into a single struct. This improves cache locality
 * during sorting and filtering.
 *
 * FIELD DOMAIN VALUES:
 *
 * 1. noun_type (char):
 * - 'P' : Plural Noun    (e.g., "COOKS", "CAKES")
 * - 'S' : Singular Noun  (e.g., "BREAD", "CAKE")
 * - 'N' : Not a Noun     (e.g., "GROPE", "THERE")
 * - 'R' : Pronoun        (e.g., "YOURS", "THEIR", "WHOSE")
 *
 * 2. verb_type (char):
 * - 'T' : Past Tense     (e.g., "BAKED", "COOKED")
 * - 'S' : Third Person   (e.g., "BAKES", "COOKS")
 * - 'P' : Present Tense  (e.g., "GROPE", "BAKE")
 * - 'N' : Not a Verb     (e.g., "ZEBRA", "APPLE")
 *
 * 3. frequency_rank (int):
 * - 100 : Highest Frequency (Very common words like "THEIR", "WHICH")
 * - 000 : Lowest Frequency  (Obscure words like "VOZHD", "XYLYL")
 * - Range: 000 to 100 inclusive.
 *
 * 4. is_eliminated (bool):
 * - true  : The word has been ruled out by game logic.
 * - false : The word is still a valid potential answer.
 */
typedef struct _dictionary_entry
{
    char word[WORDLE_WORD_LENGTH + 1];  /* The five character word + null terminator   */
    double entropy;                     /* The entropy value of the word (Calculated)  */
    int frequency_rank;                 /* Higher values indicate higher frequency     */
                                        /* Values range from 000 to 100                */
    char noun_type;                     /* See Domain Values above ('P','S','N','R')   */
    char verb_type;                     /* See Domain Values above ('T','S','P','N')   */
    bool contains_duplicate_letters;    /* true if the word contains duplicate letters */
    bool is_eliminated;                 /* true if word is ruled out by Hard Mode rule */
} dictionary_entry_t;

/*
 * TYPE: dictionary_pointer_array_t
 *
 * WHAT:
 * An alias for an array of pointers to dictionary entries.
 *
 * WHY:
 * We often need multiple "Views" of the same dictionary (e.g., one sorted by
 * Entropy, one sorted by Rank). Instead of copying the bulky `dictionary_entry_t`
 * data, we create lightweight arrays of pointers and sort those.
 */
typedef dictionary_entry_t** dictionary_pointer_array_t;

/*
 * STRUCT: word_candidate_t
 *
 * WHAT:
 * A display wrapper used by the UI/Recommendation engine.
 *
 * WHY:
 * Associates a specific Strategy Label (e.g., "Entropy Raw") with a specific
 * dictionary entry pointer. Used to print the "Aligned Box" in the UI.
 */
typedef struct _word_candidate
{
    const char* label; 
    const dictionary_entry_t* pEntry;
} word_candidate_t;


/*
 * GLOBALS: The Master Dictionary
 *
 * WHAT:
 * Holds the single source of truth for the dictionary data loaded from disk.
 *
 * WHY:
 * These are defined here as `extern` so they can be shared between `main.cpp`,
 * `load_dictionary.cpp`, and `monte_carlo.cpp`. `g_p_dictionary` holds the
 * raw data block, while `g_dictionary_word_count` tracks its size.
 */
#ifdef MAIN
int g_dictionary_word_count = 0;               /* The number of words in the dictionary       */
dictionary_entry_t* g_p_dictionary = NULL;     /* The dictionary (MASTER COPY)                */
#else
extern int g_dictionary_word_count;            /* The number of words in the dictionary       */
extern dictionary_entry_t* g_p_dictionary;     /* The dictionary (MASTER COPY)                */
#endif

#endif