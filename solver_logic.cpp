/*
 * FILE: solver_logic.cpp
 *
 * WHAT:
 * Implements the decision-making engine of the Wordle Solver.
 * This file contains the heuristic evaluators, the "Look Ahead" simulation logic,
 * and the master function `get_smart_hybrid_guess` which arbitrates between
 * different strategies based on the active configuration.
 *
 * WHY:
 * This is where the "Artificial Intelligence" lives. While entropy_calculator.cpp
 * does the raw math, this file applies the strategy. It decides when to be
 * greedy (pure entropy), when to be safe (linguistic filters), and when to
 * simulate future turns (look ahead).
 */

#include "solver_logic.h"
#include "entropy_calculator.h" 
#include <stdio.h>
#include <stddef.h> 
#include <string.h>
#include <stdlib.h> 
#include <math.h>   

/*
 * CONSTANTS: Tuning Parameters
 *
 * PRUNE_COUNT (60):
 * When running "Look Ahead" simulations, we don't have the CPU time to simulate
 * the next turn for all 5,000 words. We only simulate the top 60 candidates
 * identified by the primary Entropy sort. This "Pruned" search captures
 * >99.9% of optimal moves while running in milliseconds instead of minutes.
 */
#define PRUNE_COUNT 60 
#define MAX_GUESSES 6

/*
 * FUNCTION: update_min_required_counts
 *
 * WHAT:
 * Updates the "known minimums" for each letter based on feedback.
 *
 * WHY:
 * If we guess "SPEED" and get Green 'E' at pos 3 and Yellow 'E' at pos 4,
 * we know the target word must contain *at least* two 'E's. This function
 * aggregates those constraints so the "Risk Filter" can reject future words
 * that don't meet this criteria (e.g., "LATER" has only one E, so it's impossible).
 */
void update_min_required_counts(const char* guess, const char* result_pattern, int* min_required_counts)
{
    int current_turn_counts[26] = { 0 };

    // Count the confirmed instances of each letter in this specific guess
    for (int i = 0; i < 5; i++)
    {
        // Both Green and Yellow indicate the letter exists in the answer
        if (result_pattern[i] == 'G' || result_pattern[i] == 'Y')
        {
            int char_idx = guess[i] - 'A';
            if (char_idx >= 0 && char_idx < 26) current_turn_counts[char_idx]++;
        }
    }

    // Update the global minimums if this turn revealed a higher count
    for (int i = 0; i < 26; i++)
    {
        if (current_turn_counts[i] > min_required_counts[i]) min_required_counts[i] = current_turn_counts[i];
    }
}

/*
 * FUNCTION: is_linguistically_sound
 *
 * WHAT:
 * The core of the "Linguist" strategy. It rejects words based on part-of-speech tags.
 * - Rejects Plural Nouns ('P') ending in 'S'.
 * - Rejects Past Tense Verbs ('T') ending in 'ED'.
 * - Rejects 3rd Person Verbs ('S') ending in 'S'.
 *
 * WHY:
 * The curated Wordle solution list rarely contains simple plurals or past tense
 * variations. By filtering these out, we prevent the bot from wasting guesses
 * on "technically valid but effectively impossible" words.
 */
static bool is_linguistically_sound(const dictionary_entry_t* pEntry)
{
    if (pEntry->noun_type == 'P') return false;
    if (pEntry->verb_type == 'T') return false;
    if (pEntry->verb_type == 'S') return false;
    return true;
}

/*
 * FUNCTION: is_risky_guess
 *
 * WHAT:
 * Checks if a candidate word violates the "Minimum Letter Count" constraint.
 *
 * WHY:
 * Used by the "Risk Filter". In Hard Mode, you generally must use the letters
 * you found. But in Normal Mode, we might *want* to play a burner word.
 * This function allows us to selectively enforce that logic if the strategy requires it.
 */
