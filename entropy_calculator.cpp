/*
 * FILE: entropy_calculator.cpp
 *
 * WHAT:
 * The mathematical engine of the solver. This file contains the high-performance
 * routines for calculating Shannon Entropy (Information Bits).
 *
 * KEY OPTIMIZATIONS:
 * 1. Integer Encoding: Instead of comparing strings ("GGBYY"), we encode patterns
 * as base-3 integers (0-242). This allows for O(1) array lookups.
 * 2. Stack Allocation: We use fixed-size arrays on the stack for counting
 * patterns, avoiding expensive malloc/free calls in the hot path.
 * 3. OpenMP Parallelism: The outer loops are parallelized to utilize all
 * available CPU cores, reducing calculation time from seconds to milliseconds.
 *
 * WHY:
 * Entropy calculation is the bottleneck. Computing entropy for 5,000 words against
 * 5,000 possible answers involves 25 million comparisons *per turn*.
 * Extreme optimization here is necessary for the "Look Ahead" strategies to run
 * in real-time.
 */

#include "entropy_calculator.h"
#include <memory.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h> // REQUIRED: OpenMP Header for multi-threading

 // CONSTANT: 3^5 = 243 possible patterns (B, Y, G)
 // 0 = Black, 1 = Yellow, 2 = Green
#define MAX_PATTERNS 243 

/*
 * FUNCTION: get_feedback_pattern
 *
 * WHAT:
 * Generates the standard Wordle feedback string (e.g., "GGBYY") for a specific
 * guess against a specific answer.
 *
 * LOGIC:
 * 1. First Pass (Greens): Mark exact matches.
 * 2. Second Pass (Yellows): Mark displaced matches, respecting character counts.
 * (e.g., guessing "SPEED" against "ABIDE" yields only one Yellow 'E',
 * even though "SPEED" has two).
 *
 * WHY:
 * This function is used by the UI and the high-level Game Logic where human-readable
 * strings are required. It is NOT used in the high-performance inner loop.
 */
void get_feedback_pattern(const char* guess, const char* answer, char* result_pattern)
{
    // Initialize pattern to all Black ('B')
    memset(result_pattern, 'B', WORDLE_WORD_LENGTH);
    result_pattern[WORDLE_WORD_LENGTH] = '\0';

    int answer_char_counts[26] = { 0 };

    // 1. First Pass: Greens (Exact Matches)
    // We must identify Greens first so they "consume" the letters in the answer.
    for (int i = 0; i < WORDLE_WORD_LENGTH; i++)
    {
        if (guess[i] == answer[i])
        {
            result_pattern[i] = 'G';
        }
        else
        {
            // If not Green, count this letter in the answer for potential Yellows later
            answer_char_counts[answer[i] - 'A']++;
        }
    }

    // 2. Second Pass: Yellows (Displaced Matches)
    for (int i = 0; i < WORDLE_WORD_LENGTH; i++)
    {
        // Only check positions that aren't already Green
        if (result_pattern[i] != 'G')
        {
            int letter_index = guess[i] - 'A';
            // If the answer still has this letter available (count > 0), mark Yellow
            if (answer_char_counts[letter_index] > 0)
            {
                result_pattern[i] = 'Y';
                answer_char_counts[letter_index]--; // Consume the letter
            }
        }
    }
}

/*
 * FUNCTION: compute_feedback_index
 *
 * WHAT:
 * INTERNAL OPTIMIZATION: Calculates the unique integer index (0-242) for a pattern.
 * Mapping: Black(0), Yellow(1), Green(2).
 * Formula: Index = Sum( value * 3^position )
 *
 * WHY:
 * String manipulation is slow. By converting the feedback pattern into a single
 * integer, we can use it as an index into a histogram array (`counts[idx]++`).
 * This effectively eliminates branching and memory allocation in the entropy loop.
 */
static int compute_feedback_index(const char* guess, const char* answer)
{
    int index = 0;
    int multiplier = 1;

    // State tracking per position: 0=B, 1=Y, 2=G
    int states[5] = { 0 };

    int answer_char_counts[26] = { 0 };

    // 1. First Pass: Greens
    for (int i = 0; i < 5; i++)
    {
        if (guess[i] == answer[i])
        {
            states[i] = 2; // Green
        }
        else
        {
            answer_char_counts[answer[i] - 'A']++;
        }
    }

    // 2. Second Pass: Yellows
    for (int i = 0; i < 5; i++)
    {
        if (states[i] != 2) // If not Green
        {
            int letter_index = guess[i] - 'A';
            if (answer_char_counts[letter_index] > 0)
            {
                states[i] = 1; // Yellow
                answer_char_counts[letter_index]--;
            }
            // Else remains 0 (Black)
        }
    }

    // 3. Convert Base-3 states to Integer
    // Position 0 is LSD (Least Significant Digit)
    for (int i = 0; i < 5; i++)
    {
        index += states[i] * multiplier;
        multiplier *= 3;
    }

    return index;
}

