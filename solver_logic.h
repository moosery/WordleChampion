/*
 * FILE: solver_logic.h
 *
 * WHAT:
 * Defines the interface for the core game logic and decision-making engine.
 * This module is responsible for:
 * 1. Processing constraints (Green/Yellow/Gray letters).
 * 2. Filtering the dictionary based on those constraints.
 * 3. Selecting the best next guess based on the active Hybrid Strategy.
 *
 * WHY:
 * Separation of concerns. The Main loop handles the flow, but this module
 * handles the "Thinking." It isolates the complex heuristics (Look Ahead,
 * Vowel Bias, Risk Filters) from the UI and Data layers.
 */

#pragma once
#ifndef SOLVER_LOGIC_H
#define SOLVER_LOGIC_H
#include "wordle_types.h"
#include "comparators.h"
#include "hybrid_strategies.h" 

 /*
  * CONSTANT: Max Recommendations
  *
  * WHAT:
  * The number of distinct "best guess" categories we track for the UI display.
  * 0: Entropy Raw, 1: Entropy Filtered, 2: Rank Raw, 3: Rank Filtered.
  */
const int MAX_RECOMMENDATIONS = 4;

/*
 * TYPE: recommendations_array_t
 *
 * WHAT:
 * An array of candidate structures used to populate the UI suggestions box.
 */
typedef word_candidate_t recommendations_array_t[MAX_RECOMMENDATIONS];

// --- Core Logic Interface ---

/*
 * FUNCTION: update_min_required_counts
 *
 * WHAT:
 * Updates the persistent tracking of letter counts based on feedback.
 * Example: If we get a Green 'E' and a Yellow 'E', we know the target
 * word must contain at least two 'E's.
 *
 * WHY:
 * Used by the "Risk Filter". If a candidate word has only one 'E', but
 * we know we need two, it is a risky/invalid guess (in Hard Mode context).
 */
void update_min_required_counts(const char* guess, const char* result_pattern, int* min_required_counts);

/*
 * FUNCTION: get_smart_hybrid_guess
 *
 * WHAT:
 * The "Brain" of the solver. It takes the current sorted views of the
 * dictionary and applies the active Strategy Configuration (heuristics,
 * look-ahead, linguistic filters) to return the single best guess.
 *
 * WHY:
 * This consolidates all strategy logic into one entry point. Whether
 * we are doing a simple Entropy scan or a complex multi-turn simulation,
 * the main loop simply calls this function.
 */
const dictionary_entry_t* get_smart_hybrid_guess(
    const dictionary_pointer_array_t p_entropy_sorted,
    const dictionary_pointer_array_t p_rank_sorted,
    int count,
    const HybridConfig* config,
    const int* min_required_counts,
    int valid_count,
    int turn
);

/*
 * FUNCTION: get_best_guess_candidates
 *
 * WHAT:
 * Identifies the top candidates for the four standard categories:
 * 1. Entropy Raw (Best mathematical split).
 * 2. Entropy Filtered (Best split + Linguistic validity).
 * 3. Rank Raw (Most common word).
 * 4. Rank Filtered (Most common + Linguistic validity).
 *
 * WHY:
 * Used primarily for the Interactive UI to show the user the "Top Choices"
 * box, helping them understand the trade-offs between different moves.
 */
bool get_best_guess_candidates(
    const dictionary_pointer_array_t p_entropy_sorted,
    const dictionary_pointer_array_t p_rank_sorted,
    int count,
    recommendations_array_t candidates
);

/*
 * FUNCTION: filter_dictionary_by_constraints
 *
 * WHAT:
 * Scans the dictionary and marks entries as "eliminated" (is_eliminated = true)
 * if they conflict with the latest feedback (guess + pattern).
 *
 * WHY:
 * This is the mechanism that narrows the search space. After every turn,
 * impossible words are flagged so they are ignored in future entropy calculations.
 */
void filter_dictionary_by_constraints(
    dictionary_entry_t* p_dictionary,
    int count,
    const char* guess,
    const char* result_pattern
);

#endif