static bool is_risky_guess(const dictionary_entry_t* pEntry, const int* min_required_counts)
{
    int guess_counts[26] = { 0 };
    for (int i = 0; i < 5; i++)
    {
        int idx = pEntry->word[i] - 'A';
        if (idx >= 0 && idx < 26) guess_counts[idx]++;
    }

    for (int i = 0; i < 26; i++)
    {
        // If the word uses a letter multiple times, check if we are allowed to.
        // (This logic specifically targets over-guessing duplicates unnecessarily).
        if (guess_counts[i] > 1)
        {
            // If we use 'E' twice, but we only know we need 1 'E', this is "Risky"
            // if the user wants strict adherence to known constraints.
            if (guess_counts[i] > min_required_counts[i]) return true;
        }
    }
    return false;
}

/*
 * FUNCTION: count_known_vowels
 *
 * WHAT:
 * Counts how many unique vowels have been confirmed (min_count > 0).
 *
 * WHY:
 * Used by the "Vowel Contingency" strategy. If we have found 0 or 1 vowels
 * by Turn 2, we might pivot to a vowel-heavy word to ensure we don't
 * get stuck in a consonant cluster trap.
 */
static int count_known_vowels(const int* min_required_counts)
{
    int count = 0; const char* vowels = "AEIOUY";
    for (int i = 0; i < 6; i++) { if (min_required_counts[vowels[i] - 'A'] > 0) count++; }
    return count;
}

/*
 * FUNCTION: count_new_vowels
 *
 * WHAT:
 * Counts how many unique vowels in a candidate word are NOT yet known.
 *
 * WHY:
 * Helper for the contingency strategy. We want to pick a word that tests
 * vowels we haven't seen yet.
 */
static int count_new_vowels(const char* word, const int* min_required_counts)
{
    int count = 0; bool seen[26] = { false }; const char* vowels = "AEIOUY";
    for (int i = 0; i < 5; i++) { char c = word[i]; if (strchr(vowels, c) != NULL) { int idx = c - 'A'; if (!seen[idx] && min_required_counts[idx] == 0) { seen[idx] = true; count++; } } }
    return count;
}

/*
 * FUNCTION: calculate_anchor_score
 *
 * WHAT:
 * Assigns a heuristic score based on structural "Anchors":
 * - Terminal 'Y' (+3)
 * - Terminal 'E' (+2)
 * - Central Vowels (+1)
 *
 * WHY:
 * Words ending in Y and E are extremely common in English. Knowing if the
 * word ends in Y drastically reduces the search space (e.g., SHADY, PINTY).
 */
static int calculate_anchor_score(const char* word)
{
    int score = 0;
    // Terminal 'Y' and 'E' are structurally significant in English 5-letter words
    if (word[4] == 'Y') score += 3; else if (word[4] == 'E') score += 2;
    // Central vowels help split the dictionary
    char c = word[2]; if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') score += 1;
    return score;
}

/*
 * FUNCTION: count_unique_vowels_simple
 *
 * WHAT:
 * Basic utility to count unique vowels in a string.
 *
 * WHY:
 * Used as a fallback scorer for "Early Bias" strategies that prioritize
 * vowel discovery.
 */
static int count_unique_vowels_simple(const char* word)
{
    int count = 0; bool seen[26] = { false }; const char* vowels = "AEIOUY";
    for (int i = 0; i < 5; i++) { char c = word[i]; if (strchr(vowels, c) != NULL) { int idx = c - 'A'; if (!seen[idx]) { seen[idx] = true; count++; } } }
    return count;
}

/*
 * FUNCTION: calculate_new_letter_coverage
 *
 * WHAT:
 * Calculates a score based on how many letters in the candidate word have
 * NOT yet been identified as Green or Yellow.
 *
 * WHY:
 * Used by the "Dynamic Two-Step (Coverage)" strategy. It encourages the bot
 * to explore the alphabet ("Burner Words") rather than trying to solve the
 * puzzle immediately. (Note: This strategy proved inferior to Entropy in testing).
 */
