#include <vector>
#include <iostream>
#include <cassert>
#include "defs.h"
#include "new_position.h"
#include "magicmoves.h"
#include "bitboard.h"

uint64_t get_attackers(int, bool, const BitBoard*);
uint64_t get_blockers(int, bool, const BitBoard*);
uint64_t get_between(int, int, const BitBoard*);
void generate_evasions(std::vector<NewMove>&, const BitBoard*);
void generate_captures(std::vector<NewMove>&, const BitBoard*);
void generate_quiet(std::vector<NewMove>&, const BitBoard*);

// works the same as before - without pointers
void generate_moves(std::vector<NewMove>& moves, const BitBoard& board, bool quiesce = false) {
    std::vector<NewMove> tmp_moves;
    if(board.in_check()) {
        generate_evasions(tmp_moves, &board);
    } else {
        generate_captures(tmp_moves, &board);
        if(!quiesce) /* only captures */
            generate_quiet(tmp_moves, &board);
    }
    /* we will delete this later */
    for(int i = 0; i < (int)tmp_moves.size(); i++)
        if(board.fast_move_valid(tmp_moves[i]))
            moves.push_back(tmp_moves[i]);
}

/* returns a bitboard containing all the pieces which are attacking sq */
uint64_t get_blockers(int sq, bool attacker_side, const BitBoard* board) {
    uint64_t blockers = 0;
    
    /* pawns */
    if(board->get_piece(sq) != NEW_EMPTY) {
        if(attacker_side == BLACK) {
            if((mask_sq(sq) & ~COL_0) && board->get_piece(sq + 7) == BLACK_PAWN)
                blockers |= mask_sq(sq + 7); 
            if((mask_sq(sq) & ~COL_7) && board->get_piece(sq + 9) == BLACK_PAWN)
                blockers |= mask_sq(sq + 9);
        } else {
            if((mask_sq(sq) & ~COL_0) && board->get_piece(sq - 9) == WHITE_PAWN)
                blockers |= mask_sq(sq - 9);
            if((mask_sq(sq) & ~COL_7) && board->get_piece(sq - 7) == WHITE_PAWN) 
                blockers |= mask_sq(sq - 7);
        }
    } else {
        if(attacker_side == WHITE) {
            if(row(sq) > 0 && board->get_piece(sq - 8) == WHITE_PAWN)
                blockers |= mask_sq(sq - 8);
            else if(row(sq) == 3 && board->get_piece(sq - 8) == NEW_EMPTY && board->get_piece(sq - 16) == WHITE_PAWN)
                blockers |= mask_sq(sq - 16);
        } else {
            if(row(sq) < 7 && board->get_piece(sq + 8) == BLACK_PAWN)
                blockers |= mask_sq(sq + 8);
            else if(row(sq) == 4 && board->get_piece(sq + 8) == NEW_EMPTY && board->get_piece(sq + 16) == BLACK_PAWN)
                blockers |= mask_sq(sq + 16);
        }
    }

    blockers |= Rmagic(sq, board->get_all_mask()) & 
                (board->get_rook_mask(attacker_side) | board->get_queen_mask(attacker_side));
    blockers |= Bmagic(sq, board->get_all_mask()) &
                (board->get_bishop_mask(attacker_side) | board->get_queen_mask(attacker_side));
    blockers |= knight_attacks[sq] &
                board->get_knight_mask(attacker_side);

    return blockers;
}

/* returns a bitboard containing all the pieces from a certain side which are attacking sq */
uint64_t get_attackers(int sq, bool attacker_side, const BitBoard* board) {
    uint64_t attackers = 0;

    /* pawns */
    if(attacker_side == BLACK) {
        if((mask_sq(sq) & ~COL_0) && board->get_piece(sq + 7) == BLACK_PAWN)
            attackers |= mask_sq(sq + 7); 
        if((mask_sq(sq) & ~COL_7) && board->get_piece(sq + 9) == BLACK_PAWN)
            attackers |= mask_sq(sq + 9);
    } else {
        if((mask_sq(sq) & ~COL_0) && board->get_piece(sq - 9) == WHITE_PAWN)
            attackers |= mask_sq(sq - 9);
        if((mask_sq(sq) & ~COL_7) && board->get_piece(sq - 7) == WHITE_PAWN) 
            attackers |= mask_sq(sq - 7);
    }

    attackers |= Rmagic(sq, board->get_all_mask()) & 
                (board->get_rook_mask(attacker_side) | board->get_queen_mask(attacker_side));
    attackers |= Bmagic(sq, board->get_all_mask()) &
                (board->get_bishop_mask(attacker_side) | board->get_queen_mask(attacker_side));
    attackers |= knight_attacks[sq] &
                board->get_knight_mask(attacker_side);

    return attackers;
}

