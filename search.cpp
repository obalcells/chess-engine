#include <vector>
#include <iostream>
#include <cassert>
#include <sys/timeb.h>
#include "defs.h"
#include "protos.h"
#include "data.h"
#include "board.h"
#include "gen.h"
#include "move.h"
// #include "eval.h"
#include "eval_tscp.h"
#include "hash.h"

int get_ms() {
    struct timeb timebuffer;
    ftime(&timebuffer);
    return (timebuffer.time * 1000) + timebuffer.millitm;
}

void think(int seconds) {
    for(int initial_depth = MAX_DEPTH; initial_depth <= MAX_DEPTH; initial_depth += 2) {
        nodes = 0;
        int initial_time = get_ms();
        std::cout << "Searching with initial depth = " << initial_depth << "..." << endl;
        search(-999999, 999999, initial_depth);
        std::cout << "Best move is: " << next_move.from << " -> " << next_move.to << endl;
        std::cout << "Nodes searched: " << nodes << endl;
        std::cout << "Time elapsed: " << get_ms() - initial_time << " ms";
        std::cout << "........................................" << endl;
    }
}

int search(int alpha, int beta, int depth) {
  assert(alpha < beta);

  if(in_check(side)) {
    depth++;
  } else if(depth == 0) {
    return quiescence_search(alpha, beta);
  }

  // we check if search has already been performed for this state
  long long state_key = get_hash();
  int state_idx = state_key & (n_entries - 1);
  if(pv_table[state_idx].state_key == state_key && pv_table[state_idx].alpha >= alpha && pv_table[state_idx].alpha <= beta) {
    next_move = pv_table[state_idx].move; // what if there is a collision and we are at the root?    
    return pv_table[state_key].alpha;
  }

  int first_move = (int)move_stack.size();
  generate_moves(); // moves are already sorted
  int last_move = (int)move_stack.size() - 1;
  Move best_move;

  for(int i = last_move; i >= first_move; i--) {
    Move move = move_stack[i];
    make_move(move.from, move.to, QUEEN); // this could be sped up
    int score = -search(-beta, -alpha, depth - 1);
    take_back(move);
    move_stack.pop_back();
    // move increases the alpha-cutoff
    if(score > alpha) {
      best_move = move;
      alpha = score;
      if(score >= beta) {
        history[side][move.from][move.to] += depth * depth;
        age_history();
        // the move caused a beta-cutoff so it must be good
        // but it won't be picked by parent
        while(i-- > first_move) move_stack.pop_back();
        next_move = best_move;
        return beta;
      }
    }
  }

  history[side][best_move.from][best_move.to] += depth * depth;
  age_history();
  pv_table[state_idx] = PV_Entry(state_key, alpha, best_move);
  next_move = best_move;
  return alpha;
}

int quiescence_search(int alpha, int beta) {
  int score = eval_tscp();
  if(score >= beta) {
    return beta;
  } else if(score > alpha) {
    alpha = score;
  }

  // size of stack before
  int first_move = (int)move_stack.size();
  generate_capture_moves();
  // size of stack after move generation
  int last_move = (int)move_stack.size() - 1;

  for(int i = last_move; i >= first_move; i--) {
    Move move = move_stack[i];
    make_move(move.from, move.to, QUEEN);
    score = -quiescence_search(-beta, -alpha);
    take_back(move);
    move_stack.pop_back();
    // if score 
    if(score > alpha) {
      alpha = score;
      if(beta <= alpha) {
        while(i-- > first_move) move_stack.pop_back();
        return beta;
      }
    }
  }

  return alpha;
}

void age_history() {
  for(int i = 0; i < 2; i++) {
    for(int j = 0; j < 64; j++) {
      for(int k = 0; k < 64; k++) {
        history[i][j][k] = history[i][j][k] >> 1; // faster division by two
      }
    }
  }
}