/*
 * FUNCTION: calculate_entropy_internal
 *
 * WHAT:
 * Calculates the Shannon Entropy for a single `guess` against a list of `validAnswers`.
 * Formula: H = -Sum( p(x) * log2(p(x)) )
 * Where x is a feedback pattern, and p(x) is the probability of getting that pattern.
 *
 * WHY:
 * Higher entropy means the guess splits the set of possible answers into smaller,
 * more uniform groups. A guess with 0.0 entropy provides no new information.
 */
static double calculate_entropy_internal(const char* guess, dictionary_entry_t** ppValidAnswers, int numValidAnswers)
{
    if (numValidAnswers <= 1) return 0.0;

    // Optimization: Use a fixed-size array on the stack.
    // This histogram counts how many answers result in each of the 243 patterns.
    int counts[MAX_PATTERNS] = { 0 };

    // 1. Tally pattern frequencies
    for (int i = 0; i < numValidAnswers; i++)
    {
        // Generate the pattern index (0-242) and increment the bucket
        int pattern_idx = compute_feedback_index(guess, ppValidAnswers[i]->word);
        counts[pattern_idx]++;
    }

    // 2. Calculate Shannon Entropy
    double entropy = 0.0;
    const double LOG2_E = 1.44269504089; // log2(e) pre-calculated for speed
    double inv_num = 1.0 / (double)numValidAnswers;

    for (int i = 0; i < MAX_PATTERNS; i++)
    {
        if (counts[i] > 0)
        {
            // p = Probability of this pattern occurring
            double p = counts[i] * inv_num;
            // H -= p * ln(p)
            entropy -= p * log(p);
        }
    }

    return entropy * LOG2_E; // Convert natural log result to base-2 bits
}

/*
 * FUNCTION: calculate_entropy_on_dictionary (Hard Mode Wrapper)
 *
 * WHAT:
 * Calculates entropy for the dictionary assuming Hard Mode constraints
 * (where we usually only guess words that are themselves valid answers).
 *
 * WHY:
 * This wrapper first creates a clean list of valid pointers (removing eliminated words)
 * and then dispatches the calculation to the internal engine, utilizing OpenMP
 * for parallel processing.
 */
void calculate_entropy_on_dictionary(dictionary_entry_t* pDictionary, int dictionaryCount)
{
    // 1. Build a temporary dense list of valid pointers.
    // This creates a contiguous block of memory for the valid words, improving cache performance.
    int validCount = 0;
    for (int i = 0; i < dictionaryCount; ++i)
    {
        if (!pDictionary[i].is_eliminated) validCount++;
    }

    if (validCount == 0) return;

    dictionary_entry_t** ppValid = (dictionary_entry_t**)malloc(sizeof(dictionary_entry_t*) * validCount);
    if (!ppValid) return;

    int idx = 0;
    for (int i = 0; i < dictionaryCount; ++i)
    {
        if (!pDictionary[i].is_eliminated) ppValid[idx++] = &pDictionary[i];
    }

    // 2. Calculate Entropy (Parallelized)
    // We use OpenMP "dynamic" scheduling because some words might finish faster than others.
#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < dictionaryCount; i++)
    {
        // Optimization: Don't calculate entropy for eliminated words.
        // In Hard Mode, we can't play them anyway.
        if (pDictionary[i].is_eliminated)
        {
            pDictionary[i].entropy = 0.0;
        }
        else
        {
            pDictionary[i].entropy = calculate_entropy_internal(pDictionary[i].word, ppValid, validCount);
        }
    }

    free(ppValid);
}

/*
 * FUNCTION: calculate_entropy_for_candidates (Normal Mode Wrapper)
 *
 * WHAT:
 * Calculates entropy for a list of Candidates (all allowed guesses) against
 * a separate list of Valid Answers (remaining solutions).
 *
 * WHY:
 * In Normal Mode, the best guess is often a word that is already eliminated (e.g., "SLATE")
 * but splits the remaining valid words perfectly. We must calculate entropy for
 * the entire `pCandidates` list, not just the valid ones.
 */
void calculate_entropy_for_candidates(dictionary_entry_t* pCandidates, int candidateCount,
    dictionary_entry_t** ppValidAnswers, int validAnswerCount)
{
    // OpenMP Parallel Loop
    // Calculates H(Candidate | ValidAnswers) for every word in the dictionary.
#pragma omp parallel for schedule(static)
    for (int i = 0; i < candidateCount; i++)
    {
        pCandidates[i].entropy = calculate_entropy_internal(pCandidates[i].word, ppValidAnswers, validAnswerCount);
    }
}