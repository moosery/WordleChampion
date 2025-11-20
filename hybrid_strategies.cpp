/*
 * FILE: hybrid_strategies.cpp
 *
 * WHAT:
 * Defines the concrete instances of the Wordle Solver configurations.
 * This file acts as the "Registry" of all available bot personalities.
 *
 * THE ROSTER (19 Strategies):
 * 0.  Entropy Linguist (Strict) [THE CHAMPION] - Undefeated, 100% Win Rate.
 * 1.  Entropy Raw               - Pure Math, no Linguistic filters.
 * 2.  Legacy Reborn             - Smart Hybrid with Rank Bias.
 * 3.  Vowel Hunter (Audio)      - Forces "AUDIO" opener.
 * 4.  Vowel Hunter (Adieu)      - Forces "ADIEU" opener.
 * 5.  Vowel Contingency         - Pivots if opener fails to find vowels.
 * 6.  Pattern Hunter            - Prioritizes structural anchors (Y/E).
 * 7.  Progressive (Skip T1)     - Delays linguistic filter to Turn 2.
 * 8.  Progressive (Skip T1-2)   - Delays linguistic filter to Turn 3.
 * 9.  Look Ahead (Pruned)       - Simulation-based decision making.
 * 10. Entropy Filtered          - Hard Mode simulation using Filtered candidates.
 * 11. Rank Raw                  - Frequency-based guessing (dumb).
 * 12. Rank Filtered             - Frequency-based guessing with filters.
 * 13. Hybrid Apex (Strict)      - Combines Linguistics + Rank Bias (Failed: 0.25 tol).
 * 14. Deep Linguist             - Linguistics + Look Ahead + Safety Clamp.
 * 15. Hybrid Apex II (Safe)     - Linguistics + Look Ahead + Rank Bias (0.10 tol).
 * 16. Heatmap Seeker            - Positional Frequency priority (Failed).
 * 17. Dynamic Two-Step          - Coverage maximization on Turn 2 (Failed).
 * 18. Double Barrel             - Forces "SALET" then "COURD" (Fixed Opener).
 *
 * WHY:
 * By keeping all historical configurations in this array, we can easily
 * re-run tournaments or regression tests to verify that a logic change
 * in solver_logic.cpp hasn't inadvertently broken an older strategy.
 */

#include "hybrid_strategies.h"
#include <stddef.h> 

 // GLOBAL: Total number of defined strategies
 // Used by the Monte Carlo runner to iterate or select strategies.
const int TOTAL_DEFINED_STRATEGIES = 19;

/*
 * CONFIGURATION ARRAY:
 * Order of fields in HybridConfig struct:
 * 1.  Name (string)
 * 2.  Base Index (-1=Smart, 0=EntRaw, 1=EntFilt, 2=RankRaw, 3=RankFilt)
 * 3.  Use Linguistic Filter (bool)
 * 4.  Linguistic Start Turn (int)
 * 5.  Use Risk Filter (bool)
 * 6.  Prioritize New Vowels (bool)
 * 7.  Prioritize Anchors (bool)
 * 8.  Prioritize Vowel Contingency (bool)
 * 9.  Look Ahead Depth (int: 0 or 1)
 * 10. Rank Priority Tolerance (double)
 * 11. Opener Override (string or NULL)
 * 12. Use Heatmap Priority (bool)
 * 13. Second Opener Override (string or NULL)
 * 14. Prioritize Turn 2 Coverage (bool)
 */