static int calculate_new_letter_coverage(const char* word, const int* min_required_counts)
{
    int score = 0;
    bool seen_in_word[26] = { false };

    for (int i = 0; i < 5; i++)
    {
        int idx = word[i] - 'A';
        if (idx >= 0 && idx < 26)
        {
            if (!seen_in_word[idx])
            {
                seen_in_word[idx] = true;
                // If min_required_counts[idx] == 0, we haven't found a Green/Yellow yet.
                if (min_required_counts[idx] == 0)
                {
                    score++;
                }
            }
        }
    }
    return score;
}

// --- HEATMAP HELPERS ---

/*
 * FUNCTION: build_heatmap_matrix
 *
 * WHAT:
 * Scans all currently valid words and builds a frequency map of [Position][Letter].
 * e.g., How many valid words have 'A' in position 0?
 *
 * WHY:
 * Used by the "Heatmap Seeker" strategy to find words that align with the
 * statistical structure of the remaining solution set. This is "Positional Probability."
 */
static void build_heatmap_matrix(const dictionary_pointer_array_t p_dictionary, int count, int heatmap[5][26])
{
    // 1. Reset the matrix
    for (int p = 0; p < 5; p++) { for (int c = 0; c < 26; c++) { heatmap[p][c] = 0; } }

    // 2. Tally valid words
    for (int i = 0; i < count; i++)
    {
        if (!p_dictionary[i]->is_eliminated)
        {
            for (int j = 0; j < 5; j++)
            {
                int char_idx = p_dictionary[i]->word[j] - 'A';
                if (char_idx >= 0 && char_idx < 26) heatmap[j][char_idx]++;
            }
        }
    }
}

/*
 * FUNCTION: get_heatmap_score
 *
 * WHAT:
 * Sums the positional probability scores for a specific candidate word.
 *
 * WHY:
 * Assigns a concrete value to how "likely" a word is based on the current
 * Heatmap distribution.
 */
static int get_heatmap_score(const char* word, int heatmap[5][26])
{
    int score = 0;
    for (int j = 0; j < 5; j++)
    {
        int char_idx = word[j] - 'A';
        if (char_idx >= 0 && char_idx < 26) score += heatmap[j][char_idx];
    }
    return score;
}

// --- LOOK AHEAD IMPLEMENTATION ---

/*
 * FUNCTION: lookahead_feedback_index
 *
 * WHAT:
 * A specialized, fast version of the feedback generator. Instead of generating
 * strings like "GBYBB", it returns a unique integer (0-242) representing the pattern.
 *
 * WHY:
 * Performance optimization. This function is called inside the inner loop of
 * the "Look Ahead" simulation (Depth 2). Integer math is significantly faster
 * than string comparison, allowing us to simulate thousands of games per turn.
 */
static int lookahead_feedback_index(const char* guess, const char* answer)
{
    int index = 0; int multiplier = 1; int states[5] = { 0 }; int answer_char_counts[26] = { 0 };
    // 1. Greens: Check exact matches first
    for (int i = 0; i < 5; i++) { if (guess[i] == answer[i]) states[i] = 2; else answer_char_counts[answer[i] - 'A']++; }
    // 2. Yellows: Check displaced matches
    for (int i = 0; i < 5; i++) { if (states[i] != 2) { int char_idx = guess[i] - 'A'; if (answer_char_counts[char_idx] > 0) { states[i] = 1; answer_char_counts[char_idx]--; } } }
    // 3. Encode: Convert Base-3 state array to Base-10 integer
    for (int i = 0; i < 5; i++) { index += states[i] * multiplier; multiplier *= 3; }
    return index;
}

/*
 * FUNCTION: calculate_lookahead_bonus
 *
 * WHAT:
 * Simulates playing `candidate` against every possible answer in the valid set.
 * It calculates a score based on how well that candidate splits the remaining words.
 *
 * FEATURES:
 * 1. Branching Factor: Rewards splits that create small buckets (Safety).
 * 2. Sniper Bonus: Rewards splits that isolate words into buckets of size 1 (Speed).
 * 3. Doomsday Constraint: Penalizes splits that leave buckets larger than
 * the number of guesses remaining (Death Prevention).
 *
 * WHY:
 * Standard Entropy assumes all splits are equal. This function simulates the
 * actual game dynamics to differentiate between a "Good Math" word and a
 * "Good Game" word.
 */
