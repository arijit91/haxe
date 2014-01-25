#include<map>
#include<queue>
#include<vector>
#include<cstring>
#include <string.h>
#include <algorithm>

#include "dropblox_ai.h"

using namespace json;
using namespace std;

#define INF 1000000000

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

void Block::set_position(posn pos) {
  translation.i = pos.tx;
  translation.j = pos.ty;
  rotation = pos.rot;
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

  heuristic_params[0] = .0;
  heuristic_params[1] = .0;
  heuristic_params[2] = .0;
  heuristic_params[3] = .0;
  heuristic_params[4] = .0;
  heuristic_params[5] = .0;
  heuristic_params[6] = .0;
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
  int vis[100][100][4];
  int SHIFT = 50;

  int tx, ty, rot;
  tx = ty = rot = 0;

  memset(vis, -1, sizeof(vis));

  Q.push(tx); Q.push(ty); Q.push(rot);

  posn pos(tx, ty, rot);
  commands[pos] = empty;

  vis[tx + SHIFT][ty + SHIFT][rot] = 1;

  while (!Q.empty()) {
    tx = Q.front(); Q.pop();
    ty = Q.front(); Q.pop();
    rot = Q.front(); Q.pop();
   
    posn pos(tx, ty, rot);
    vector<string> cmd;

    cmd = commands[pos];
    tx = pos.tx; ty = pos.ty; rot = pos.rot;
    block->set_position(tx, ty, rot);
    ty += 1;
    block->right();
    cmd.push_back("right");
    if (check(*block) && vis[tx + SHIFT][ty + SHIFT][rot] == -1) {
      vis[tx+SHIFT][ty+SHIFT][rot] = 1;
      commands[posn(tx, ty, rot)] = cmd;
      Q.push(tx); Q.push(ty); Q.push(rot);
    }

    cmd = commands[pos];
    tx = pos.tx; ty = pos.ty; rot = pos.rot;
    block->set_position(tx, ty, rot);
    ty -= 1;
    block->left();
    cmd.push_back("left");
    if (check(*block) && vis[tx+SHIFT][ty+SHIFT][rot] == -1) {

      vis[tx+SHIFT][ty+SHIFT][rot] = 1;
      commands[posn(tx, ty, rot)] = cmd;
      Q.push(tx); Q.push(ty); Q.push(rot);
    }

    cmd = commands[pos];
    tx = pos.tx; ty = pos.ty; rot = pos.rot;
    block->set_position(tx, ty, rot);
    tx += 1;
    block->down();
    cmd.push_back("down");
    if (check(*block) && vis[tx+SHIFT][ty+SHIFT][rot] == -1) {

      vis[tx+SHIFT][ty+SHIFT][rot] = 1;
      commands[posn(tx, ty, rot)] = cmd;
      Q.push(tx); Q.push(ty); Q.push(rot);
    }

    cmd = commands[pos];
    tx = pos.tx; ty = pos.ty; rot = pos.rot;
    block->set_position(tx, ty, rot);
    tx -= 1;
    block->up();
    cmd.push_back("up");
    if (check(*block) && vis[tx+SHIFT][ty+SHIFT][rot] == -1) {

      vis[tx+SHIFT][ty+SHIFT][rot] = 1;
      commands[posn(tx, ty, rot)] = cmd;
      Q.push(tx); Q.push(ty); Q.push(rot);
    }

    cmd = commands[pos];
    tx = pos.tx; ty = pos.ty; rot = pos.rot;
    block->set_position(tx, ty, rot);
    rot = (rot + 1) % 4;
    block->rotate();
    cmd.push_back("rotate");
    if (check(*block) && vis[tx+SHIFT][ty+SHIFT][rot] == -1) {

      vis[tx+SHIFT][ty+SHIFT][rot] = 1;
      commands[posn(tx, ty, rot)] = cmd;
      Q.push(tx); Q.push(ty); Q.push(rot);
    }
  }

  block->reset_position();
}

void print_moves(vector<string>& moves) {
  for (int i = 0; i < moves.size(); i++)
    cout<<moves[i]<<endl;
}

void Board::choose_move() {
  vector<string> best;
  float min_score = INF;
  for (map<posn, vector<string> >::iterator it = commands.begin();
    it != commands.end(); it++) {
    posn pos = it -> first;

    //cout<<pos.tx<<" "<<pos.ty<<" "<<pos.rot<<endl;

    vector<string> moves = it -> second;
    //cout<<moves.size()<<"\n";

    block->set_position(pos);
    place();

    float score = get_score(bitmap);
    if (score <= min_score) {
      min_score = score;
      best = moves;
    }
  }

  print_moves(best);
}

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
      if (newState[row][col] != 0) {
          has_ceiling = 1;
      }
    }
  }
  return hole_count;
}

int Board::altitude(Bitmap &newState) {
  int res = 0;
  for (int i = ROWS - 1; i >= 0; i--){
    bool success = false;
    for (int j = 0; j < COLS; j++) {
      if (newState[i][j]) {
        success = true;
        break;
      }
    }

    if (success) {
         res++;
    } else {
      break;
    }
  }
  return res;
}

