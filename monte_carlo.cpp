/*
 * FILE: monte_carlo.cpp
 *
 * WHAT:
 * Implements the high-performance Simulation Engine ("The Tournament").
 * This module runs thousands of full Wordle games in parallel to empirically
 * measure the performance of a specific Strategy Configuration.
 *
 * ARCHITECTURE:
 * 1. Serial Setup: Pre-calculates the optimal opening word (Opener) once.
 * 2. Parallel Execution: Uses OpenMP to spawn threads. Each thread takes a
 * subset of the dictionary (the "Secret Answers") and plays a full game.
 * 3. Thread Isolation: Each thread gets its own copy of the dictionary memory
 * to ensure that filtering words in Game A doesn't corrupt Game B.
 * 4. Aggregation: Uses atomic operations to collect Wins/Losses safely.
 *
 * WHY:
 * Theoretical analysis of Wordle is complex. The only way to prove a strategy
 * achieves a 100% win rate or a 3.76 average is to force it to play against
 * every single possible answer word and record the outcome.
 */

#include "monte_carlo.h"
#include "solver_logic.h"
#include "entropy_calculator.h"
#include "duplicate_dictionary.h"
#include "comparators.h"
#include "hybrid_strategies.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h> 

#define MAX_GUESSES 6
extern bool g_isHardMode;

/*
 * STRUCT: SimStats
 *
 * WHAT:
 * A container for the results of a single strategy simulation.
 *
 * FIELDS:
 * - wins/losses: Raw counts.
 * - guess_distribution: Histogram (How many games won in 1, 2, 3..6 guesses).
 * - average_guesses: The primary "Efficiency" metric.
 * - time_taken: Wall-clock time for the sim (performance benchmarking).
 */
typedef struct _sim_stats
{
    char strategy_name[50];
    int wins;
    int losses;
    long total_guesses;
    int guess_distribution[MAX_GUESSES + 1];
    double average_guesses;
    double win_percent;
    double time_taken;
} SimStats;

/*
 * FUNCTION: print_distribution
 *
 * WHAT:
 * Prints a visual histogram of the guess distribution.
 *
 * WHY:
 * Averages can be misleading. A bot might have a great average (3.5) but
 * lose 1% of games. The distribution reveals the "Tail Risk" (how often
 * it reaches 6 guesses).
 */
void print_distribution(SimStats* s)
{
    printf("  %s Distribution:\n", s->strategy_name);
    if (s->wins == 0) { printf("    N/A (0 wins)\n"); return; }

    for (int i = 1; i <= MAX_GUESSES; i++)
    {
        if (s->guess_distribution[i] > 0)
        {
            double pct = 100.0 * s->guess_distribution[i] / s->wins;
            printf("    %d guess%s | %4d (%5.2f%%)\n", i, (i == 1 ? "  " : "es"), s->guess_distribution[i], pct);
        }
    }
    printf("\n");
}

/*
 * FUNCTION: run_hybrid_strategy
 *
 * WHAT:
 * The Core Simulation Loop.
 * Runs a full pass over the `p_master_dictionary`, treating every word as
 * the target answer once.
 *
 * FLOW:
 * 1. Determine Opener: Calculates the best starting word (or uses override).
 * 2. OpenMP Parallel Region: Spawns threads.
 * 3. Thread Setup: Allocates local memory.
 * 4. Game Loop: For each target word...
 * a. Reset dictionary.
 * b. Guess & Filter (Turns 1-6).
 * c. Record outcome.
 * 5. Cleanup & Return Stats.
 *
 * WHY:
 * This is the heavy lifter. It handles the memory management required to
 * run thousands of independent games simultaneously without race conditions.
 */
