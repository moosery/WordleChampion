/*
 * FILE: entropy_calculator.h
 *
 * WHAT:
 * Defines the interface for the mathematical engine of the Wordle solver.
 * This module handles the core Information Theory calculations (Shannon Entropy)
 * and the mechanics of generating feedback patterns (Green/Yellow/Gray).
 *
 * WHY:
 * This is the "Calculator" component. It doesn't know about strategy or game
 * state; it simply crunches numbers. It is separated to allow the logic layer
 * to request heavy computations without polluting the decision-making code.
 */

#pragma once
#ifndef ENTROPY_CALCULATOR_H
#define ENTROPY_CALCULATOR_H
#include "wordle_types.h"

 /*
  * FUNCTION: get_feedback_pattern
  *
  * WHAT:
  * Generates the standard Wordle feedback string (e.g., "GGBYY") given a
  * guess and a target answer.
  *
  * WHY:
  * Used by the UI and the filtering logic to simulate game turns. It implements
  * the specific rules of Wordle coloring, including the tricky handling of
  * multiple instances of the same letter.
  */
void get_feedback_pattern(const char* guess, const char* answer, char* result_pattern);

/*
 * FUNCTION: calculate_entropy_on_dictionary
 *
 * WHAT:
 * Calculates the Shannon Entropy for every word in the provided dictionary
 * array. It assumes the candidate pool and the answer pool are the same
 * (Standard Hard Mode scenario).
 *
 * WHY:
 * This updates the `entropy` field of each `dictionary_entry_t`. Higher entropy
 * means the word is statistically more likely to split the remaining possibilities
 * into smaller groups.
 */
void calculate_entropy_on_dictionary(dictionary_entry_t* pDictionary, int dictionaryCount);

/*
 * FUNCTION: calculate_entropy_for_candidates
 *
 * WHAT:
 * A specialized entropy calculation for "Normal Mode".
 * - pCandidates: The list of words we can GUESS (often the full dictionary).
 * - ppValidAnswers: The subset of words that could actually BE the answer.
 *
 * WHY:
 * In Normal Mode, the best guess is often a word that cannot be the answer
 * but provides massive information about the valid set. This function allows
 * us to calculate the utility of *all* words against the *subset* of remaining answers.
 */
void calculate_entropy_for_candidates(dictionary_entry_t* pCandidates, int candidateCount,
    dictionary_entry_t** ppValidAnswers, int validAnswerCount);

#endif