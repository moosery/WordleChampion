/*
 * FILE: hybrid_strategies.h
 *
 * WHAT:
 * Defines the configuration structures for the "Hybrid" Wordle Bot.
 * This header acts as the blueprint for creating distinct solver personalities.
 * By tweaking these boolean flags and integer thresholds, we can drastically
 * alter the bot's behavior (e.g., from "Greedy Mathematician" to "Safe Linguist"
 * to "Experimental Explorer").
 *
 * WHY:
 * Hardcoding logic makes testing difficult. By extracting these parameters
 * into a data structure, we can define an array of 20+ different bots and
 * race them against each other in the Monte Carlo simulation without writing
 * new code for each one.
 */

#pragma once
#ifndef HYBRID_STRATEGIES_H
#define HYBRID_STRATEGIES_H

#include <stdbool.h>

 /*
  * STRUCT: HybridConfig
  *
  * WHAT:
  * The master configuration object for a single solver instance.
  * Passed into `get_smart_hybrid_guess` to control decision making.
  */
typedef struct
{
    // The display name of the strategy (e.g., "Entropy Linguist").
    // Used in console output and Monte Carlo reports.
    const char* name;

    // --- Base Selection Strategy ---
    // Controls the underlying sorting algorithm used before heuristics are applied.
    // -1: Smart Hybrid (Entropy Sort, but allows logic to override).
    //  0: Entropy Raw (Pure Information Theory).
    //  1: Entropy Filtered (Info Theory + Basic Filters).
    //  2: Rank Raw (Frequency only).
    //  3: Rank Filtered (Frequency + Basic Filters).
    int base_strategy_index;

    // --- Filters & Heuristics ---

    // 1. Linguistic Filter:
    // If true, rejects words that are linguistically unlikely to be answers,
    // specifically Plural Nouns ('P') and Past/3rd Person Verbs ('T', 'S').
    // WHY: Crucial for Hard Mode safety.
    bool use_linguistic_filter;

    // 2. Linguistic Filter Start Turn:
    // Defines when the linguistic filter kicks in.
    // 1 = Strict (Always on). 2 = Skip opener. 3 = Skip first 2 words.
    // WHY: Sometimes we want to use a plural opener (like TARES) for info,
    // even if we know it's not the answer.
    int linguistic_filter_start_turn;

    // 3. Risk Filter:
    // If true, rejects words that don't use known letters (e.g., guessing
    // a word with 1 'E' when we know the answer has 2).
    // WHY: Enforces "Hard Mode" constraints logic even in Normal Mode context.
    bool use_risk_filter;

    // 4. Vowel Bias:
    // If true, prioritizes words with high unique vowel counts in early turns (Turn <= 2).
    // WHY: Helps prevent getting stuck in consonant clusters early on.
    bool prioritize_new_vowels;

    // 5. Anchor Bias:
    // If true, prioritizes words with structural anchors (Terminal Y, Terminal E).
    // WHY: Resolving the end of the word often solves the rest via rhyming.
    bool prioritize_anchors;

    // 6. Vowel Contingency:
    // If true, forces a pivot to a vowel-heavy word if Turn 1 revealed < 2 vowels.
    // WHY: Safety mechanism against "All Black" openers.
    bool prioritize_vowel_contingency;

    // 7. Look Ahead Depth:
    // 0 = Greedy (Standard Entropy).
    // 1 = 1-Step Lookahead (Simulate next turn for top candidates).
    // WHY: Greedy optimization sometimes leads to traps. Lookahead avoids them.
    int look_ahead_depth;

    // 8. Rank Priority Tolerance:
    // Fuzzy Tie-Breaker. If Entropy difference between Best Entropy word and
    // Best Rank word is less than this value (e.g., 0.25), pick the Rank word.
    // WHY: If two words give similar info, pick the one that is a common English word
    // because it has a higher probability of being the actual answer.
    double rank_priority_tolerance;

    // 9. Opener Override:
    // If not NULL, forces the first guess to be this specific word (e.g., "SALET").
    // WHY: Allows testing specific opening theories without changing code.
    const char* opener_override_word;

    // 10. Heatmap Priority:
    // If true, rescores top candidates based on Positional Frequency (5x26 Matrix).
    // WHY: Attempts to guess words that match the "shape" of remaining answers.
    bool use_heatmap_priority;

    // 11. Second Opener Override:
    // If not NULL, forces the *second* guess to be this word (e.g., "COURD").
    // WHY: Implements "Two-Step" strategies where we play 2 fixed words to
    // cover 10 letters immediately.
    const char* second_opener_override_word;

    // 12. Turn 2 Coverage:
    // If true, Turn 2 ignores Entropy and picks the valid word with the most NEW letters.
    // WHY: Maximizes alphabet coverage (Exploration over Exploitation).
    bool prioritize_turn2_coverage;

} HybridConfig;

// --- GLOBAL STRATEGY DEFINITIONS ---
// Defined in hybrid_strategies.cpp
extern const int TOTAL_DEFINED_STRATEGIES;
extern HybridConfig ALL_STRATEGIES[];

#endif