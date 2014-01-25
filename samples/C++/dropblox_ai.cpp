#include<map>
#include<queue>
#include<vector>
#include<cstring>
#include <string.h>

#include "dropblox_ai.h"

using namespace json;
using namespace std;

//----------------------------------
// Block implementation starts here!
//----------------------------------

Block::Block(Object& raw_block) {
  center.i = (int)(Number&)raw_block["center"]["i"];
  center.j = (int)(Number&)raw_block["center"]["j"];
  size = 0;

  Array& raw_offsets = raw_block["offsets"];
  for (Array::const_iterator it = raw_offsets.Begin(); it < raw_offsets.End(); it++) {
    size += 1;
  }
  for (int i = 0; i < size; i++) {
    offsets[i].i = (Number&)raw_offsets[i]["i"];
    offsets[i].j = (Number&)raw_offsets[i]["j"];
  }

  translation.i = 0;
  translation.j = 0;
  rotation = 0;
}

void Block::left() {
  translation.j -= 1;
}

void Block::right() {
  translation.j += 1;
}

void Block::up() {
  translation.i -= 1;
}

void Block::down() {
  translation.i += 1;
}

void Block::rotate() {
  rotation += 1;
}

void Block::unrotate() {
  rotation -= 1;
}

// The checked_* methods below perform an operation on the block
// only if it's a legal move on the passed in board.  They
// return true if the move succeeded.
//
// The block is still assumed to start in a legal position.
bool Block::checked_left(const Board& board) {
  left();
  if (board.check(*this)) {
    return true;
  }
  right();
  return false;
}

bool Block::checked_right(const Board& board) {
  right();
  if (board.check(*this)) {
    return true;
  }
  left();
  return false;
}

bool Block::checked_up(const Board& board) {
  up();
  if (board.check(*this)) {
    return true;
  }
  down();
  return false;
}

bool Block::checked_down(const Board& board) {
  down();
  if (board.check(*this)) {
    return true;
  }
  up();
  return false;
}

bool Block::checked_rotate(const Board& board) {
  rotate();
  if (board.check(*this)) {
    return true;
  }
  unrotate();
  return false;
}

void Block::do_command(const string& command) {
  if (command == "left") {
    left();
  } else if (command == "right") {
    right();
  } else if (command == "up") {
    up();
  } else if (command == "down") {
    down();
  } else if (command == "rotate") {
    rotate();
  } else {
    throw Exception("Invalid command " + command);
  }
}

void Block::do_commands(const vector<string>& commands) {
  for (int i = 0; i < commands.size(); i++) {
    do_command(commands[i]);
  }
}

void Block::reset_position() {
  translation.i = 0;
  translation.j = 0;
  rotation = 0;
}

void Block::set_position(int a, int b, int c) {
  translation.i = a;
  translation.j = b;
  rotation = c;
}

//----------------------------------
// Board implementation starts here!
//----------------------------------

Board::Board() {
  rows = ROWS;
  cols = COLS;

  heuristic_params[0] = 1.0;
  heuristic_params[1] = 1.0;
  heuristic_params[2] = 1.0;
  heuristic_params[3] = 1.0;
  heuristic_params[4] = 1.0;
  heuristic_params[5] = 1.0;
}

Board::Board(Object& state) {
  Board();

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      bitmap[i][j] = ((int)(Number&)state["bitmap"][i][j] ? 1 : 0);
    }
  }

  // Note that these blocks are NEVER destructed! This is because calling
  // place() on a board will create new boards which share these objects.
  //
  // There's a memory leak here, but it's okay: blocks are only constructed
  // when you construct a board from a JSON Object, which should only happen
  // for the very first board. The total memory leaked will only be ~10 kb.
  block = new Block(state["block"]);
  for (int i = 0; i < PREVIEW_SIZE; i++) {
    preview.push_back(new Block(state["preview"][i]));
  }
}