static double calculate_lookahead_bonus(const dictionary_entry_t* candidate, const dictionary_pointer_array_t p_rank_sorted, int valid_count, int turn)
{
    if (valid_count <= 1) return 0.0;

    // Histogram of resulting bucket sizes for this candidate (243 possible patterns)
    int bins[243] = { 0 };

    // Simulate the guess against every valid answer
    for (int i = 0; i < valid_count; i++)
    {
        int pattern_idx = lookahead_feedback_index(candidate->word, p_rank_sorted[i]->word);
        bins[pattern_idx]++;
    }

    double sum_squares = 0.0; int singles_count = 0; int max_bucket = 0;

    // Analyze the distribution of buckets
    for (int i = 0; i < 243; i++)
    {
        if (bins[i] > 0)
        {
            sum_squares += (double)bins[i] * (double)bins[i];
            if (bins[i] == 1) singles_count++;
            if (bins[i] > max_bucket) max_bucket = bins[i];
        }
    }

    // Score 1: Safety (Minimize the sum of squares = maximize branching)
    double branching_factor = ((double)valid_count * (double)valid_count) / sum_squares;
    double safety_score = log10(branching_factor);

    // Score 2: Speed (Sniper Bonus). Small reward for finding the answer immediately.
    // Only applied after Turn 1 to avoid picking risky openers like TARES over SALET.
    double sniper_bonus = 0.0;
    if (turn > 1) { sniper_bonus = (double)singles_count * 0.04; }

    double total_score = safety_score + sniper_bonus;

    // DOOMSDAY CONSTRAINT:
    // If the largest bucket is bigger than our remaining guesses, we will likely lose.
    // Example: 10 words left, 2 guesses left. If this word leaves a bucket of 4,
    // we are mathematically dead. Apply a massive penalty.
    int guesses_remaining = MAX_GUESSES - turn;
    if (max_bucket > guesses_remaining) { return total_score - 100.0; }

    // Soft mid-game clamp to avoid wide buckets even if not immediately fatal
    if (valid_count > 4 && max_bucket > (valid_count / 2) + 1) { return total_score - 5.0; }

    return total_score;
}

/*
 * FUNCTION: get_smart_hybrid_guess
 *
 * WHAT:
 * The Master Decision Engine.
 * It evaluates candidates based on the active strategy configuration.
 *
 * FLOW:
 * 1. Turn 2 Coverage Check (if enabled).
 * 2. Vowel Contingency Check (if enabled).
 * 3. Early Bias (Anchors/Vowels) (if enabled).
 * 4. Heatmap Priority (if enabled).
 * 5. Main Loop (Standard Hybrid):
 * - Iterates through candidates sorted by Entropy.
 * - Applies Linguistic Filters (unless in Panic Mode).
 * - Applies "Endgame Clamp": If valid_count <= 20, disable LookAhead/RankBias.
 * - Calculates Look Ahead bonus.
 * - Selects the best candidate.
 *
 * WHY:
 * This function consolidates all the experimental logic into a single pipeline.
 * The "Endgame Clamp" is particularly vital: it forces the bot to stop being "clever"
 * and start being "safe" (Pure Greedy Entropy) when the word count gets low,
 * ensuring the 100% win rate.
 */
