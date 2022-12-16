#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

// global variables, does not use #define because they can be changed with arguments
uint8_t ROWS = 16,
	COLS = 64,

	PADDLE_X = 2,
	PADDLE_Y = 3,

	BALL_SIZE = 1,

	BOT_FOW = 6, // bot fog of war. higher means easier opponent
	UPDATE_FREQUENCY = 50, // game status updates every X frames (default)
			       // affects: bots, ball
	_UPDATE_FREQUENCY; // changed throughout game

unsigned long long updates = 0;

bool pvp = false,
     eve = false,
     debug = false;

struct paddle{
  uint8_t x, y;
  unsigned long long score;
} player[2];

struct _ball{
  uint8_t x, y;
  int dx, dy;
} ball;

//todo: once game is running, only update what isn't bounds or background
void clear_grid(uint8_t grid[ROWS][COLS]){
  for(uint8_t y = 0; y < ROWS; y++){
    for(uint8_t x = 0; x < COLS; x++){
      if(y == 0 || y == ROWS-1) grid[y][x] = 1; 
      else grid[y][x] = 0;
    }
  }
}

void draw_game(uint8_t grid[ROWS][COLS]){
  clear_grid(grid);
  printf("\033[H");
  
  // insert players
  for(uint8_t i = 0; i < 2; i++){
    for(uint8_t y = player[i].y; y < player[i].y + PADDLE_Y; y++){
      for(uint8_t x = player[i].x; x < player[i].x + PADDLE_X; x++){
	grid[y][x] = 2;
      }
    }
  }

  // insert ball
  for(uint8_t y = ball.y; y < ball.y + BALL_SIZE; y++){
    for(uint8_t x = ball.x; x < ball.x + BALL_SIZE; x++){
      grid[y][x] = 3;
    }
  }

  // draw values
  for(uint8_t y = 0; y < ROWS; y++){
    for(uint8_t x = 0; x < COLS; x++){
      switch(grid[y][x]){
	case 0:
	  printf(" "); // background
	  break;

	case 1:
	  printf("-"); // top/bottom limits
	  break;

	case 2:
	  printf("|"); // paddle
	  break;

	case 3:
	  printf("#"); // ball
	  break;
  
	default:
	    printf("?");
      }
    }
    printf("\n");
  }

  // draw scores in the middle
  uint8_t score1_length = floor(log10(player[0].score));
  for(int i = 1; i < (COLS/2)-(score1_length+1); i++){
    printf(" ");
  }
  printf("%llu | %llu", player[0].score, player[1].score);

  // debug stuff
  if(debug){
    printf("\n");
    printf("\n Player 1: (%hhu, %hhu)    |    Player 2: (%hhu, %hhu)               ", player[0].x, player[0].y, player[1].x, player[1].y);
    printf("\n Ball: (%hhu (%d), %hhu (%d))                         ", ball.x, ball.dx, ball.y, ball.dy);
    printf("\n Updates: %llu  |  Game Update Frequency: %hhu                     ", updates, _UPDATE_FREQUENCY);
  }
}

void move_player(uint8_t index, bool direction){
  if (!direction){ // move up
    if(player[index].y >= 2) player[index].y -= 1;
    return;
  }
  // move down
  if(player[index].y + PADDLE_Y <= ROWS-2)player[index].y += 1;
}

void update_ball(){
  // player collision
  for(int i = 0; i < 2; i++){
    bool near_ball = (player[i].x > COLS/2) ?            // paddle is on the right?
		     (ball.x >= player[i].x - PADDLE_X)  // right paddle
                   : (ball.x <= player[i].x + PADDLE_X); // left paddle

    if(near_ball){
      if(ball.y >= player[i].y && ball.y <= player[i].y + PADDLE_Y){ // paddle hit ball
	ball.dx *= -1; 
	ball.x += ball.dx;
	if(_UPDATE_FREQUENCY > 1) _UPDATE_FREQUENCY--; // game gets progressively faster after each hit
      }

      else{ // paddle missed ball
	ball.x = (COLS/2)-BALL_SIZE;
	ball.y = (ROWS/2)-BALL_SIZE;
	player[1].score++;
	ball.dx *= -1;
  
        _UPDATE_FREQUENCY = UPDATE_FREQUENCY; // reset game speed
      }
    }
  }

  // y collision
  if(ball.y <= 1 || ball.y + BALL_SIZE >= ROWS-1) ball.dy *= -1;

  ball.y += ball.dy;
  ball.x += ball.dx;
}