// Returns true if the `query` block is in valid position - that is, if all of
// its squares are in bounds and are currently unoccupied.
bool Board::check(const Block& query) const {
  Point point;
  for (int i = 0; i < query.size; i++) {
    point.i = query.center.i + query.translation.i;
    point.j = query.center.j + query.translation.j;
    if (query.rotation % 2) {
      point.i += (2 - query.rotation)*query.offsets[i].j;
      point.j +=  -(2 - query.rotation)*query.offsets[i].i;
    } else {
      point.i += (1 - query.rotation)*query.offsets[i].i;
      point.j += (1 - query.rotation)*query.offsets[i].j;
    }
    if (point.i < 0 || point.i >= ROWS ||
        point.j < 0 || point.j >= COLS || bitmap[point.i][point.j]) {
      return false;
    }
  }
  return true;
}

// Resets the block's position, moves it according to the given commands, then
// drops it onto the board. Returns a pointer to the new board state object.
//
// Throws an exception if the block is ever in an invalid position.
Board* Board::do_commands(const vector<string>& commands) {
  block->reset_position();
  if (!check(*block)) {
    throw Exception("Block started in an invalid position");
  }
  for (int i = 0; i < commands.size(); i++) {
    if (commands[i] == "drop") {
      return place();
    } else {
      block->do_command(commands[i]);
      if (!check(*block)) {
        throw Exception("Block reached in an invalid position");
      }
    }
  }
  // If we've gotten here, there was no "drop" command. Drop anyway.
  return place();
}

// Drops the block from whatever position it is currently at. Returns a
// pointer to the new board state object, with the next block drawn from the
// preview list.
//
// Assumes the block starts out in valid position.
// This method translates the current block downwards.
//
// If there are no blocks left in the preview list, this method will fail badly!
// This is okay because we don't expect to look ahead that far.
Board* Board::place() {
  Board* new_board = new Board();

  while (check(*block)) {
    block->down();
  }
  block->up();

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      new_board->bitmap[i][j] = bitmap[i][j];
    }
  }

  Point point;
  for (int i = 0; i < block->size; i++) {
    point.i = block->center.i + block->translation.i;
    point.j = block->center.j + block->translation.j;
    if (block->rotation % 2) {
      point.i += (2 - block->rotation)*block->offsets[i].j;
      point.j +=  -(2 - block->rotation)*block->offsets[i].i;
    } else {
      point.i += (1 - block->rotation)*block->offsets[i].i;
      point.j += (1 - block->rotation)*block->offsets[i].j;
    }
    new_board->bitmap[point.i][point.j] = 1;
  }
  Board::remove_rows(&(new_board->bitmap));

  new_board->block = preview[0];
  for (int i = 1; i < preview.size(); i++) {
    new_board->preview.push_back(preview[i]);
  }

  return new_board;
}

// A static method that takes in a new_bitmap and removes any full rows from it.
// Mutates the new_bitmap in place.
void Board::remove_rows(Bitmap* new_bitmap) {
  int rows_removed = 0;
  for (int i = ROWS - 1; i >= 0; i--) {
    bool full = true;
    for (int j = 0; j < COLS; j++) {
      if (!(*new_bitmap)[i][j]) {
        full = false;
        break;
      }
    }
    if (full) {
      rows_removed += 1;
    } else if (rows_removed) {
      for (int j = 0; j < COLS; j++) {
        (*new_bitmap)[i + rows_removed][j] = (*new_bitmap)[i][j];
      }
    }
  }
  for (int i = 0; i < rows_removed; i++) {
    for (int j = 0; j < COLS; j++) {
      (*new_bitmap)[i][j] = 0;
    }
  }
}

struct posn {
  int tx;
  int ty;
  int rot;

  posn(int a, int b, int c) {
    tx = a; ty = b; rot = c;
  }
};

bool operator<(const posn& a, const posn& b) {
  if (a.tx != b.tx) return (a.tx < b.tx);
  if (a.ty != b.ty) return (a.ty < b.ty);
  return (a.rot < b.rot);
}

map<posn, vector<string> > commands;

vector<pair<vector<string>, Board*> > valid_moves;

void Board::generate_moves() {
  vector<string> empty;
  queue<int> Q;
  int vis[ROWS][COLS][4];

  int tx, ty, rot;
  tx = ty = rot = 0;

  memset(vis, -1, sizeof(vis));

  Q.push(tx); Q.push(ty); Q.push(rot);

  posn pos(tx, ty, rot);
  commands[pos] = empty;

  vis[tx][ty][rot] = 1;

  while (!Q.empty()) {
    tx = Q.front(); Q.pop();
    ty = Q.front(); Q.pop();
    rot = Q.front(); Q.pop();
   
    posn pos(tx, ty, rot);
    vector<string> cmd = commands[pos];

    block->set_position(tx, ty, rot);
    ty += 1;
    block->right();
    cmd.push_back("right");
    if (check(*block) && vis[tx][ty][rot] == 0) {
      vis[tx][ty][rot] = 1;
      //commands[posn(tx, ty, rot)] = cmd;
      Q.push(tx); Q.push(ty); Q.push(rot);
    }
  }

  block->reset_position();
}

