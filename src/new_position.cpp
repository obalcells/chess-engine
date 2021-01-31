#include <iostream>
#include <string>
#include <cassert>
#include "new_position.h"
#include "new_board.h"
#include "new_defs.h"
#include "defs.h"

NewPosition::NewPosition() {
    board_history.push_back(static_cast<BitBoard>(BitBoard()));
}

NewPosition::NewPosition(const std::string& fen) {
    board_history.push_back(static_cast<BitBoard>(BitBoard(fen)));
}

NewPosition::~NewPosition() {
    board_history.clear();
}

void NewPosition::set_from_fen(const std::string& fen) {
	board_history.clear();
	board_history.push_back(static_cast<BitBoard>(BitBoard(fen)));
	/* reset the other variables, which we don't have yet */
}

BitBoard& NewPosition::get_board() {
	return board_history.back();
}

uint64_t NewPosition::get_key() const {
	return board_history.back().key;
}

int NewPosition::get_move_count() const {
	return board_history.back().move_count;
}

void NewPosition::make_move(const NewMove& move) {
    board_history.push_back(static_cast<BitBoard>(board_history.back()));
    move_history.push_back(static_cast<NewMove>(move));
    BitBoard& board = board_history.back();
    assert(board.key == board.calculate_key());
    board.make_move(move);
    board.update_key(board_history[board_history.size() - 2], move);
    if(false && board.key != board.calculate_key()) {
        std::cerr << "Move is: " << move_to_str(Move(move.get_from(), move.get_to())) << endl;
        std::cerr << "Board was:" << endl;
        board_history[board_history.size() - 2].print_board();
        std::cerr << "Board now is" << endl;
        board.print_board();
        assert(board.key == board.calculate_key());
    }
}

NewMove NewPosition::pair_to_move(int from_sq, int to_sq) {
    BitBoard& board = board_history.back(); 
    int flags = 0;
    int piece = board.get_piece(from_sq);
    bool side = board.side;

    if(piece == NEW_EMPTY)
        return NewMove();

    if((piece == BLACK_PAWN || piece == WHITE_PAWN) && (row(to_sq) == 0 || row(to_sq) == 7)) {
        if((side == WHITE && row(to_sq) == 0) || (side == BLACK && row(to_sq) == 7))
            return NewMove();
        flags = QUEEN_PROMOTION;
        /*
        std::cout << "Which piece should the pawn be promoted to (q, r, b, k)? " << endl;
        std::string piece_str;
        std::cin >> piece_str;
        switch(piece) {
            case 'q':
                flags = QUEEN_PROMOTION;
                break;
            case 'r':
                flags = ROOK_PROMOTION;
                break;
            case 'b':
                flags = BISHOP_PROMOTION;
                break;
            case 'k':
                flags = KNIGHT_PROMOTION;
                break;
            default:
                std::cout << "Invalid piece" << endl;
                return false;
        }
        */
    } else if((piece == WHITE_KING || piece == BLACK_KING) && abs(col(from_sq) - col(to_sq)) == 2) {
        flags = CASTLING_MOVE;
    } else if((piece == WHITE_PAWN || piece == BLACK_PAWN) && abs(col(from_sq) - col(to_sq)) == 1 && board.get_piece(to_sq) == NEW_EMPTY) {
        flags = ENPASSANT_MOVE;
    } else if(board.get_piece(to_sq) != NEW_EMPTY) {
        flags = CAPTURE_MOVE;
    } else {
        flags = QUIET_MOVE;
    }

    return NewMove(from_sq, to_sq, flags);
}