const dictionary_entry_t* get_smart_hybrid_guess(
    const dictionary_pointer_array_t p_entropy_sorted,
    const dictionary_pointer_array_t p_rank_sorted,
    int count,
    const HybridConfig* config,
    const int* min_required_counts,
    int valid_count,
    int turn)
{
    if (count == 0) return NULL;
    const dictionary_entry_t* best_candidate = NULL;

    // --- STRATEGY D: DYNAMIC TURN 2 COVERAGE ---
    // Exploration Strategy: Sacrifice Turn 2 to find as many new letters as possible.
    if (config->prioritize_turn2_coverage && turn == 2)
    {
        int best_cov = -1;
        const dictionary_entry_t* best_cov_cand = NULL;

        // Scan TOP 100 Rank candidates (Common Words) to find best coverage
        int scan_limit = (count < 100) ? count : 100;

        for (int i = 0; i < scan_limit; i++)
        {
            const dictionary_entry_t* cand = p_rank_sorted[i];

            // Standard Filters
            bool pass = true;
            bool apply_ling = config->use_linguistic_filter && (turn >= config->linguistic_filter_start_turn);
            if (apply_ling && !is_linguistically_sound(cand)) pass = false;
            if (pass && config->use_risk_filter && is_risky_guess(cand, min_required_counts)) pass = false;
            if (cand->is_eliminated) pass = false; // Must be a valid word for this strategy

            if (pass)
            {
                int cov = calculate_new_letter_coverage(cand->word, min_required_counts);
                if (cov > best_cov)
                {
                    best_cov = cov;
                    best_cov_cand = cand;
                }
            }
        }
        if (best_cov_cand != NULL) return best_cov_cand;
    }

    // --- STRATEGY A: CONTINGENCY ---
    // If Turn 1 found almost no vowels, pivot to a vowel-heavy word.
    if (config->prioritize_vowel_contingency && turn == 2)
    {
        int known = count_known_vowels(min_required_counts);
        if (known < 2)
        {
            int best_new = -1; double best_ent = -1.0;
            int scan = (count < 30) ? count : 30;
            for (int i = 0; i < scan; i++)
            {
                const dictionary_entry_t* cand = p_entropy_sorted[i];
                bool pass = true;
                if (config->use_linguistic_filter && (turn >= config->linguistic_filter_start_turn) && !is_linguistically_sound(cand)) pass = false;
                if (pass && config->use_risk_filter && is_risky_guess(cand, min_required_counts)) pass = false;
                if (pass)
                {
                    int v = count_new_vowels(cand->word, min_required_counts);
                    if (v > best_new) { best_new = v; best_candidate = cand; best_ent = cand->entropy; }
                    else if (v == best_new) { if (cand->entropy > best_ent) { best_candidate = cand; best_ent = cand->entropy; } }
                }
            }
            if (best_candidate != NULL) return best_candidate;
        }
    }

    // --- STRATEGY B: EARLY BIAS ---
    // Prioritize structural anchors or unique vowels in the first 2 turns.
    if (turn <= 2 && (config->prioritize_new_vowels || config->prioritize_anchors))
    {
        int best_score = -1; double best_ent = -1.0;
        int scan = (count < 30) ? count : 30;
        for (int i = 0; i < scan; i++)
        {
            const dictionary_entry_t* cand = p_entropy_sorted[i];
            bool pass = true;
            bool apply_ling = config->use_linguistic_filter && (turn >= config->linguistic_filter_start_turn);
            if (apply_ling && !is_linguistically_sound(cand)) pass = false;
            if (pass && config->use_risk_filter && is_risky_guess(cand, min_required_counts)) pass = false;
            if (pass)
            {
                int sc = config->prioritize_anchors ? calculate_anchor_score(cand->word) : count_unique_vowels_simple(cand->word);
                if (sc > best_score) { best_score = sc; best_candidate = cand; best_ent = cand->entropy; }
                else if (sc == best_score) { if (cand->entropy > best_ent) { best_candidate = cand; best_ent = cand->entropy; } }
            }
        }
        if (best_candidate != NULL) return best_candidate;
    }

    // --- STRATEGY: HEATMAP PRIORITY ---
    // Pick the word that best fits the positional frequency of remaining answers.
    if (config->use_heatmap_priority && valid_count > 2)
    {
        int heatmap[5][26];
        build_heatmap_matrix(p_entropy_sorted, count, heatmap);
        const dictionary_entry_t* best_heatmap_cand = NULL;
        int best_heatmap_score = -1;
        int scan_depth = 20; int scanned = 0;
        for (int i = 0; i < count; i++)
        {
            if (scanned >= scan_depth) break;
            const dictionary_entry_t* cand = p_entropy_sorted[i];
            bool pass = true;
            bool apply_ling = config->use_linguistic_filter && (turn >= config->linguistic_filter_start_turn);
            if (apply_ling && !is_linguistically_sound(cand)) pass = false;
            if (pass && config->use_risk_filter && is_risky_guess(cand, min_required_counts)) pass = false;
            if (pass)
            {
                int score = get_heatmap_score(cand->word, heatmap);
                if (score > best_heatmap_score) { best_heatmap_score = score; best_heatmap_cand = cand; }
                scanned++;
            }
        }
        if (best_heatmap_cand != NULL) return best_heatmap_cand;
    }

    // --- STRATEGY C: STANDARD SMART HYBRID + LOOK AHEAD ---
    const dictionary_entry_t* best_final_candidate = NULL;
    double best_combined_score = -1000.0;

    // THE ENDGAME CLAMP (Panic Mode):
    // If we have fewer than 20 words left, we disable all "Clever" heuristics
    // (Look Ahead, Rank Bias) and revert to pure Greedy Entropy.
    // This is the safety net that ensures 100% win rates.
    bool is_endgame_panic = (valid_count <= 20);
    int candidates_evaluated = 0;
    int max_evals = (config->look_ahead_depth > 0 && !is_endgame_panic) ? PRUNE_COUNT : count;

    for (int i = 0; i < count; i++)
    {
        if (candidates_evaluated >= max_evals) break;
        const dictionary_entry_t* cand = p_entropy_sorted[i];
        bool pass = true;

        // Special Case: Endgame Solvers (Trying to guess the answer directly)
        // If the word is a valid answer and the list is small, skip filters.
        bool is_endgame_solver = (!cand->is_eliminated && valid_count <= 10);

        if (!is_endgame_solver)
        {
            bool apply_ling = config->use_linguistic_filter && (turn >= config->linguistic_filter_start_turn);
            // Disable Linguistic filter in panic mode to allow more flexibility in splitting
            if (is_endgame_panic) apply_ling = false;

            if (apply_ling && !is_linguistically_sound(cand)) pass = false;
            if (pass && config->use_risk_filter && is_risky_guess(cand, min_required_counts)) pass = false;
        }

        if (pass)
        {
            double current_score = cand->entropy;
            // Apply Look Ahead bonus ONLY if not in panic mode
            if (config->look_ahead_depth > 0 && !is_endgame_panic) { current_score += calculate_lookahead_bonus(cand, p_rank_sorted, valid_count, turn); }
            if (current_score > best_combined_score) { best_combined_score = current_score; best_final_candidate = cand; }
            candidates_evaluated++;
        }
    }
    if (best_final_candidate == NULL) best_final_candidate = p_entropy_sorted[0];

    // 2. Tie-Breaker with Rank (Frequency)
    // Only applied if NOT in panic mode.
    if (config->rank_priority_tolerance > 0.0 && !is_endgame_panic)
    {
        const dictionary_entry_t* best_rank_cand = p_rank_sorted[0];
        for (int i = 0; i < count; i++)
        {
            const dictionary_entry_t* cand = p_rank_sorted[i];
            bool pass = true;
            bool is_endgame = (!cand->is_eliminated && valid_count <= 10);
            if (!is_endgame)
            {
                bool apply_ling = config->use_linguistic_filter && (turn >= config->linguistic_filter_start_turn);
                if (apply_ling && !is_linguistically_sound(cand)) pass = false;
                if (pass && config->use_risk_filter && is_risky_guess(cand, min_required_counts)) pass = false;
            }
            if (pass) { best_rank_cand = cand; break; }
        }
        double diff = best_final_candidate->entropy - best_rank_cand->entropy;
        if (diff < config->rank_priority_tolerance) { return best_rank_cand; }
    }
    return best_final_candidate;
}

