# WordleChampion

**A High-Performance C++ Hybrid Wordle Solver & Research Engine.**

WordleChampion is a modular simulation platform designed to analyze, optimize, and mathematically prove the efficacy of various Wordle solving strategies. It features a multi-threaded Monte Carlo engine capable of simulating thousands of games in seconds to determine the "Game Theory Optimal" strategy for both Standard and Hard Mode.

## 🏆 The Champion Strategy
After extensive tournaments on a 6,000+ word dictionary, the current reigning champion is:

### **Entropy Linguist (Strict)**
* **Win Rate:** 100.00% (Undefeated)
* **Average Guesses:** ~3.76
* **Logic:** Combines **Information Theory (Shannon Entropy)** with **Linguistic Heuristics**. It maximizes the information gained from every guess but strictly rejects "garbage" words (Plurals, Past Tense) that are statistically unlikely to be the answer.

## 🚀 Key Features

* **Hybrid Strategy Engine:** A flexible configuration system allows for the creation of distinct bot personalities (e.g., "Greedy", "Safe", "Vowel Hunter") without code rewriting.
* **Monte Carlo Simulator:** A multi-threaded OpenMP tournament runner that pits strategies against the full dictionary to generate empirical win/loss data.
* **Interactive Solver:** A playable command-line tool that suggests optimal moves for your daily Wordle game.
* **Live Data Ingestion:** Automatically scrapes the latest "Used Word" lists from the web to ensure the bot never guesses a word that has already been the solution.
* **High-Performance Architecture:** Uses integer-based pattern encoding and cache-friendly memory views to perform millions of entropy calculations per second.

## 📂 Project Structure

The codebase is separated into distinct layers to ensure modularity:

* **`main.cpp`**: Application bootstrap and Interactive/Simulation mode selection.
* **`hybrid_strategies.cpp`**: The "Museum" of bot configurations. Contains 19 distinct strategies, including historical experiments and failed prototypes.
* **`solver_logic.cpp`**: The decision-making brain. Contains the heuristics for Look Ahead, Risk Filtering, and Candidate Selection.
* **`entropy_calculator.cpp`**: The mathematical engine. Heavily optimized OMP loops for Shannon Entropy calculation.
* **`load_dictionary.cpp`**: Data ingestion pipeline. Handles the parsing of the fixed-width dictionary format.
* **`monte_carlo.cpp`**: The tournament director. Manages thread-local storage and statistical aggregation.

## 📄 Data Format (`AllWords.txt`)

The solver relies on a specific, highly-structured dictionary file format. Each line must strictly adhere to the following fixed-width layout:

| Offset | Length | Description | Values |
| :--- | :--- | :--- | :--- |
| **0** | 5 | **The Word** | Uppercase (e.g., `SALET`, `CRANE`) |
| **5** | 3 | **Frequency Rank** | `000`-`100` (100 = Most Common, 000 = Obscure) |
| **8** | 1 | **Noun Type** | `P` (Plural), `S` (Singular), `N` (None), `R` (Pronoun) |
| **9** | 1 | **Verb Type** | `T` (Past), `S` (3rd Person), `P` (Present), `N` (None) |

*Example Line:*
`CAKES095PS` -> Word: **CAKES**, Rank: **95** (Common), Noun: **Plural**, Verb: **3rd Person**.

## 🛠️ Building & Running

### Prerequisites
* **C++ Compiler:** MSVC (Visual Studio 2022 recommended) or GCC/Clang with C++17 support.
* **Libraries:**
    * `libcurl`: For downloading the live "Used Words" list.
    * `OpenMP`: For multi-threaded simulation performance.

### Visual Studio Instructions
1.  Open `WordleChampion.sln`.
2.  Ensure **Release** or **Debug** configuration is selected (x64 recommended).
3.  Build Solution (**Ctrl+Shift+B**).
4.  Run (**Ctrl+F5**).
5.  Follow the on-screen prompts to choose between **Interactive Mode** or **Monte Carlo Simulation**.

## 🔬 Research History

This repository includes the full history of strategy development defined in `hybrid_strategies.cpp`:

* **Baseline:** `Entropy Raw` (Pure Math) vs `Rank Raw` (Pure Frequency).
* **Experiments:**
    * *Vowel Contingency:* Pivoting strategies based on early vowel discovery.
    * *Look Ahead:* Simulating Depth-2 game trees to avoid traps.
    * *Heatmap Seeker:* Attempting to maximize positional probability.
    * *Coverage:* Maximizing unique letter counts (proven inferior to Entropy).
* **The Solution:** The **Entropy Linguist (Strict)** emerged as the only strategy capable of maintaining a 100% win rate across jagged/filtered dictionaries where aggressive strategies often fell into "potholes."

## 📜 License

This project is open source. Feel free to fork, experiment, and try to dethrone the Champion!