/* returns a bitboard with the squares between from_sq and to_sq */
/* it takes into account the type of piece at from_sq            */
uint64_t get_between(int from_sq, int to_sq, const BitBoard* board) {
    const int piece = board->get_piece(from_sq);

    if(row(from_sq) == row(to_sq) || col(from_sq) == col(to_sq)) {
        if(piece == WHITE_ROOK || piece == BLACK_ROOK || piece == WHITE_QUEEN || piece == BLACK_QUEEN) {
            const uint64_t all_mask = board->get_all_mask();
            return Rmagic(from_sq, all_mask) & Rmagic(to_sq, all_mask) & ~all_mask;
        }       
    } else if(abs(row(from_sq) - row(to_sq)) == abs(col(from_sq) - col(to_sq))) {
        if(piece == WHITE_BISHOP || piece == BLACK_BISHOP || piece == WHITE_QUEEN || piece == BLACK_QUEEN) {
            const uint64_t all_mask = board->get_all_mask();
            return Bmagic(from_sq, all_mask) & Bmagic(to_sq, all_mask) & ~all_mask;
        }
    }

    return 0;
}

/* we assume that king is in check */
void generate_evasions(std::vector<NewMove>& moves, const BitBoard* board) {
    const uint64_t king_mask = board->get_king_mask(board->side);
    const int king_pos = lsb(king_mask);
    int from_sq, to_sq; 
    uint64_t king_attackers = get_attackers(king_pos, board->xside, board);    

    if(popcnt(king_attackers) == 1) {
        /* a piece (different than the checked king) will try to eat the attacker */
        int attacker_pos = lsb(king_attackers);

        /* special case: the attacker is a pawn and we eat it enpass */
        if(board->side == BLACK && (mask_sq(attacker_pos) & ROW_3) && board->enpassant == col(attacker_pos)) {
            if(col(attacker_pos) > 0 && board->get_piece(attacker_pos - 1) == BLACK_PAWN)
                moves.push_back(NewMove(attacker_pos - 1, attacker_pos - 8, ENPASSANT_MOVE));
            if(col(attacker_pos) < 7 && board->get_piece(attacker_pos + 1) == BLACK_PAWN) {
                moves.push_back(NewMove(attacker_pos + 1, attacker_pos - 8, ENPASSANT_MOVE));
            }
        } else if(board->side == WHITE && (mask_sq(attacker_pos) & ROW_4) && board->enpassant == col(attacker_pos)) {
            if(col(attacker_pos) > 0 && board->get_piece(attacker_pos - 1) == WHITE_PAWN)  
                moves.push_back(NewMove(attacker_pos - 1, attacker_pos + 8, ENPASSANT_MOVE));
            if(col(attacker_pos) < 7 && board->get_piece(attacker_pos + 1) == WHITE_PAWN)    
                moves.push_back(NewMove(attacker_pos + 1, attacker_pos + 8, ENPASSANT_MOVE));
        }

        uint64_t attackers_of_attacker = get_attackers(attacker_pos, board->side, board);

        while(attackers_of_attacker) {
            from_sq = pop_first_bit(attackers_of_attacker);
            moves.push_back(NewMove(from_sq, attacker_pos, CAPTURE_MOVE));
        }

        /* a piece (different than the checked king) will try to block the attack without eating anything */
        uint64_t positions_between = get_between(attacker_pos, king_pos, board);
        while(positions_between) {
            to_sq = pop_first_bit(positions_between);
            uint64_t blockers = get_blockers(to_sq, board->side, board);

            /* special case: we block a square by eating enpass */
            if(board->enpassant == col(to_sq)) {
                if(board->side == WHITE && row(to_sq) == 5) {
                    if(col(to_sq) > 0 && board->get_piece(to_sq - 9) == WHITE_PAWN)
                        moves.push_back(NewMove(to_sq - 9, to_sq, ENPASSANT_MOVE));
                    if(col(to_sq) < 7 && board->get_piece(to_sq - 7) == WHITE_PAWN)
                        moves.push_back(NewMove(to_sq - 7, to_sq, ENPASSANT_MOVE));
                } else if(board->side == BLACK && row(to_sq) == 2) {
                    if(col(to_sq) > 0 && board->get_piece(to_sq + 7) == BLACK_PAWN)
                        moves.push_back(NewMove(to_sq + 7, to_sq, ENPASSANT_MOVE));
                    if(col(to_sq) < 7 && board->get_piece(to_sq + 9) == BLACK_PAWN)
                        moves.push_back(NewMove(to_sq + 9, to_sq, ENPASSANT_MOVE));
                }
            }

            while(blockers) {
                from_sq = pop_first_bit(blockers);
                moves.push_back(NewMove(from_sq, to_sq, QUIET_MOVE));
            }
        }
    }

    /* we just move the king, remember that we will check whether a move is valid later */
    uint64_t attack_mask = king_attacks[king_pos] & ~board->get_side_mask(board->side);
    while(attack_mask) {
        to_sq = pop_first_bit(attack_mask);
        if(board->get_piece(to_sq) != NEW_EMPTY)
            moves.push_back(NewMove(king_pos, to_sq, CAPTURE_MOVE)); 
        else
            moves.push_back(NewMove(king_pos, to_sq, QUIET_MOVE)); 
    }
}