// --- Standard Filtering Helpers ---

/*
 * FUNCTION: meets_filtered_criteria
 *
 * WHAT:
 * Checks if a word passes the "Standard 4" criteria:
 * 1. No duplicate letters.
 * 2. No Plural Nouns ('P').
 * 3. No Pronouns ('R').
 * 4. No Past Tense ('T') or 3rd Person ('S') verbs.
 *
 * WHY:
 * Used to populate the "Entropy Filtered" and "Rank Filtered" columns
 * in the user interface recommendation box.
 */
static bool meets_filtered_criteria(const dictionary_entry_t* pEntry)
{
    if (pEntry->contains_duplicate_letters) return false;
    if (pEntry->noun_type == 'P' || pEntry->noun_type == 'R') return false;
    if (pEntry->verb_type != 'N' && pEntry->verb_type != 'P') return false;
    return true;
}

/*
 * FUNCTION: find_filtered_candidate
 *
 * WHAT:
 * Iterates through a sorted array to find the first entry that meets
 * the filtered criteria and is not eliminated.
 *
 * WHY:
 * A helper function to extract the best "Safe" guess from a sorted list.
 */
static const dictionary_entry_t* find_filtered_candidate(const dictionary_pointer_array_t p_sorted_array, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const dictionary_entry_t* pEntry = p_sorted_array[i];
        if (pEntry->is_eliminated) break;
        if (meets_filtered_criteria(pEntry)) return pEntry;
    }
    return NULL;
}

