/*
 * FILE: load_dictionary.cpp
 *
 * WHAT:
 * Implements the data ingestion pipeline.
 * This module is responsible for reading the raw dictionary text file, parsing
 * the specific fixed-width fields (Word, Rank, Noun/Verb types), and populating
 * the in-memory data structures.
 *
 * CRITICAL DEPENDENCIES:
 * - load_used_words: We must know which words have already been answers to
 * exclude them (or mark them) depending on the configuration.
 * - entropy_calculator: We pre-calculate the entropy of every word immediately
 * upon loading. This is a heavy one-time cost (seconds) that saves massive
 * time during the simulation loops (milliseconds).
 *
 * WHY:
 * A robust loader is essential for stability. This file handles the dirty work
 * of file I/O, string trimming, and error checking so the rest of the application
 * can assume clean, valid data.
 */

#include "load_dictionary.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "entropy_calculator.h"

 /*
  * FUNCTION: trim
  *
  * WHAT:
  * Modifies a string in-place to remove trailing whitespace (spaces, tabs, newlines).
  *
  * WHY:
  * Files read via `fgets` often include the newline character (`\n`) at the end.
  * If not removed, this invisible character disrupts string comparisons and length
  * checks throughout the application.
  */
static void trim(char* str)
{
    char* ptrToWhereNullCharShouldGo = str;
    char* currentPtr = str;
    while (*currentPtr != '\0')
    {
        char ch = *currentPtr++;
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
        {
            ptrToWhereNullCharShouldGo = currentPtr;
        }
    }
    *ptrToWhereNullCharShouldGo = '\0';
}

/*
 * FUNCTION: contains_duplicate_letter
 *
 * WHAT:
 * Scans a 5-letter word to detect if any character appears more than once.
 * Returns true if duplicates exist (e.g., "APPLE"), false if unique (e.g., "WORLD").
 *
 * WHY:
 * This is a pre-computation step. The "Entropy Filtered" strategy relies on
 * prioritizing words with unique letters to maximize information spread.
 * Calculating this once at load time is O(1) lookup later, versus O(N) every
 * time we evaluate a guess.
 */
bool contains_duplicate_letter(const char* word)
{
    bool letterSeen[26] = { false };
    for (int i = 0; i < WORDLE_WORD_LENGTH; i++)
    {
        char ch = word[i];
        if (ch < 'A' || ch > 'Z')
        {
            continue;
        }
        int index = ch - 'A';
        if (letterSeen[index])
        {
            return true;
        }
        letterSeen[index] = true;
    }
    return false;
}

/*
 * FUNCTION: load_dictionary
 *
 * WHAT:
 * The main data loader.
 * 1. Loads the list of "Used Words" (past Wordle answers) IF requested.
 * 2. Opens the master "AllWords.txt" file.
 * 3. Iterates through every line:
 * - Skips words found in the "Used Words" list (if filtering is on).
 * - Parses the Word, Rank, and Linguistic Tags.
 * - Pre-calculates metadata (duplicates, initial entropy).
 *
 * PARAMETERS:
 * - pp_dictionary: Output parameter. Will point to the newly allocated array.
 * - p_dictionary_count: Output parameter. Will hold the number of loaded words.
 * - should_filter_history: If true, connects to web to download used words.
 *
 * RETURNS:
 * - true if successful, false if memory allocation or file I/O fails.
 *
 * WHY:
 * This function transforms the raw text data on disk into the structured
 * `dictionary_entry_t` objects that the solver engine requires. It effectively
 * "hydrates" the application state.
 */