/* we assume that the king isn't in check */
void generate_captures(std::vector<NewMove>& moves, const BitBoard* board) {
    const bool side = board->side;
    const bool xside = board->xside;
    const uint64_t all_mask = board->get_all_mask();
    const uint64_t xside_mask = board->get_side_mask(xside);
    const uint64_t pawn_mask = board->get_pawn_mask(side);
    uint64_t mask, attack_mask;
    int from_sq, to_sq;

    /* first we generate pawn moves */
    if(board->side == WHITE) {

        /* enpassant capture */
        if(board->enpassant != NO_ENPASSANT) {
            int enpassant_sq = 40 + int(board->enpassant);
            assert(board->get_piece(enpassant_sq - 8) == BLACK_PAWN);
            /* eating to the left (sq -> sq + 7) */
            if(board->enpassant < 7 && board->get_piece(enpassant_sq - 7) == WHITE_PAWN)
                moves.push_back(NewMove(enpassant_sq - 7, enpassant_sq, ENPASSANT_MOVE));
            /* eating to the right (sq -> sq + 9) */
            if(board->enpassant > 0 && board->get_piece(enpassant_sq - 9) == WHITE_PAWN)
                moves.push_back(NewMove(enpassant_sq - 9, enpassant_sq, ENPASSANT_MOVE));
        }

        /* promotion eating diagonally to the left (sq -> sq + 7) */
        mask = ((pawn_mask & ROW_6 & ~COL_0) << 7) & xside_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            /* NewMove constructor:  'from'    'to'     'move type'    */
            moves.push_back(NewMove(to_sq - 7, to_sq, QUEEN_PROMOTION));
            // moves.push_back(NewMove(to_sq - 7, to_sq, KNIGHT_PROMOTION));
        }

        /* promotion eating diagonally to the right (sq -> sq + 9) */
        mask = ((pawn_mask & ROW_6 & ~COL_7) << 9) & xside_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq - 9, to_sq, QUEEN_PROMOTION));
            // moves.push_back(NewMove(to_sq - 9, to_sq, KNIGHT_PROMOTION));
        }

        /* promotion front (sq -> sq + 8) */
        mask = ((pawn_mask & ROW_6) << 8) & ~all_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq - 8, to_sq, QUEEN_PROMOTION));
            // moves.push_back(NewMove(to_sq - 8, to_sq, KNIGHT_PROMOTION));
        }

        /* pawn capture to the left (sq -> sq + 7) */
        mask = ((pawn_mask & ~ROW_6 & ~COL_0) << 7) & xside_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq - 7, to_sq, CAPTURE_MOVE));
        }

        /* pawn capture to the right (sq -> sq + 9) */
        mask = ((pawn_mask & ~ROW_6 & ~COL_7) << 9) & xside_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq - 9, to_sq, CAPTURE_MOVE));
        }
    } else {
        /* enpassant capture */
        if(board->enpassant != NO_ENPASSANT) {
            int enpassant_sq = 16 + int(board->enpassant);
            assert(board->get_piece(enpassant_sq + 8) == WHITE_PAWN);
            /* to the left (sq -> sq - 9) */
            if(board->enpassant < 7 && board->get_piece(enpassant_sq + 9) == BLACK_PAWN)
                moves.push_back(NewMove(enpassant_sq + 9, enpassant_sq, ENPASSANT_MOVE));
            /* to the right (sq -> sq - 7) */
            if(board->enpassant > 0 && board->get_piece(enpassant_sq + 7) == BLACK_PAWN)
                moves.push_back(NewMove(enpassant_sq + 7, enpassant_sq, ENPASSANT_MOVE));
        }

        /* promotion eating diagonally to the left (sq -> sq - 9) */
        mask = ((pawn_mask & ROW_1 & ~COL_0) >> 9) & xside_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq + 9, to_sq, QUEEN_PROMOTION));
            // moves.push_back(NewMove(to_sq + 9, to_sq, KNIGHT_PROMOTION));
        }

        /* promotion eating diagonally to the right (sq -> sq - 7) */
        mask = ((pawn_mask & ROW_1 & ~COL_7) >> 7) & xside_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq + 7, to_sq, QUEEN_PROMOTION));
            // moves.push_back(NewMove(to_sq + 7, to_sq, KNIGHT_PROMOTION));
        }

        /* promotion front (sq -> sq - 8) */
        mask = ((pawn_mask & ROW_1) >> 8) & ~all_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq + 8, to_sq, QUEEN_PROMOTION));
            // moves.push_back(NewMove(to_sq + 8, to_sq, KNIGHT_PROMOTION));
        }

        /* pawn capture to the left (sq -> sq - 9) */
        mask = ((pawn_mask & ~ROW_1 & ~COL_0) >> 9) & xside_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq + 9, to_sq, CAPTURE_MOVE));
        }

        /* pawn capture to the right (sq -> sq - 7) */
        mask = ((pawn_mask & ~ROW_1 & ~COL_7) >> 7) & xside_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq + 7, to_sq, CAPTURE_MOVE));
        }
    }

    /* king captures */
    mask = board->get_king_mask(side);
    assert(mask != 0);
    from_sq = pop_first_bit(mask);
    attack_mask = king_attacks[from_sq] & xside_mask;
    while(attack_mask) {
        assert(attack_mask != 0);
        to_sq = pop_first_bit(attack_mask);
        assert(to_sq >= 0 && to_sq < 64);
        moves.push_back(NewMove(from_sq, to_sq, CAPTURE_MOVE));
    }

    assert(mask == 0);

    /* knight captures */
    mask = board->get_knight_mask(side);
    while(mask) {
        from_sq = pop_first_bit(mask);
        attack_mask = knight_attacks[from_sq] & xside_mask;
        while(attack_mask) {
            to_sq = pop_first_bit(attack_mask);
            moves.push_back(NewMove(from_sq, to_sq, CAPTURE_MOVE));
        }
    }

    /* bishop and queen captures */
    mask = board->get_bishop_mask(side) | board->get_queen_mask(side);
    while(mask) {
        from_sq = pop_first_bit(mask);
        attack_mask = Bmagic(from_sq, all_mask) & xside_mask;
        while(attack_mask) {
            to_sq = pop_first_bit(attack_mask);
            moves.push_back(NewMove(from_sq, to_sq, CAPTURE_MOVE));
        }
    }

    /* rook and queen captures */
    mask = board->get_rook_mask(side) | board->get_queen_mask(side);
    while(mask) {
        from_sq = pop_first_bit(mask);
        attack_mask = Rmagic(from_sq, all_mask) & xside_mask;
        while(attack_mask) {
            to_sq = pop_first_bit(attack_mask);
            moves.push_back(NewMove(from_sq, to_sq, CAPTURE_MOVE));
        }
    }
}

