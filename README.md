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

---

## ⚙️ Setup & Compilation Guide

**Important:** You cannot simply clone and run this project. You must configure the dependencies and update the dictionary file path for your local machine.

### 1. Install Dependencies (vcpkg)
This project uses **libcurl** for web scraping. The easiest way to install it on Windows is via [Microsoft vcpkg](https://github.com/microsoft/vcpkg).

1.  **Install vcpkg** (if you haven't already):
    ```powershell
    git clone [https://github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg)
    .\vcpkg\bootstrap-vcpkg.bat
    ```
2.  **Install the library:**
    Open your terminal/PowerShell and run:
    ```powershell
    vcpkg install curl:x64-windows
    ```
3.  **Link to Visual Studio:**
    Run this command to allow Visual Studio to automatically find the headers and DLLs:
    ```powershell
    vcpkg integrate install
    ```

### 2. Enable OpenMP
The simulation engine uses multi-threading to run thousands of games per second. You must enable this in your project settings.

1.  Right-click **WordleChampion** in the Solution Explorer and select **Properties**.
2.  Set **Configuration** to `All Configurations` and **Platform** to `All Platforms` (or `x64`).
3.  Navigate to **Configuration Properties** > **C/C++** > **Language**.
4.  Change **Open MP Support** to **Yes (/openmp)**.
5.  Click **Apply**.

### 3. ⚠️ CRITICAL: Set Dictionary Path
The path to the `AllWords.txt` dictionary file is currently hardcoded in the source. **You must change this to match your computer.**

1.  Open **`load_dictionary.cpp`**.
2.  Locate the `load_dictionary` function (around line 125).
3.  Find the `fopen_s` call:
    ```cpp
    // Find this line:
    errval = fopen_s(&fpIn, "C:\\VS2022.Projects\\StuffForWordle\\WordleWordsCSVs\\AllWords.txt", "r");
    ```
4.  **Update the path** to point to where *you* saved `AllWords.txt`.
    * *Option A (Hardcoded):* Change it to your absolute path, e.g., `"C:\\Users\\YourName\\Desktop\\AllWords.txt"`.
    * *Option B (Portable):* Change it to `"AllWords.txt"` and ensure the text file is in the same folder as your `.exe` (usually `x64/Debug` or `x64/Release`).

---
## 🔬 The Grand Tournament Results

We ran a comprehensive Monte Carlo simulation comparing **19 distinct strategies** against the full dictionary (~5,000 words). The results highlight the critical trade-off between **Speed** (Average Guesses) and **Safety** (Win Rate).

### 🏆 The Podium

| Rank | Strategy | Win Rate | Avg Guesses | Verdict |
| :--- | :--- | :--- | :--- | :--- |
| 🥇 | **Entropy Linguist (Strict)** | **100.00%** | **3.7652** | **The Perfect Solver.** Safe, robust, and efficient. |
| 🥈 | Vowel Contingency | 100.00% | 3.8094 | Safe, but mathematically inefficient compared to Linguist. |
| 🥉 | Entropy Raw (Baseline) | 99.96% | 3.7518 | The fastest bot, but lost 2 games due to risky guesses. |

### 📉 The "Human Strategy" Myth
Many human players start with vowel-heavy words like `AUDIO` or `ADIEU`. The simulation proves this is suboptimal. Consonants provide more information entropy than vowels.

| Strategy | Win Rate | Avg Guesses | Games Lost |
| :--- | :--- | :--- | :--- |
| **Entropy Linguist** | **100.00%** | **3.76** | **0** |
| Vowel Hunter (Adieu) | 99.88% | 3.98 | 6 |
| Vowel Hunter (Audio) | 99.84% | 4.00 | 8 |

### 🧠 Key Findings

1.  **Entropy > Frequency:** Strategies that relied on `Rank` (picking common words) collapsed in Hard Mode conditions, losing hundreds of games. Math beats intuition.
2.  **The "Green Trap":** Strategies like `Heatmap Seeker` that try to "fit" the pattern too early often get locked into "silos" (e.g., `_ATCH`) and run out of guesses.
3.  **The "Pothole" Effect:** Aggressive strategies like `Look Ahead` and `Entropy Raw` are faster on average (3.75 vs 3.76), but they occasionally crash on obscure words. The `Entropy Linguist` sacrifices 0.01 speed for 100% survival.

---