bool load_dictionary(dictionary_entry_t** pp_dictionary, int* p_dictionary_count, bool should_filter_history)
{
    // 1. Load the "Used Words" list (Optional)
    // Only perform the web download if the user specifically wants to filter history.
    // Otherwise, we treat the dictionary as a "Fresh Universe".
    if (should_filter_history)
    {
        if (!load_used_words(&g_p_used_words, &g_used_word_count))
        {
            printf("Warning: Failed to load used words. Continuing with full dictionary.\n");
            // We don't return false here; we just degrade gracefully to full dictionary mode.
            should_filter_history = false;
        }
    }

    char* pTmp = g_p_used_words;
    FILE* fpIn;
    errno_t errval;
    char buffer[100];
    *p_dictionary_count = 0;
    int total_loaded_words = 0;

    // Pointers for linear scan filtering
    char* pNextSortedUsedWord = g_p_used_words;
    int numLeftInUsedWords = g_used_word_count;

    // Allocate the Master Dictionary Array
    *pp_dictionary = (dictionary_entry_t*)malloc(sizeof(dictionary_entry_t) * MAX_DICTIONARY_WORDS);
    if (pp_dictionary == NULL)
    {
        fprintf(stderr, "Out of memory allocating dictionary!\n");
        return false;
    }

    // Open the Master Data File (Hardcoded path for this environment)
    errval = fopen_s(&fpIn, "C:\\VS2022.Projects\\StuffForWordle\\WordleWordsCSVs\\AllWords.txt", "r");

    if (fpIn == NULL || errval != 0)
    {
        fprintf(stderr, "Could not open consolidated dictionary file (AllWords.txt)! Check the hardcoded path.\n");
        free(*pp_dictionary);
        return false;
    }

    // 2. Parse the File Line by Line
    while (fgets(buffer, sizeof(buffer), fpIn) != NULL && *p_dictionary_count < MAX_DICTIONARY_WORDS)
    {
        trim(buffer);

        // Ensure line has minimal expected length (Word + Rank + Tags)
        if (strlen(buffer) >= 10)
        {
            total_loaded_words++;

            // Filter: Check if this word is in the Used Words list.
            // We only perform this check if the flag is set AND we have words left to check.
            if (should_filter_history && numLeftInUsedWords > 0 && strncmp(buffer, pNextSortedUsedWord, WORDLE_WORD_LENGTH) == 0)
            {
                pNextSortedUsedWord += WORDLE_WORD_LENGTH;
                numLeftInUsedWords--;
                continue; // Skip this word, it has already been used.
            }

            // Get pointer to the next free slot in our array
            dictionary_entry_t* pEntry = (*pp_dictionary) + (*p_dictionary_count);

            // Parse Word (Offsets 0-4)
            for (int i = 0; i < WORDLE_WORD_LENGTH; i++)
            {
                pEntry->word[i] = toupper((unsigned char)(*(buffer + i)));
            }
            pEntry->word[WORDLE_WORD_LENGTH] = '\0';

            // Parse Rank (Offsets 5-7)
            char rankStr[4];
            memcpy(rankStr, buffer + 5, 3);
            rankStr[3] = '\0';
            pEntry->frequency_rank = atoi(rankStr);

            // Parse Tags (Offsets 8 and 9)
            pEntry->noun_type = buffer[8];
            pEntry->verb_type = buffer[9];

            // Pre-calculate Metadata
            pEntry->contains_duplicate_letters = contains_duplicate_letter(pEntry->word);
            pEntry->entropy = 0.0;         // Will be calculated shortly
            pEntry->is_eliminated = false; // Default state: Valid

            (*p_dictionary_count)++;
        }
    }

    fclose(fpIn);

    printf("Loaded %d words from the new consolidated dictionary.\n", *p_dictionary_count);
    if (should_filter_history)
    {
        printf("Filtered out %d used words from %d loaded.  Did not find %d used words.\n", g_used_word_count, total_loaded_words, numLeftInUsedWords);
    }
    else
    {
        printf("History Filter DISABLED. All %d words are active.\n", total_loaded_words);
    }

    // 3. Initial Entropy Calculation
    // This is expensive! We do it once at startup so we don't have to do it
    // for the very first turn of every game.
    printf("Calculating entropy for each word in the dictionary...");
    fflush(stdout);
    calculate_entropy_on_dictionary(*pp_dictionary, *p_dictionary_count);
    printf(" Done.\n");
    fflush(stdout);

    return true;
}