static SimStats run_hybrid_strategy(const HybridConfig config, const dictionary_entry_t* p_master_dictionary, int master_count)
{
    SimStats stats;
    strcpy_s(stats.strategy_name, 50, config.name);
    stats.wins = 0; stats.losses = 0; stats.total_guesses = 0;
    for (int i = 0; i <= MAX_GUESSES; i++) stats.guess_distribution[i] = 0;

    printf(">>> Simulating Bot: %s ...\n", config.name);

    // --- PHASE 1: DETERMINE OPENER (Serial Step) ---
    // We calculate the opening word once on the main thread to avoid re-doing
    // the exact same heavy math 5,000 times in the loop.
    printf("    Determining optimal opening guess...\n");

    dictionary_entry_t* p_opener_data = (dictionary_entry_t*)malloc(sizeof(dictionary_entry_t) * master_count);
    if (!p_opener_data) return stats;
    memcpy(p_opener_data, p_master_dictionary, sizeof(dictionary_entry_t) * master_count);

    dictionary_pointer_array_t p_view_ent = NULL;
    dictionary_pointer_array_t p_view_rank = NULL;

    calculate_entropy_on_dictionary(p_opener_data, master_count);
    duplicate_dictionary_pointers(p_opener_data, master_count, &p_view_ent, compare_dictionary_entries_by_entropy_desc);
    duplicate_dictionary_pointers(p_opener_data, master_count, &p_view_rank, compare_dictionary_entries_by_rank_desc);

    char opening_word[6];
    int init_req_counts[26] = { 0 };

    // Check for Manual Override (e.g., "SALET")
    if (config.opener_override_word != NULL)
    {
        strcpy_s(opening_word, 6, config.opener_override_word);
    }
    // Check for Simple Strategies (Index 0-3)
    else if (config.base_strategy_index != -1)
    {
        recommendations_array_t opening_recs;
        get_best_guess_candidates(p_view_ent, p_view_rank, master_count, opening_recs);
        strcpy_s(opening_word, 6, opening_recs[config.base_strategy_index].pEntry->word);
    }
    // Default: Use the Smart Hybrid Calculator
    else
    {
        const dictionary_entry_t* pOpener = get_smart_hybrid_guess(p_view_ent, p_view_rank, master_count, &config, init_req_counts, master_count, 1);
        strcpy_s(opening_word, 6, pOpener->word);
    }
    printf("    Opener: %s\n", opening_word);

    // Clean up the temporary opener memory
    free(p_view_ent); free(p_view_rank); free(p_opener_data);

    // --- PHASE 2: PARALLEL SIMULATION LOOP ---
    time_t start_time = time(NULL);

#pragma omp parallel
    {
        // --- THREAD LOCAL STORAGE ---
        // Each thread needs its OWN copy of the dictionary.
        // If we shared the master dictionary, Thread A filtering "APPLE" would
        // mess up Thread B trying to find "ZEBRA".
        dictionary_entry_t* p_thread_data = (dictionary_entry_t*)malloc(sizeof(dictionary_entry_t) * master_count);
        dictionary_entry_t** pp_thread_valid = (dictionary_entry_t**)malloc(sizeof(dictionary_entry_t*) * master_count);
        dictionary_pointer_array_t p_thread_view_ent = NULL;
        dictionary_pointer_array_t p_thread_view_rank = NULL;

        // Local stats accumulator to reduce atomic contention
        int local_distribution[MAX_GUESSES + 1] = { 0 };

        if (p_thread_data && pp_thread_valid)
        {
            // Dynamic Schedule: Hands out chunks of work (games) to threads as they finish.
            // This balances the load since some words are harder (take longer) to solve.
#pragma omp for schedule(dynamic)
            for (int t = 0; t < master_count; t++)
            {
                const dictionary_entry_t* target_word = &p_master_dictionary[t];

                // Reset: Copy fresh dictionary state for the new game
                memcpy(p_thread_data, p_master_dictionary, sizeof(dictionary_entry_t) * master_count);
                int current_count = master_count;

                char current_guess[6];
                strcpy_s(current_guess, 6, opening_word);

                int min_required_counts[26] = { 0 };
                bool won = false;
                int guesses_taken = 0;

                // GAME LOOP (Turns 1-6)
                for (int turn = 1; turn <= MAX_GUESSES; turn++)
                {
                    guesses_taken = turn;

                    // Check for Win
                    if (strncmp(current_guess, target_word->word, 5) == 0) { won = true; break; }

                    // Generate Feedback (Simulate the Game Engine)
                    char result_pattern[6];
                    get_feedback_pattern(current_guess, target_word->word, result_pattern);

                    // Update Logic State
                    update_min_required_counts(current_guess, result_pattern, min_required_counts);
                    filter_dictionary_by_constraints(p_thread_data, current_count, current_guess, result_pattern);

                    // Determine Next Guess (Logic differs slightly for Hard/Normal mode optimization)
                    bool use_normal_mode_scan = (!g_isHardMode && (config.base_strategy_index == -1 || config.base_strategy_index <= 1));

                    if (use_normal_mode_scan)
                    {
                        // NORMAL MODE: We scan all words, even invalid ones (for burner value).
                        int validCount = 0;
                        for (int i = 0; i < master_count; ++i) { if (!p_thread_data[i].is_eliminated) pp_thread_valid[validCount++] = &p_thread_data[i]; }
                        if (validCount == 0) break; // Should not happen

                        // Calculate Entropy for ALL candidates based on VALID answer probabilities
                        calculate_entropy_for_candidates(p_thread_data, master_count, pp_thread_valid, validCount);

                        // Sort Views
                        duplicate_dictionary_pointers(p_thread_data, master_count, &p_thread_view_ent, compare_dictionary_entries_by_entropy_no_filter_desc);
                        duplicate_dictionary_pointers(p_thread_data, master_count, &p_thread_view_rank, compare_dictionary_entries_by_rank_desc);

                        // --- TURN 2 FORCED GUESS CHECK ---
                        // Implements "Double Barrel" strategies (e.g., SALET -> COURD)
                        if (turn == 1 && config.second_opener_override_word != NULL)
                        {
                            strcpy_s(current_guess, 6, config.second_opener_override_word);
                        }
                        else if (config.base_strategy_index != -1)
                        {
                            // Simple Strategy (Pick index 0)
                            if (config.base_strategy_index == 0)
                            {
                                // If last turn, must pick a valid word!
                                if (turn == MAX_GUESSES) { for (int i = 0; i < master_count; ++i) { if (!p_thread_view_ent[i]->is_eliminated) { strcpy_s(current_guess, 6, p_thread_view_ent[i]->word); break; } } }
                                else { strcpy_s(current_guess, 6, p_thread_view_ent[0]->word); }
                            }
                            else
                            {
                                for (int i = 0; i < master_count; ++i) { if (!p_thread_view_ent[i]->is_eliminated) { strcpy_s(current_guess, 6, p_thread_view_ent[i]->word); break; } }
                            }
                        }
                        else
                        {
                            // Smart Strategy
                            const dictionary_entry_t* pNext = get_smart_hybrid_guess(p_thread_view_ent, p_thread_view_rank, master_count, &config, min_required_counts, validCount, turn + 1);

                            // Safety: If last turn and bot picked an eliminated burner, force a valid pick
                            if (turn == MAX_GUESSES && pNext->is_eliminated)
                            {
                                for (int i = 0; i < master_count; ++i) { if (!p_thread_view_rank[i]->is_eliminated) { pNext = p_thread_view_rank[i]; break; } }
                            }
                            strcpy_s(current_guess, 6, pNext->word);
                        }
                        free(p_thread_view_ent); free(p_thread_view_rank);
                    }
                    else
                    {
                        // HARD MODE: We physically sort/shrink the array to strictly valid words.
                        qsort(p_thread_data, current_count, sizeof(dictionary_entry_t), compare_master_entries_eliminated_then_alpha);

                        int new_count = current_count;
                        for (int i = 0; i < current_count; ++i) { if (p_thread_data[i].is_eliminated) { new_count = i; break; } }
                        current_count = new_count;
                        if (current_count == 0) break;

                        duplicate_dictionary_pointers(p_thread_data, current_count, &p_thread_view_ent, compare_dictionary_entries_by_entropy_desc);
                        duplicate_dictionary_pointers(p_thread_data, current_count, &p_thread_view_rank, compare_dictionary_entries_by_rank_desc);

                        if (config.base_strategy_index != -1)
                        {
                            recommendations_array_t turn_recs;
                            get_best_guess_candidates(p_thread_view_ent, p_thread_view_rank, current_count, turn_recs);
                            strcpy_s(current_guess, 6, turn_recs[config.base_strategy_index].pEntry->word);
                        }
                        else
                        {
                            const dictionary_entry_t* pNext = get_smart_hybrid_guess(
                                p_thread_view_ent,
                                p_thread_view_rank,
                                current_count,
                                &config,
                                min_required_counts,
                                current_count,
                                turn + 1
                            );
                            strcpy_s(current_guess, 6, pNext->word);
                        }

                        free(p_thread_view_ent); free(p_thread_view_rank);
                    }
                }

                // End of Game: Record Stats
                if (won)
                {
                    // Use atomic increment to prevent race conditions on global stats
#pragma omp atomic
                    stats.wins++;
#pragma omp atomic
                    stats.total_guesses += guesses_taken;

                    // Update local thread histogram
                    local_distribution[guesses_taken]++;
                }
                else
                {
#pragma omp atomic
                    stats.losses++;
                }

                // Progress Indicator (Only thread 0 prints to avoid console chaos)
                if (t % 500 == 0 && omp_get_thread_num() == 0) printf("    Progress: %d/%d (approx)\r", t, master_count);
            }
        }

        // Merge local distribution histogram into global stats safely
#pragma omp critical
        {
            for (int i = 1; i <= MAX_GUESSES; i++) stats.guess_distribution[i] += local_distribution[i];
        }

        // Clean up thread-local memory
        if (p_thread_data) free(p_thread_data);
        if (pp_thread_valid) free(pp_thread_valid);
    }

    // --- PHASE 3: FINALIZE STATS ---
    time_t end_time = time(NULL);
    stats.time_taken = difftime(end_time, start_time);

    if (stats.wins > 0) stats.average_guesses = (double)stats.total_guesses / stats.wins;
    else stats.average_guesses = 0.0;
    if (master_count > 0) stats.win_percent = ((double)stats.wins / master_count) * 100.0;

    printf("    Finished. Wins: %d (%.2f%%) Avg: %.4f\n", stats.wins, stats.win_percent, stats.average_guesses);
    return stats;
}