void automate_player(unsigned int index){
  int diff = abs(player[index].x - ball.x);
  if(diff <= COLS/BOT_FOW){
    bool direction = ((ball.y*2+BALL_SIZE)/2 < (player[index].y*2+PADDLE_Y)/2) ? 
		      0 : 1;
    move_player(index, direction);
  }
}

// shamelessly stolen from https://github.com/mevdschee/2048.c/blob/main/2048.c 
void setBufferedInput(bool enable){
  static bool enabled = true;
  static struct termios old;
  struct termios new;

  if (enable && !enabled){
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    enabled = true;
  }
  else if (!enable && enabled){
    tcgetattr(STDIN_FILENO, &new);
    old = new;
    new.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
    setbuf(stdin, NULL);
    enabled = false;
  }
}

void end_game(int signum){
  setBufferedInput(true);
  printf("\033[?25h\033[m");
  exit(signum);
}

// https://www.flipcode.com/archives/_kbhit_for_Linux.shtml
int _kbhit() {
    static const int STDIN = 0;

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

int main(int argc, char *argv[]){
  // pass arguments
  if(argc > 1){
    for(int i = 1; i < argc; i++){
	if(strcmp(argv[i],"-pvp")==0)   pvp = true;
	if(strcmp(argv[i],"-eve")==0)   eve = true;
	if(strcmp(argv[i],"-debug") ==0 || strcmp(argv[i],"-d")==0) debug = true;
	if(strcmp(argv[i],"-w")==0)    {sscanf(argv[i+1], "%hhu", &COLS); ++i; }
	if(strcmp(argv[i],"-h")==0)    {sscanf(argv[i+1], "%hhu", &ROWS); ++i; }
	if(strcmp(argv[i],"-u")==0)    {sscanf(argv[i+1], "%hhu", &UPDATE_FREQUENCY); ++i; }
	if(strcmp(argv[i],"-fow")==0)    {sscanf(argv[i+1], "%hhu", &BOT_FOW); ++i; }
      }
    }


  uint8_t grid[ROWS][COLS];

  // init values
  player[0].x = 1;
  player[0].y = (ROWS/2) - (PADDLE_Y/2);
  player[0].score = 0;
  player[1].x = COLS-(PADDLE_X+1);
  player[1].y = (ROWS/2) - (PADDLE_Y/2);
  player[1].score = 0;

  ball.x = COLS/2 - BALL_SIZE;
  ball.y = ROWS/2 - BALL_SIZE;
  ball.dx = 2;
  ball.dy = 1;

  _UPDATE_FREQUENCY = UPDATE_FREQUENCY;

  // force quit handler
  signal(SIGINT, end_game);

  // setup terminal for  game
  printf("\033[?25l\033[2J");
  draw_game(grid);
  setBufferedInput(false);
  char m;
  
  // begin main loop
  while(true){
    if(_kbhit()){
      m = getchar();
      switch (m) {
      // first player (w-s)
	case 119:
	  move_player(0, 0);
	  break; 
	case 115:
	  move_player(0, 1);
	  break;

	// second player(up-down)
	case 65:
	  if(pvp) move_player(1, 0);
	  break;

	case 66:
	  if(pvp) move_player(1, 1);
	  break;
      }
    }

    if(updates % _UPDATE_FREQUENCY == 0){
      update_ball();
      if(!pvp || eve) automate_player(1);
      if(eve) automate_player(0);
    }
    draw_game(grid);
    updates++;

    usleep(1000); // minimum 1ms delay
  }
  end_game(0);
}