void Board::choose_move() {
  //vector<string> best;
  //for (map<posn, vector<string> >::iterator it = commands.begin();
  //  it != commands.end(); it++) {
  //  posn pos = it -> first;
  //  vector<string> commands = it -> second;
  //}
int Board::count_holes(Bitmap& newState) 
{
  // A cell is a hole if it is empty but somewhere above it, there is
  // block or part of a block.
  int row, col, has_ceiling;
  unsigned int hole_count = 0;
  for (col = 0; col < COLS; col++) {
    has_ceiling = 0;
    for (row = 0; row < ROWS; row++) {
      if ((newState[row][col] == 0) &&
         (has_ceiling == 1)) {
          hole_count++;
      }
      if (newState[row][col] == 1) {
          has_ceiling = 1;
      }
    }
  }
  return hole_count;
}

int Board::altitude(Bitmap &newState) {
  int res = 0;
  for (int i = ROWS - 1; i >= 0; i++) {
    bool success = false;
    for (int j = 0; j < COLS; j++) {
      if (newState[i][j]) {
        success = true;
        break;
      }
    }

    if (success) {
      res = i + 1;
    } else {
      break;
    }
  }
  return res;
}

int Board::roughness(Bitmap &newState) {
  int res = 0;
  bool visited[ROWS][COLS];
  memset(visited, 0, sizeof visited);

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      if(newState[i][j]) {
        if ( (j - 1) >= 0 && !visited[i][j - 1] && !newState[i][j - 1]) {
          int height = i;
          int temp = 0;
          while (height < ROWS && !newState[height][j - 1]) {
            visited[height][j - 1] = true;
            temp++;
            height ++;
          }
          res += temp;
        }

        if ( (j + 1) < COLS && !visited[i][j+1] && !newState[i][j+1]) {
          int height = i;
          int temp = 0;
          while (height < ROWS && !newState[height][j + 1]) {
            visited[height][j + 1] = true;
            temp ++;
            height ++;
          }

          res += temp;
        }
      }
    }
  }
  return res;
}

int Board::full_cells(Bitmap& newState) {
  int count = 0;
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      if (newState[i][j] != 0) count++;
    }
  }
  return count;
}

int Board::higher_slope(Bitmap& newState) {
  int count = 0;
  for (int i = ROWS-1; i >= 0; i--) {
    int flag = 0;
    for (int j = 0; j < COLS; j++) {
      if (newState[i][j] != 0) {
        flag = 1;
        break;
      }
    }
    if (!flag) break;
    count++;
  }
  return count;
}

int Board::full_cells_weighted(Bitmap& newState) {
  int count = 0;
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      if (newState[i][j] != 0) count += ROWS-i;
    }
  }
  return count;
}

float Board::get_score(Bitmap& newState) {
  float score = 0.0;
  score += heuristic_params[0]*count_holes(newState);
  score += heuristic_params[1]*altitude(newState);
  score += heuristic_params[2]*full_cells(newState);
  score += heuristic_params[3]*higher_slope(newState);
  score += heuristic_params[4]*roughness(newState);
  score += heuristic_params[5]*full_cells_weighted(newState);
  return score;
}

int main(int argc, char** argv) {
  // Construct a JSON Object with the given game state.
  istringstream raw_state(argv[1]);
  Object state;
  Reader::Read(state, raw_state);

  // Construct a board from this Object.
  Board board(state);

  board.generate_moves();
  board.choose_move();

  // // Make some moves!
  // vector<string> moves;
  // while (board.check(*board.block)) {
  //   board.block->left();
  //   moves.push_back("left");
  // }
  // // Ignore the last move, because it moved the block into invalid
  // // position. Make all the rest.
  // for (int i = 0; i < moves.size() - 1; i++) {
  //   cout << moves[i] << endl;
  // }
}