/*
 * FUNCTION: run_monte_carlo_simulation
 *
 * WHAT:
 * The Tournament Director.
 * 1. Defines the "Active Roster" (which strategies to test).
 * 2. Loops through the roster.
 * 3. Calls `run_hybrid_strategy` for each one.
 * 4. Prints the final comparison table.
 *
 * WHY:
 * This is the user-facing entry point for the simulation mode. It aggregates
 * the results of potentially hours of processing into a single report,
 * identifying the "Tournament Champion."
 */
void run_monte_carlo_simulation(const dictionary_entry_t* p_master_dictionary, int master_count)
{
    printf("\n=============================================\n");
    printf("   STARTING ULTIMATE TOURNAMENT\n");
    printf("   Targeting %d words. Mode: %s\n", master_count, g_isHardMode ? "HARD" : "NORMAL");
    printf("   (Parallel Processing Enabled)\n");
    printf("=============================================\n\n");

    // --- MASTER ROSTER MENU ---
    // Uncomment the indices you want to include in the tournament.
    // Reference `hybrid_strategies.cpp` for details on each ID.

    // 0: Entropy Linguist (Strict)  [THE CHAMPION]
    // 1: Entropy Raw (Baseline)
    // 2: Legacy Reborn (Smart)
    // 3: Vowel Hunter (Audio)
    // 4: Vowel Hunter (Adieu)
    // 5: Vowel Contingency
    // 6: Pattern Hunter (Anchor)
    // 7: Progressive (Skip T1)
    // 8: Progressive (Skip T1-2)
    // 9: Look Ahead (Pruned)
    // 10: Entropy Filtered
    // 11: Rank Raw
    // 12: Rank Filtered
    // 13: Hybrid Apex (Strict)      
    // 14: Deep Linguist             
    // 15: Hybrid Apex II (Safe)     
    // 16: Heatmap Seeker            
    // 17: Dynamic Two-Step (Coverage)
    // 18: Double Barrel (Salet/Courd)

    int active_roster[] = {
        0 // Defaulting to the Undefeated Champion
        ,9 // Look Ahead (Pruned)
        ,5 // Vowel Contingency
        ,2 // Legacy Reborn (Smart)
    };

    int roster_size = sizeof(active_roster) / sizeof(active_roster[0]);
    SimStats* results = (SimStats*)malloc(sizeof(SimStats) * roster_size);

    // Run the simulations
    for (int i = 0; i < roster_size; ++i)
    {
        int strat_idx = active_roster[i];
        results[i] = run_hybrid_strategy(ALL_STRATEGIES[strat_idx], p_master_dictionary, master_count);
    }

    // --- FINAL REPORT ---
    printf("\n\n===========================================================================================\n");
    printf("                               FINAL TOURNAMENT RESULTS                          \n");
    printf("===========================================================================================\n");
    printf("| %-30s | %-5s | %-6s | %-10s | %-11s | %-8s |\n", "STRATEGY", "WINS", "LOSSES", "WIN %", "AVG GUESSES", "TIME (s)");
    printf("|--------------------------------|-------|--------|------------|-------------|----------|\n");

    int best_idx = -1;
    double best_avg = 100.0;
    double best_win = 0.0;

    // Print Rows and Determine Winner
    for (int i = 0; i < roster_size; i++)
    {
        printf("| %-30s | %-5d | %-6d | %9.2f%% | %11.4f | %8.0f |\n",
            results[i].strategy_name,
            results[i].wins,
            results[i].losses,
            results[i].win_percent,
            results[i].average_guesses,
            results[i].time_taken);

        // Winner Logic: Highest Win % First, Lowest Average Second
        if (results[i].win_percent > best_win)
        {
            best_win = results[i].win_percent; best_idx = i; best_avg = results[i].average_guesses;
        }
        else if (results[i].win_percent == best_win && results[i].average_guesses < best_avg)
        {
            best_avg = results[i].average_guesses; best_idx = i;
        }
    }
    printf("===========================================================================================\n");

    if (best_idx >= 0)
    {
        printf("\n*** TOURNAMENT CHAMPION: %s ***\n", results[best_idx].strategy_name);
    }

    printf("\n--- Detailed Distribution for Champion ---\n");
    if (best_idx >= 0)
    {
        print_distribution(&results[best_idx]);
    }

    // If Head-to-Head, show the runner-up stats too for comparison
    if (roster_size == 2)
    {
        int runner_up = (best_idx == 0) ? 1 : 0;
        print_distribution(&results[runner_up]);
    }

    free(results);
}