HybridConfig ALL_STRATEGIES[] = {

    // --- THE CHAMPION ---
    // "Entropy Linguist (Strict)"
    // Logic: Pure Entropy + Linguistic Filter (No Plurals/Past Tense).
    // Result: 100.00% Win Rate. Lowest risk profile.
    /* 0 */ { "Entropy Linguist (Strict)",  -1, true, 1, false, false, false, false, 0, 0.0, NULL, false, NULL, false },

    // --- BASELINE CONTROLS ---
    // "Entropy Raw"
    // Logic: Pure Math. Guesses plurals like "TARES" or "SOARE".
    // Result: High win rate, but occasional losses due to traps.
    /* 1 */ { "Entropy Raw (Baseline)",     0, false, 99, false, false, false, false, 0, 0.0, NULL, false, NULL, false },

    // "Legacy Reborn"
    // Logic: The original 'Smart' hybrid with heavy rank bias (0.50).
    /* 2 */ { "Legacy Reborn (Smart)",     -1, true, 1, true,  false, false, false, 0, 0.50, NULL, false, NULL, false },

    // --- VOWEL OPENERS ---
    // Testing the popular "Vowel Heavy" starting words.
    /* 3 */ { "Vowel Hunter (Audio)",      -1, true, 1, false, true,  false, false, 0, 0.0, "AUDIO", false, NULL, false },
    /* 4 */ { "Vowel Hunter (Adieu)",      -1, true, 1, false, true,  false, false, 0, 0.0, "ADIEU", false, NULL, false },

    // --- HEURISTIC EXPERIMENTS ---
    // "Vowel Contingency"
    // Logic: If Turn 1 finds < 2 vowels, force a vowel hunt on Turn 2.
    /* 5 */ { "Vowel Contingency",         -1, true, 1, false, false, false, true,  0, 0.0, NULL, false, NULL, false },

    // "Pattern Hunter"
    // Logic: Prioritizes words ending in 'Y' or 'E' to resolve structure early.
    /* 6 */ { "Pattern Hunter (Anchor)",   -1, true, 1, false, false, true,  false, 0, 0.0, NULL, false, NULL, false },

    // "Progressive"
    // Logic: Allow "Bad" words (Plurals) on Turn 1/2 to get info, then switch to Strict.
    /* 7 */ { "Progressive (Skip T1)",     -1, true, 2, false, false, false, false, 0, 0.0, NULL, false, NULL, false },
    /* 8 */ { "Progressive (Skip T1-2)",   -1, true, 3, false, false, false, false, 0, 0.0, NULL, false, NULL, false },

    // "Look Ahead"
    // Logic: Simulates the next turn to find the best split.
    // Result: Lowest average guesses (3.7632) but suffered 2 losses without Safety Clamp.
    /* 9 */ { "Look Ahead (Pruned)",       -1, true, 1, false, false, false, false, 1, 0.0, NULL, false, NULL, false },

    // --- SIMPLE SORTING STRATEGIES ---
    /* 10 */ { "Entropy Filtered",          1, false, 99, false, false, false, false, 0, 0.0, NULL, false, NULL, false },
    /* 11 */ { "Rank Raw",                  2, false, 99, false, false, false, false, 0, 0.0, NULL, false, NULL, false },
    /* 12 */ { "Rank Filtered",             3, false, 99, false, false, false, false, 0, 0.0, NULL, false, NULL, false },

    // --- ADVANCED HYBRIDS ---
    // "Hybrid Apex"
    // Logic: Strict Linguistics + Look Ahead + Aggressive Rank Bias (0.25).
    // Result: FAILED. Too much rank bias caused it to pick common traps.
    /* 13 */ { "Hybrid Apex (Strict)",     -1, true, 1, true,  false, false, true,  1, 0.25, NULL, false, NULL, false },

    // "Deep Linguist"
    // Logic: Strict Linguistics + Look Ahead. No Rank Bias.
    // Result: Excellent speed, but requires "Endgame Clamp" to be safe.
    /* 14 */ { "Deep Linguist",            -1, true, 1, false, false, false, false, 1, 0.0, NULL, false, NULL, false },

    // "Hybrid Apex II"
    // Logic: Strict Linguistics + Look Ahead + Conservative Rank Bias (0.10).
    /* 15 */ { "Hybrid Apex II (Safe)",    -1, true, 1, false, false, false, false, 1, 0.10, NULL, false, NULL, false },

    // "Heatmap Seeker"
    // Logic: Prioritizes words that fit the positional frequency distribution (Matrix).
    // Result: FAILED. Susceptible to "Green Traps" (Silos) in Hard Mode.
    /* 16 */ { "Heatmap Seeker",           -1, true, 1, false, false, false, false, 0, 0.0, NULL, true,  NULL, false },

    // --- COVERAGE STRATEGIES ---
    // "Dynamic Two-Step"
    // Logic: Turn 2 prioritizes New Letters over Entropy.
    // Result: FAILED. Maximizing coverage is inferior to maximizing entropy split.
    /* 17 */ { "Dynamic Two-Step (Coverage)", -1, true, 1, false, false, false, false, 0, 0.0, NULL, false, NULL, true },

    // "Double Barrel"
    // Logic: Forces "SALET" then "COURD" to cover 10 unique letters.
    // Result: Preserved for archival purposes.
    /* 18 */ { "Double Barrel (Salet/Courd)", -1, true, 1, false, false, false, false, 0, 0.0, "SALET", false, "COURD", false }
};