/* we assume that the king isn't in check */
void generate_quiet(std::vector<NewMove>& moves, const BitBoard* board) {
    const bool side = board->side;
    const bool xside = board->xside;
    const uint64_t all_mask = board->get_all_mask(); 
    const uint64_t pawn_mask = board->get_pawn_mask(side);
    const uint64_t xside_mask = board->get_all_mask(); 
    uint64_t mask, attack_mask;
    int from_sq, to_sq;

    if(side == WHITE) {
        /* front only one square (no promotion) (sq -> sq + 8) */
        mask = ((pawn_mask & ~ROW_6) << 8) & ~all_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq - 8, to_sq, QUIET_MOVE));
        }
        
        /* frontal two squares (sq -> sq + 8) */
        mask = ((pawn_mask & ROW_1) << 8) & ~all_mask;
        mask = ((mask & ROW_2) << 8) & ~all_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq - 16, to_sq, QUIET_MOVE));
        }
    } else {
        /* front only one square (no promotion) (sq -> sq - 8) */
        mask = ((pawn_mask & ~ROW_1) >> 8) & ~all_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq + 8, to_sq, QUIET_MOVE));
        }

        /* frontal two squares (sq -> sq - 16) */
        mask = ((pawn_mask & ROW_6) >> 8) & ~all_mask;
        mask = ((mask & ROW_5) >> 8) & ~all_mask;
        while(mask) {
            to_sq = pop_first_bit(mask);
            moves.push_back(NewMove(to_sq + 16, to_sq, QUIET_MOVE));
        }
    }

    /* generate the castling moves */
    if(side == WHITE) {
        /* white queen side castling */
        if(board->castling_rights[WHITE_QUEEN_SIDE]
        && !(all_mask & castling_mask[WHITE_QUEEN_SIDE])
        && !board->is_attacked(D1))
            moves.push_back(NewMove(E1, C1, CASTLING_MOVE));
        /* white king side castling */
        if(board->castling_rights[WHITE_KING_SIDE]
        && !(all_mask & castling_mask[WHITE_KING_SIDE])
        && !board->is_attacked(F1)) 
            moves.push_back(NewMove(E1, G1, CASTLING_MOVE));
    } else if(side == BLACK) {
        /* black queen side castling */
        if(board->castling_rights[BLACK_QUEEN_SIDE]
        && !(all_mask & castling_mask[BLACK_QUEEN_SIDE])
        && !board->is_attacked(D8))
            moves.push_back(NewMove(E8, C8, CASTLING_MOVE));
        /* black king side castling */
        if(board->castling_rights[BLACK_KING_SIDE]
        && !(all_mask & castling_mask[BLACK_KING_SIDE])
        && !board->is_attacked(F8))
            moves.push_back(NewMove(E8, G8, CASTLING_MOVE));
    }

    // int moves_prev = (int)moves.size();

    /* king */
    mask = board->get_king_mask(side);
    from_sq = pop_first_bit(mask);  
    attack_mask = king_attacks[from_sq] & ~all_mask;
    while(attack_mask) {
        to_sq = pop_first_bit(attack_mask);
        moves.push_back(NewMove(from_sq, to_sq, QUIET_MOVE));
    }

    // std::cout << moves.size() - moves_prev << " king moves have been generated" << endl;
    // std::cout << "These are the king moves:" << endl;
    // for(int i = moves.size() - 1; i >= moves_prev; i--) {
    //     std::cout << move_to_str(Move(moves[i].get_from(), moves[i].get_to())) << endl;
    // }
    // moves_prev = moves.size();

    /* knight */
    mask = board->get_knight_mask(side);
    while(mask) {
        from_sq = pop_first_bit(mask);
        attack_mask = knight_attacks[from_sq] & ~all_mask;
        while(attack_mask) {
            to_sq = pop_first_bit(attack_mask);
            moves.push_back(NewMove(from_sq, to_sq, QUIET_MOVE));
        }
    }

    // std::cout << moves.size() - moves_prev << " knight moves have been generated" << endl;
    // std::cout << "These are the knight moves:" << endl;
    // for(int i = moves.size() - 1; i >= moves_prev; i--) {
    //     std::cout << move_to_str(Move(moves[i].get_from(), moves[i].get_to())) << endl;
    // }
    // moves_prev = moves.size();

    /* bishop and queen */
    mask = board->get_bishop_mask(side) | board->get_queen_mask(side);
    while(mask) {
        from_sq = pop_first_bit(mask);
        attack_mask = Bmagic(from_sq, all_mask) & ~all_mask;
        while(attack_mask) {
            to_sq = pop_first_bit(attack_mask);
            moves.push_back(NewMove(from_sq, to_sq, QUIET_MOVE));
        }
    }

    // std::cout << moves.size() - moves_prev << " bishop and queen moves have been generated" << endl;
    // std::cout << "These are the bq moves:" << endl;
    // for(int i = moves.size() - 1; i >= moves_prev; i--) {
    //     std::cout << move_to_str(Move(moves[i].get_from(), moves[i].get_to())) << endl;
    // }
    // moves_prev = moves.size();

    /* rooks and queen */
    mask = board->get_rook_mask(side) | board->get_queen_mask(side);
    while(mask) {
        from_sq = pop_first_bit(mask);
        attack_mask = Rmagic(from_sq, all_mask) & ~all_mask;
        while(attack_mask) {
            to_sq = pop_first_bit(attack_mask);
            moves.push_back(NewMove(from_sq, to_sq, QUIET_MOVE));
        }
    }

    // std::cout << moves.size() - moves_prev << " rook and queen moves have been generated" << endl;
    // std::cout << "These are the rq moves:" << endl;
    // for(int i = moves.size() - 1; i >= moves_prev; i--) {
    //     std::cout << move_to_str(Move(moves[i].get_from(), moves[i].get_to())) << endl;
    // }
    // moves_prev = moves.size();
}