/* returns false if move is invalid, otherwise it applies the move and returns true */
bool NewPosition::make_move_from_str(const std::string& str_move) {
    if(str_move.size() != 4
    || (str_move[0] < 'a' || str_move[0] > 'h')
    || (str_move[1] < '1' || str_move[1] > '8')
    || (str_move[2] < 'a' || str_move[2] > 'h')
    || (str_move[3] < '1' || str_move[3] > '8'))
        return false;

    BitBoard& board = board_history.back(); 
    int from_sq = (str_move[1] - '1') * 8 + (str_move[0] - 'a');
    int to_sq = (str_move[3] - '1') * 8 + (str_move[2] - 'a');
    int flags = 0;
    int piece = board.get_piece(from_sq);
    bool side = board.side;

    if(piece == NEW_EMPTY)
        return false;

    if((piece == BLACK_PAWN || piece == WHITE_PAWN) && (row(to_sq) == 0 || row(to_sq) == 7)) {
        if((side == WHITE && row(to_sq) == 0) || (side == BLACK && row(to_sq) == 7))
            return false;
        flags = QUEEN_PROMOTION;
        /*
        std::cout << "Which piece should the pawn be promoted to (q, r, b, k)? " << endl;
        std::string piece_str;
        std::cin >> piece_str;
        switch(piece) {
            case 'q':
                flags = QUEEN_PROMOTION;
                break;
            case 'r':
                flags = ROOK_PROMOTION;
                break;
            case 'b':
                flags = BISHOP_PROMOTION;
                break;
            case 'k':
                flags = KNIGHT_PROMOTION;
                break;
            default:
                std::cout << "Invalid piece" << endl;
                return false;
        }
        */
    } else if((piece == WHITE_KING || piece == BLACK_KING) && abs(col(from_sq) - col(to_sq)) == 2) {
        flags = CASTLING_MOVE;
    } else if((piece == WHITE_PAWN || piece == BLACK_PAWN) && abs(col(from_sq) - col(to_sq)) == 1 && board.get_piece(to_sq) == NEW_EMPTY) {
        flags = ENPASSANT_MOVE;
    } else if(board.get_piece(to_sq) != NEW_EMPTY) {
        flags = CAPTURE_MOVE;
    } else {
        flags = QUIET_MOVE;
    }

    NewMove move = NewMove(from_sq, to_sq, flags);


    if(!board.move_valid(move))
        return false;

    board.quick_check("Inside make move but before make_move");

    make_move(move);

    return true;
}

void NewPosition::print_board() const {
    board_history.back().print_board();
}

bool NewPosition::move_valid(const NewMove& move) {
    return board_history.back().move_valid(move);
}

/* returns false if move is invalid, otherwise it applies the move and returns true */
bool NewPosition::move_valid(int from_sq, int to_sq) {
    BitBoard& board = board_history.back(); 
    int flags = 0;
    int piece = board.get_piece(from_sq);
    bool side = board.side;

    if(piece == NEW_EMPTY)
        return false;

    if((piece == WHITE_PAWN || piece == BLACK_PAWN) && (row(to_sq) == 0 || row(to_sq) == 7)) {
        if((side == WHITE && row(to_sq) == 0) || (side == BLACK && row(to_sq) == 7))
            return false;
        flags = QUEEN_PROMOTION;
    } else if((piece == WHITE_KING || piece == BLACK_KING) && abs(col(from_sq) - col(to_sq)) == 2) {
        flags = CASTLING_MOVE;
    } else if((piece == WHITE_PAWN || piece == BLACK_PAWN) && abs(col(from_sq) - col(to_sq)) == 1 && board.get_piece(to_sq) == NEW_EMPTY) {
        flags = ENPASSANT_MOVE;
    } else if(board.get_piece(to_sq) != NEW_EMPTY) {
        flags = CAPTURE_MOVE;
    } else {
        flags = QUIET_MOVE;
    }

    NewMove move = NewMove(from_sq, to_sq, flags);

    assert(move.get_to() == to_sq);

    return board.move_valid(move);
}

/* take back the last move in move history */
void NewPosition::take_back() {
    assert(!board_history.empty());
    board_history.pop_back();
}

bool NewPosition::in_check() const {
    return board_history.back().in_check();
}

void NewPosition::debug(int cnt) const {
    std::cout << RED_COLOR << "Stack trace" << RESET_COLOR << endl;
    for(int i = (int)board_history.size() - cnt; i < (int)board_history.size(); i++) {
        board_history[i].print_board();
        if(i < (int)move_history.size()) {
            NewMove move = static_cast<NewMove>(move_history[i]);
            std::cout << GREEN_COLOR << move << RESET_COLOR << endl;
        }
    }
}

bool NewPosition::only_kings_left() const {
    uint64_t all_mask = board_history.back().get_all_mask();
    pop_first_bit(all_mask);
    pop_first_bit(all_mask);
    if(all_mask == 0)
        return true;
    return false;
}