int dirX[] = {-1, 0, 0, 1};
int dirY[] = {0, 1, -1, 0};

inline
bool sameColor(int x1, int y1, int x2, int y2, Bitmap &newState) {
  if (newState[x1][y1] == 0 && newState[x2][y2] == 0)
    return true;

  if (newState[x1][y1] != 0 && newState[x2][y2] != 0)
    return true;

  return false;
}

void dfs(Bitmap &newState, int x, int y, vector< vector<bool> > &visited) {

  if (visited[x][y])
    return;

  visited[x][y] = true;
  for (int i = 0; i < 4; i++) {
    int newX = x + dirX[i];
    int newY = y + dirY[i];
    if (newX < 0 || newX >= ROWS || newY < 0 || newY >= COLS) {
      continue;
    }

    if (sameColor(newX, newY, x, y, newState)) {
      dfs(newState, newX, newY, visited);
    }
  }
}
int Board::countComponents(Bitmap &newState) {

  int res = 0;
  vector< vector<bool> > visited(ROWS, vector<bool>(COLS, false));

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      if (visited[i][j])
        continue;
      // cout << i << "\t" << j << endl;
      res++;
      dfs(newState, i, j, visited);
    }
  }
  return res;
}

int Board::roughness(Bitmap &newState) {
     int row, col, has_ceiling = 0;
     unsigned int sum_height = 0;
  
     for (col = 0; col < COLS; col++) {
          for (row = 0; row < ROWS; row++) {
               if (newState[row][col] != 0) {
                    unsigned int height_left = 0;
                    unsigned int height_right = 0;

                    // No ceiling on left
                    int row_iter = row;
                    if (col-1 >= 0) {
                         has_ceiling = 0;
                         for (int i = row_iter - 1; i >= 0; i--) {
                              if (newState[i][col-1] != 0) {
                                   has_ceiling = 1;
                                   break;
                              }
                         }
                    
                         if (! has_ceiling) {
                              while (row_iter < ROWS &&
                                     newState[row_iter++][col-1] == 0) {
                                   height_left++;
                              }
                         }
                    }
                    
                    

                    if (col+1 < COLS) {
                         row_iter = row;
                         has_ceiling = 0;
                         for (int i = row_iter - 1; i >= 0; i--) {
                              if (newState[i][col+1] != 0) {
                                   has_ceiling = 1;
                                   break;
                              }
                         }
                         if (! has_ceiling) {
                              while (row_iter < ROWS &&
                                     newState[row_iter++][col+1] == 0) {
                                   height_right++;
                              }
                         }
                    }
                    
                    sum_height += (height_right + height_left);
                    break;
               }
          }
     }
     return sum_height;
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

int Board::higher_slope(Bitmap& newState)
{
     int row, col, has_ceiling = 0;
     unsigned int max_height = 0;
  
     for (col = 0; col < COLS; col++) {
          for (row = 0; row < ROWS; row++) {
               if (newState[row][col] != 0) {
                    unsigned int height_left = 0;
                    unsigned int height_right = 0;

                    // No ceiling on left
                    int row_iter = row;
                    if (col-1 >= 0) {
                         has_ceiling = 0;
                         for (int i = row_iter - 1; i >= 0; i--) {
                              if (newState[i][col-1] != 0) {
                                   has_ceiling = 1;
                                   break;
                              }
                         }
                    
                         if (! has_ceiling) {
                              while (row_iter < ROWS &&
                                     newState[row_iter++][col-1] == 0) {
                                   height_left++;
                              }
                         }
                    }
                    
                    

                    if (col+1 < COLS) {
                         row_iter = row;
                         has_ceiling = 0;
                         for (int i = row_iter - 1; i >= 0; i--) {
                              if (newState[i][col+1] != 0) {
                                   has_ceiling = 1;
                                   break;
                              }
                         }
                         if (! has_ceiling) {
                              while (row_iter < ROWS &&
                                     newState[row_iter++][col+1] == 0) {
                                   height_right++;
                              }
                         }
                    }
                    
                    
           
                    max_height = max (max_height, height_right);
                    max_height = max (max_height, height_left);
                    break;
               }
          }
     }
     return max_height;
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
  score += heuristic_params[6]*countComponents(newState);
  return score;
}

int test () 
{Bitmap bitmap = {
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8},
            {1, 1, 0, 4, 0, 2, 2, 0, 0, 0, 0, 9},
            {0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8},
     };
     cout << Board::count_holes (bitmap) << endl;
     cout << Board::altitude (bitmap) << endl;
     cout << Board::full_cells (bitmap) << endl;
     cout << Board::higher_slope (bitmap) << endl;
     cout << Board::roughness (bitmap) << endl;
     cout << Board::full_cells_weighted (bitmap) << endl;
     cout << Board::countComponents (bitmap) << endl;

     return 0;
}

int main(int argc, char** argv) {
     // test ();
     // return 0;

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