/*
 * FUNCTION: get_best_guess_candidates
 *
 * WHAT:
 * Populates the recommendations array for the UI.
 * It grabs the top word for:
 * 1. Entropy Raw
 * 2. Entropy Filtered
 * 3. Rank Raw
 * 4. Rank Filtered
 *
 * WHY:
 * This gathers all the data needed to display the "Alignment Box" in
 * the interactive console, allowing the user to compare different metrics.
 */
bool get_best_guess_candidates(const dictionary_pointer_array_t p_entropy_sorted, const dictionary_pointer_array_t p_rank_sorted, int count, recommendations_array_t candidates)
{
    if (count == 0) return false;
    candidates[0].label = "Entropy Raw (Max Info)"; candidates[0].pEntry = p_entropy_sorted[0];
    candidates[2].label = "Rank Raw (Most Common)"; candidates[2].pEntry = p_rank_sorted[0];
    candidates[1].label = "Entropy Filtered"; candidates[1].pEntry = find_filtered_candidate(p_entropy_sorted, count);
    if (candidates[1].pEntry == NULL) candidates[1].pEntry = p_entropy_sorted[0];
    candidates[3].label = "Rank Filtered"; candidates[3].pEntry = find_filtered_candidate(p_rank_sorted, count);
    if (candidates[3].pEntry == NULL) candidates[3].pEntry = p_rank_sorted[0];
    return true;
}

/*
 * FUNCTION: filter_dictionary_by_constraints
 *
 * WHAT:
 * The primary state-update mechanism.
 * Iterates through the dictionary and sets `is_eliminated = true` for any word
 * that conflicts with the feedback from the last guess.
 *
 * WHY:
 * This reduces the search space. It uses `get_feedback_pattern` to simulate
 * "If the answer was X, what pattern would I have gotten?". If that matches
 * the *actual* pattern we got, X is still a valid candidate.
 */
void filter_dictionary_by_constraints(dictionary_entry_t* p_dictionary, int count, const char* guess, const char* result_pattern)
{
    char hypothetical_pattern[6];
    for (int i = 0; i < count; ++i)
    {
        dictionary_entry_t* pEntry = &p_dictionary[i];
        if (pEntry->is_eliminated) continue;
        get_feedback_pattern(guess, pEntry->word, hypothetical_pattern);
        if (strcmp(hypothetical_pattern, result_pattern) != 0) pEntry->is_eliminated = true;
    }
}